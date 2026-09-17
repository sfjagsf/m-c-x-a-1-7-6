#include "UartDriver.h"

#include <string.h>

#include "fsl_edma.h"
#include "fsl_gpio.h"
#include "fsl_lpuart.h"
#include "fsl_lpuart_edma.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "../../SmartDMA_UART/app_smartdma_lpuart0.h"

#define UART_LINE_ERROR_FLAGS                                                               \
    (kLPUART_RxOverrunFlag | kLPUART_NoiseErrorFlag | kLPUART_FramingErrorFlag |            \
     kLPUART_ParityErrorFlag)

typedef enum
{
    kUartBackendDisabled,
    kUartBackendRs485SharedDma,
    kUartBackendRs232DualDma,
} uart_backend_t;

typedef enum
{
    kUartRxStopped,
    kUartRxReceiving,
    kUartRxFrameReady,
} uart_rx_state_t;

typedef enum
{
    kUartTxIdle,
    kUartTxDma,
    kUartTxDraining,
} uart_tx_state_t;

typedef struct
{
    uint8_t rx[UART_BUFFER_SIZE];
    uint8_t tx[UART_BUFFER_SIZE];
    volatile uart_rx_state_t rxState;
    volatile uart_tx_state_t txState;
    volatile size_t rxLength;
    volatile uint32_t errors;
    volatile uart_diagnostics_t diagnostics;
} uart_runtime_t;

typedef struct
{
    uart_backend_t backend;
    LPUART_Type *base;
    IRQn_Type uartIrq;
    uint32_t txDmaChannel;
    uint32_t rxDmaChannel;
    uint32_t txDmaRequest;
    uint32_t rxDmaRequest;
    IRQn_Type txDmaIrq;
    IRQn_Type rxDmaIrq;
    edma_handle_t *sharedDmaHandle;
    edma_handle_t *txDmaHandle;
    edma_handle_t *rxDmaHandle;
    lpuart_edma_handle_t *lpuartEdmaHandle;
    void (*setDirection)(bool transmit);
} uart_port_config_t;

static uart_runtime_t s_uartRuntime[UART_PORT_COUNT];

static void Uart0_SetDirection(bool transmit)
{
    /* U51 ties DE and /RE together: low receives, high transmits. */
    GPIO_PinWrite(BOARD_INITPINS_RS485_EN_GPIO, BOARD_INITPINS_RS485_EN_GPIO_PIN,
                  transmit ? 1U : 0U);
}

/*
 * Hardware ownership is centralized here. Add a generated port by filling one
 * entry; protocol-independent state, buffers and frame ownership need no copy.
 */
static const uart_port_config_t s_uartConfig[UART_PORT_COUNT] = {
    [kUartPort0] = {
        .backend = kUartBackendRs485SharedDma,
        .base = LPUART0,
        .uartIrq = LPUART0_IRQn,
        .txDmaChannel = DMA0_CH0_DMA_CHANNEL,
        .rxDmaChannel = DMA0_CH0_DMA_CHANNEL,
        .txDmaRequest = kDma0RequestLPUART0Tx,
        .rxDmaRequest = kDma0RequestLPUART0Rx,
        .txDmaIrq = DMA_CH0_IRQn,
        .rxDmaIrq = DMA_CH0_IRQn,
        .sharedDmaHandle = &DMA0_CH0_Handle,
        .setDirection = Uart0_SetDirection,
    },
    [kUartPort1] = {
        .backend = kUartBackendRs232DualDma,
        .base = LPUART1,
        .uartIrq = LPUART1_IRQn,
        .txDmaChannel = LPUART1_TX_DMA_CHANNEL,
        .rxDmaChannel = LPUART1_RX_DMA_CHANNEL,
        .txDmaRequest = LPUART1_TX_DMA_REQUEST,
        .rxDmaRequest = LPUART1_RX_DMA_REQUEST,
        .txDmaIrq = DMA_CH5_IRQn,
        .rxDmaIrq = DMA_CH6_IRQn,
        .txDmaHandle = &LPUART1_TX_Handle,
        .rxDmaHandle = &LPUART1_RX_Handle,
        .lpuartEdmaHandle = &LPUART1_LPUART_eDMA_Handle,
    },
    [kUartPort2] = {.backend = kUartBackendDisabled},
    [kUartPort3] = {.backend = kUartBackendDisabled},
};

static bool UartCore_IsValidPort(uart_port_id_t port)
{
    return ((uint32_t)port < UART_PORT_COUNT) &&
           (s_uartConfig[port].backend != kUartBackendDisabled);
}

static void UartCore_SaveStatus(uart_port_id_t port, status_t status)
{
    s_uartRuntime[port].diagnostics.lastDriverStatus = status;
}

static void UartCore_SaveDmaError(uart_port_id_t port, uint32_t channel)
{
    uart_runtime_t *runtime = &s_uartRuntime[port];

    if ((EDMA_GetChannelStatusFlags(DMA0, channel) & kEDMA_ErrorFlag) != 0U)
    {
        runtime->diagnostics.dmaChannelErrors = DMA0->CH[channel].CH_ES;
        runtime->diagnostics.dmaGlobalErrors = EDMA_GetErrorStatusFlags(DMA0);
        runtime->diagnostics.dmaErrorCount++;
    }
}

static void UartCore_ResetRuntime(uart_port_id_t port)
{
    uart_runtime_t *runtime = &s_uartRuntime[port];

    (void)memset(runtime, 0, sizeof(*runtime));
    runtime->rxState = kUartRxStopped;
    runtime->txState = kUartTxIdle;
    runtime->diagnostics.lastRxRemaining = UART_BUFFER_SIZE;
    runtime->diagnostics.lastDriverStatus = kStatus_Success;
}

static void UartCore_EnableUartInterrupts(const uart_port_config_t *config)
{
    LPUART_EnableInterrupts(config->base, kLPUART_IdleLineInterruptEnable |
                                             kLPUART_RxOverrunInterruptEnable |
                                             kLPUART_NoiseErrorInterruptEnable |
                                             kLPUART_FramingErrorInterruptEnable |
                                             kLPUART_ParityErrorInterruptEnable);
}

static void UartCore_StopSharedDma(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];

    UartCore_SaveDmaError(port, config->rxDmaChannel);
    LPUART_EnableTxDMA(config->base, false);
    LPUART_EnableRxDMA(config->base, false);
    EDMA_AbortTransfer(config->sharedDmaHandle);
    EDMA_ClearChannelStatusFlags(DMA0, config->rxDmaChannel,
                                 (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                     (uint32_t)kEDMA_InterruptFlag);
    EDMA_SetChannelMux(DMA0, config->rxDmaChannel, kDma0RequestDisabled);
}

static status_t UartCore_StartSharedReceive(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    edma_transfer_config_t transfer;
    status_t status;

    UartCore_StopSharedDma(port);
    config->setDirection(false);
    LPUART_ClearStatusFlags(config->base, kLPUART_IdleLineFlag);
    EDMA_PrepareTransferConfig(&transfer, (void *)LPUART_GetDataRegisterAddress(config->base),
                               1U << kEDMA_TransferSize1Bytes, 0, runtime->rx,
                               1U << kEDMA_TransferSize1Bytes, 1, 1U, UART_BUFFER_SIZE);
    status = EDMA_SubmitTransfer(config->sharedDmaHandle, &transfer);
    if (status != kStatus_Success)
    {
        runtime->rxState = kUartRxStopped;
        UartCore_SaveStatus(port, status);
        return status;
    }

    runtime->rxLength = 0U;
    runtime->rxState = kUartRxReceiving;
    EDMA_SetChannelMux(DMA0, config->rxDmaChannel, config->rxDmaRequest);
    EDMA_EnableChannelInterrupts(DMA0, config->rxDmaChannel, kEDMA_ErrorInterruptEnable);
    EDMA_StartTransfer(config->sharedDmaHandle);
    LPUART_EnableRxDMA(config->base, true);
    UartCore_SaveStatus(port, kStatus_Success);
    return kStatus_Success;
}

static status_t UartCore_StartDualReceive(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    lpuart_transfer_t transfer = {
        .rxData = runtime->rx,
        .dataSize = UART_BUFFER_SIZE,
    };
    status_t status;

    LPUART_ClearStatusFlags(config->base, kLPUART_IdleLineFlag);
    runtime->rxLength = 0U;
    runtime->rxState = kUartRxReceiving;
    status = LPUART_ReceiveEDMA(config->base, config->lpuartEdmaHandle, &transfer);
    if (status != kStatus_Success)
    {
        /* NXP sets rxState busy before TCD submission; abort resets it. */
        LPUART_TransferAbortReceiveEDMA(config->base, config->lpuartEdmaHandle);
        runtime->rxState = kUartRxStopped;
        UartCore_SaveDmaError(port, config->rxDmaChannel);
    }
    UartCore_SaveStatus(port, status);
    return status;
}

static status_t UartCore_StartReceiveInternal(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];

    if (config->backend == kUartBackendRs485SharedDma)
    {
        return UartCore_StartSharedReceive(port);
    }
    return UartCore_StartDualReceive(port);
}

static status_t UartCore_StartSharedTransmit(uart_port_id_t port, size_t size, bool replying)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    edma_transfer_config_t transfer;
    status_t status;

    UartCore_StopSharedDma(port);
    runtime->rxState = kUartRxStopped;
    config->setDirection(true);
    EDMA_PrepareTransferConfig(&transfer, runtime->tx, 1U << kEDMA_TransferSize1Bytes, 1,
                               (void *)LPUART_GetDataRegisterAddress(config->base),
                               1U << kEDMA_TransferSize1Bytes, 0, 1U, (uint32_t)size);
    status = EDMA_SubmitTransfer(config->sharedDmaHandle, &transfer);
    if (status != kStatus_Success)
    {
        config->setDirection(false);
        runtime->txState = kUartTxIdle;
        UartCore_SaveStatus(port, status);
        if (replying)
        {
            /* The completed RX frame is still valid and can be retried/released. */
            runtime->rxState = kUartRxFrameReady;
        }
        else
        {
            (void)UartCore_StartReceiveInternal(port);
        }
        UartCore_SaveStatus(port, status);
        return status;
    }

    runtime->txState = kUartTxDma;
    EDMA_SetChannelMux(DMA0, config->txDmaChannel, config->txDmaRequest);
    EDMA_EnableChannelInterrupts(DMA0, config->txDmaChannel, kEDMA_ErrorInterruptEnable);
    EDMA_StartTransfer(config->sharedDmaHandle);
    LPUART_EnableTxDMA(config->base, true);
    UartCore_SaveStatus(port, kStatus_Success);
    return kStatus_Success;
}

static status_t UartCore_StartDualTransmit(uart_port_id_t port, size_t size)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    lpuart_transfer_t transfer = {
        .txData = runtime->tx,
        .dataSize = size,
    };
    status_t status;

    runtime->txState = kUartTxDma;
    status = LPUART_SendEDMA(config->base, config->lpuartEdmaHandle, &transfer);
    if (status != kStatus_Success)
    {
        /* NXP sets txState busy before TCD submission; abort resets it. */
        LPUART_TransferAbortSendEDMA(config->base, config->lpuartEdmaHandle);
        runtime->txState = kUartTxIdle;
        UartCore_SaveDmaError(port, config->txDmaChannel);
    }
    UartCore_SaveStatus(port, status);
    return status;
}

static status_t UartCore_StartTransmit(uart_port_id_t port, const uint8_t *data, size_t size,
                                       bool replying)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    uint32_t irqMask;

    if ((data == NULL) || (size == 0U) || (size > UART_BUFFER_SIZE))
    {
        return kStatus_InvalidArgument;
    }
    /* Atomically reserve TX; DMA setup itself remains outside the critical section. */
    irqMask = DisableGlobalIRQ();
    if ((runtime->txState != kUartTxIdle) ||
        (!replying && (config->backend == kUartBackendRs485SharedDma) &&
         (runtime->rxState == kUartRxFrameReady)))
    {
        EnableGlobalIRQ(irqMask);
        return kStatus_Busy;
    }
    runtime->txState = kUartTxDma;
    if (config->backend == kUartBackendRs485SharedDma)
    {
        /* Reserving the half-duplex bus invalidates the active RX transaction. */
        runtime->rxState = kUartRxStopped;
    }
    EnableGlobalIRQ(irqMask);

    (void)memcpy(runtime->tx, data, size);
    if (config->backend == kUartBackendRs485SharedDma)
    {
        return UartCore_StartSharedTransmit(port, size, replying);
    }
    return UartCore_StartDualTransmit(port, size);
}

static void UartCore_FinishSharedReceive(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    const uint32_t remaining =
        EDMA_GetRemainingMajorLoopCount(DMA0, config->rxDmaChannel);

    runtime->diagnostics.lastRxRemaining = remaining;
    UartCore_StopSharedDma(port);
    runtime->rxLength = UART_BUFFER_SIZE - remaining;
    runtime->diagnostics.lastRxLength = (uint32_t)runtime->rxLength;
    runtime->rxState = (runtime->rxLength != 0U) ? kUartRxFrameReady : kUartRxStopped;
    if (runtime->rxState == kUartRxStopped)
    {
        (void)UartCore_StartReceiveInternal(port);
    }
}

static void UartCore_FinishDualReceive(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    uint32_t count = 0U;
    const status_t status =
        LPUART_TransferGetReceiveCountEDMA(config->base, config->lpuartEdmaHandle, &count);

    if (status != kStatus_Success)
    {
        UartCore_SaveStatus(port, status);
        return;
    }

    LPUART_TransferAbortReceiveEDMA(config->base, config->lpuartEdmaHandle);
    runtime->rxLength = (size_t)count;
    runtime->diagnostics.lastRxLength = count;
    runtime->diagnostics.lastRxRemaining = UART_BUFFER_SIZE - count;
    runtime->rxState = (count != 0U) ? kUartRxFrameReady : kUartRxStopped;
    if (runtime->rxState == kUartRxStopped)
    {
        (void)UartCore_StartReceiveInternal(port);
    }
}

static void UartCore_CompleteSharedTransmit(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];

    if (runtime->txState != kUartTxDraining)
    {
        return;
    }

    LPUART_DisableInterrupts(config->base, kLPUART_TransmissionCompleteInterruptEnable);
    config->setDirection(false);
    runtime->txState = kUartTxIdle;
    UartCore_SaveStatus(port, UartCore_StartReceiveInternal(port));
}

static bool UartCore_SaveAndClearLineErrors(uart_port_id_t port, uint32_t flags)
{
    const uint32_t errors = flags & UART_LINE_ERROR_FLAGS;
    uart_runtime_t *runtime = &s_uartRuntime[port];

    if (errors == 0U)
    {
        return false;
    }

    LPUART_ClearStatusFlags(s_uartConfig[port].base, errors);
    runtime->errors |= errors;
    runtime->diagnostics.uartErrors |= errors;
    runtime->diagnostics.uartErrorCount++;
    return true;
}

static void UartCore_RecoverReceiveError(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];

    if (config->backend == kUartBackendRs485SharedDma)
    {
        UartCore_StopSharedDma(port);
        config->setDirection(false);
    }
    else
    {
        LPUART_TransferAbortReceiveEDMA(config->base, config->lpuartEdmaHandle);
    }
    runtime->rxState = kUartRxStopped;
    runtime->rxLength = 0U;
    runtime->diagnostics.discardedFrameCount++;
    UartCore_SaveStatus(port, UartCore_StartReceiveInternal(port));
}

static void UartCore_HandleUartIrq(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    const uint32_t flags = LPUART_GetStatusFlags(config->base);
    const bool lineError = UartCore_SaveAndClearLineErrors(port, flags);

    if (lineError && (runtime->rxState == kUartRxReceiving))
    {
        UartCore_RecoverReceiveError(port);
    }
    else if (((flags & kLPUART_IdleLineFlag) != 0U) &&
             (runtime->rxState == kUartRxReceiving))
    {
        LPUART_ClearStatusFlags(config->base, kLPUART_IdleLineFlag);
        if (config->backend == kUartBackendRs485SharedDma)
        {
            UartCore_FinishSharedReceive(port);
        }
        else
        {
            UartCore_FinishDualReceive(port);
        }
    }
    else if ((flags & kLPUART_IdleLineFlag) != 0U)
    {
        LPUART_ClearStatusFlags(config->base, kLPUART_IdleLineFlag);
    }

    if (config->backend == kUartBackendRs485SharedDma)
    {
        if (((flags & kLPUART_TransmissionCompleteFlag) != 0U) &&
            (runtime->txState == kUartTxDraining))
        {
            UartCore_CompleteSharedTransmit(port);
        }
    }
    else if (((flags & kLPUART_TransmissionCompleteFlag) != 0U) &&
             ((config->base->CTRL & LPUART_CTRL_TCIE_MASK) != 0U))
    {
        /* Continue NXP's eDMA TX chain; this produces kStatus_LPUART_TxIdle. */
        LPUART_TransferEdmaHandleIRQ(config->base, config->lpuartEdmaHandle);
    }
}

static void UartCore_HandleSharedDmaDone(uart_port_id_t port, bool transferDone)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];

    if (!transferDone)
    {
        return;
    }

    if (runtime->txState == kUartTxDma)
    {
        LPUART_EnableTxDMA(config->base, false);
        EDMA_SetChannelMux(DMA0, config->txDmaChannel, kDma0RequestDisabled);
        runtime->txState = kUartTxDraining;
        if ((LPUART_GetStatusFlags(config->base) & kLPUART_TransmissionCompleteFlag) != 0U)
        {
            UartCore_CompleteSharedTransmit(port);
        }
        else
        {
            LPUART_EnableInterrupts(config->base,
                                    kLPUART_TransmissionCompleteInterruptEnable);
        }
    }
    else if (runtime->rxState == kUartRxReceiving)
    {
        LPUART_EnableRxDMA(config->base, false);
        EDMA_SetChannelMux(DMA0, config->rxDmaChannel, kDma0RequestDisabled);
        runtime->rxLength = UART_BUFFER_SIZE;
        runtime->rxState = kUartRxFrameReady;
        runtime->diagnostics.lastRxRemaining = 0U;
        runtime->diagnostics.lastRxLength = UART_BUFFER_SIZE;
    }
}

static void UartCore_HandleDualDmaCallback(uart_port_id_t port, status_t status)
{
    uart_runtime_t *runtime = &s_uartRuntime[port];

    UartCore_SaveStatus(port, status);
    if (status == kStatus_LPUART_TxIdle)
    {
        runtime->txState = kUartTxIdle;
    }
    else if (status == kStatus_LPUART_RxIdle)
    {
        runtime->rxLength = UART_BUFFER_SIZE;
        runtime->rxState = kUartRxFrameReady;
        runtime->diagnostics.lastRxRemaining = 0U;
        runtime->diagnostics.lastRxLength = UART_BUFFER_SIZE;
    }
}

static void UartCore_HandleSharedDmaIrq(uart_port_id_t port)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];

    if ((EDMA_GetChannelStatusFlags(DMA0, config->rxDmaChannel) & kEDMA_ErrorFlag) != 0U)
    {
        if (runtime->rxState == kUartRxReceiving)
        {
            runtime->diagnostics.discardedFrameCount++;
        }
        /* StopSharedDma records the channel error once before clearing it. */
        UartCore_StopSharedDma(port);
        LPUART_DisableInterrupts(config->base, kLPUART_TransmissionCompleteInterruptEnable);
        config->setDirection(false);
        runtime->rxState = kUartRxStopped;
        runtime->txState = kUartTxIdle;
        runtime->rxLength = 0U;
        UartCore_SaveStatus(port, UartCore_StartReceiveInternal(port));
    }
    else
    {
        EDMA_HandleIRQ(config->sharedDmaHandle);
    }
}

static void UartCore_HandleDualDmaIrq(uart_port_id_t port, bool isTx)
{
    const uart_port_config_t *config = &s_uartConfig[port];
    uart_runtime_t *runtime = &s_uartRuntime[port];
    const uint32_t channel = isTx ? config->txDmaChannel : config->rxDmaChannel;
    edma_handle_t *handle = isTx ? config->txDmaHandle : config->rxDmaHandle;

    if ((EDMA_GetChannelStatusFlags(DMA0, channel) & kEDMA_ErrorFlag) == 0U)
    {
        EDMA_HandleIRQ(handle);
        return;
    }

    UartCore_SaveDmaError(port, channel);
    if (isTx)
    {
        LPUART_TransferAbortSendEDMA(config->base, config->lpuartEdmaHandle);
        runtime->txState = kUartTxIdle;
    }
    else
    {
        LPUART_TransferAbortReceiveEDMA(config->base, config->lpuartEdmaHandle);
        runtime->rxState = kUartRxStopped;
        runtime->rxLength = 0U;
        runtime->diagnostics.discardedFrameCount++;
    }
    EDMA_ClearChannelStatusFlags(DMA0, channel,
                                 (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                     (uint32_t)kEDMA_InterruptFlag);
    UartCore_SaveStatus(port, kStatus_Fail);
    if (!isTx)
    {
        UartCore_SaveStatus(port, UartCore_StartReceiveInternal(port));
    }
}

void UartPort_Init(uart_port_id_t port)
{
    const uart_port_config_t *config;

    if (!UartCore_IsValidPort(port))
    {
        return;
    }
    if (port == kUartPort0)
    {
        APP_SmartDMALPUART0_TransportInit();
        return;
    }
    config = &s_uartConfig[port];

    if (config->backend == kUartBackendRs485SharedDma)
    {
        UartCore_StopSharedDma(port);
    }
    else
    {
        LPUART_TransferAbortSendEDMA(config->base, config->lpuartEdmaHandle);
        LPUART_TransferAbortReceiveEDMA(config->base, config->lpuartEdmaHandle);
    }
    LPUART_DisableInterrupts(config->base, kLPUART_AllInterruptEnable);
    LPUART_ClearStatusFlags(config->base, kLPUART_AllClearFlags);
    LPUART_SetRxFifoWatermark(config->base, 0U);
    if (config->setDirection != NULL)
    {
        config->setDirection(false);
    }
    UartCore_ResetRuntime(port);

    NVIC_ClearPendingIRQ(config->uartIrq);
    NVIC_SetPriority(config->uartIrq, 5U);
    EnableIRQ(config->uartIrq);
    if (config->txDmaIrq != config->uartIrq)
    {
        NVIC_SetPriority(config->txDmaIrq, 5U);
        EnableIRQ(config->txDmaIrq);
    }
    if (config->rxDmaIrq != config->txDmaIrq)
    {
        NVIC_SetPriority(config->rxDmaIrq, 5U);
        EnableIRQ(config->rxDmaIrq);
    }
    UartCore_EnableUartInterrupts(config);
    UartCore_SaveStatus(port, UartCore_StartReceiveInternal(port));
}

status_t UartPort_StartReceive(uart_port_id_t port)
{
    if (port == kUartPort0)
    {
        return APP_SmartDMALPUART0_StartReceive();
    }
    if (!UartCore_IsValidPort(port))
    {
        return kStatus_Fail;
    }
    if (s_uartRuntime[port].rxState != kUartRxStopped)
    {
        return kStatus_Busy;
    }
    return UartCore_StartReceiveInternal(port);
}

status_t UartPort_Send(uart_port_id_t port, const uint8_t *data, size_t size)
{
    if (port == kUartPort0)
    {
        return APP_SmartDMALPUART0_Send(data, size, false);
    }
    if (!UartCore_IsValidPort(port))
    {
        return kStatus_Fail;
    }
    return UartCore_StartTransmit(port, data, size, false);
}

status_t UartPort_Reply(uart_port_id_t port, const uint8_t *data, size_t size)
{
    status_t status;

    if (!UartCore_IsValidPort(port))
    {
        return kStatus_Fail;
    }
    if (port == kUartPort0)
    {
        if (!APP_SmartDMALPUART0_IsFrameAvailable())
        {
            return kStatus_NoTransferInProgress;
        }
        return APP_SmartDMALPUART0_Send(data, size, true);
    }
    if (s_uartRuntime[port].rxState != kUartRxFrameReady)
    {
        return kStatus_NoTransferInProgress;
    }

    status = UartCore_StartTransmit(port, data, size, true);
    if (status != kStatus_Success)
    {
        return status;
    }

    if (s_uartConfig[port].backend == kUartBackendRs485SharedDma)
    {
        /* TC completion restarts shared-channel RX. */
        s_uartRuntime[port].rxState = kUartRxStopped;
    }
    else
    {
        const status_t rxStatus = UartCore_StartReceiveInternal(port);
        if (rxStatus != kStatus_Success)
        {
            /* TX owns a copy, so retain the received frame if RX restart fails. */
            s_uartRuntime[port].rxState = kUartRxFrameReady;
        }
    }
    return status;
}

void UartPort_Abort(uart_port_id_t port)
{
    const uart_port_config_t *config;

    if (!UartCore_IsValidPort(port))
    {
        return;
    }
    if (port == kUartPort0)
    {
        APP_SmartDMALPUART0_Abort();
        return;
    }
    config = &s_uartConfig[port];
    if (config->backend == kUartBackendRs485SharedDma)
    {
        UartCore_StopSharedDma(port);
        LPUART_DisableInterrupts(config->base, kLPUART_TransmissionCompleteInterruptEnable);
        config->setDirection(false);
    }
    else
    {
        LPUART_TransferAbortSendEDMA(config->base, config->lpuartEdmaHandle);
        LPUART_TransferAbortReceiveEDMA(config->base, config->lpuartEdmaHandle);
    }
    s_uartRuntime[port].rxState = kUartRxStopped;
    s_uartRuntime[port].txState = kUartTxIdle;
    s_uartRuntime[port].rxLength = 0U;
}

bool UartPort_IsBusy(uart_port_id_t port)
{
    if (port == kUartPort0)
    {
        return APP_SmartDMALPUART0_IsBusy();
    }
    return UartCore_IsValidPort(port) && (s_uartRuntime[port].txState != kUartTxIdle);
}

bool UartPort_IsFrameAvailable(uart_port_id_t port)
{
    if (port == kUartPort0)
    {
        return APP_SmartDMALPUART0_IsFrameAvailable();
    }
    return UartCore_IsValidPort(port) &&
           (s_uartRuntime[port].rxState == kUartRxFrameReady);
}

const uint8_t *UartPort_GetFrame(uart_port_id_t port, size_t *length)
{
    if (port == kUartPort0)
    {
        return APP_SmartDMALPUART0_GetFrame(length);
    }
    if (length != NULL)
    {
        *length = UartPort_IsFrameAvailable(port) ? s_uartRuntime[port].rxLength : 0U;
    }
    return UartPort_IsFrameAvailable(port) ? s_uartRuntime[port].rx : NULL;
}

status_t UartPort_ReleaseFrame(uart_port_id_t port)
{
    status_t status;

    if (!UartCore_IsValidPort(port))
    {
        return kStatus_Fail;
    }
    if (port == kUartPort0)
    {
        return APP_SmartDMALPUART0_ReleaseFrame();
    }
    if (s_uartRuntime[port].rxState != kUartRxFrameReady)
    {
        return kStatus_NoTransferInProgress;
    }

    s_uartRuntime[port].rxState = kUartRxStopped;
    status = UartCore_StartReceiveInternal(port);
    if (status != kStatus_Success)
    {
        s_uartRuntime[port].rxState = kUartRxFrameReady;
    }
    return status;
}

uint32_t UartPort_GetAndClearErrors(uart_port_id_t port)
{
    uint32_t errors;
    uint32_t irqMask;

    if (!UartCore_IsValidPort(port))
    {
        return 0U;
    }
    if (port == kUartPort0)
    {
        return APP_SmartDMALPUART0_GetAndClearErrors();
    }
    irqMask = DisableGlobalIRQ();
    errors = s_uartRuntime[port].errors;
    s_uartRuntime[port].errors = 0U;
    EnableGlobalIRQ(irqMask);
    return errors;
}

void UartPort_GetDiagnostics(uart_port_id_t port, uart_diagnostics_t *diagnostics)
{
    uint32_t irqMask;
    const volatile uart_diagnostics_t *source;

    if (!UartCore_IsValidPort(port) || (diagnostics == NULL))
    {
        return;
    }
    irqMask = DisableGlobalIRQ();
    source = &s_uartRuntime[port].diagnostics;
    diagnostics->uartErrors = source->uartErrors;
    diagnostics->uartErrorCount = source->uartErrorCount;
    diagnostics->dmaChannelErrors = source->dmaChannelErrors;
    diagnostics->dmaGlobalErrors = source->dmaGlobalErrors;
    diagnostics->dmaErrorCount = source->dmaErrorCount;
    diagnostics->discardedFrameCount = source->discardedFrameCount;
    diagnostics->lastRxRemaining = source->lastRxRemaining;
    diagnostics->lastRxLength = source->lastRxLength;
    diagnostics->lastDriverStatus = source->lastDriverStatus;
    EnableGlobalIRQ(irqMask);
}

void UartPort_ClearDiagnostics(uart_port_id_t port)
{
    uint32_t irqMask;

    if (!UartCore_IsValidPort(port))
    {
        return;
    }
    irqMask = DisableGlobalIRQ();
    (void)memset((void *)&s_uartRuntime[port].diagnostics, 0,
                 sizeof(s_uartRuntime[port].diagnostics));
    s_uartRuntime[port].diagnostics.lastRxRemaining = UART_BUFFER_SIZE;
    s_uartRuntime[port].diagnostics.lastDriverStatus = kStatus_Success;
    EnableGlobalIRQ(irqMask);
}

/* Port-specific API remains thin and contains no hardware policy. */
#define UART_DEFINE_PORT_API(n)                                                              \
    void Uart##n##_Init(void) { UartPort_Init(kUartPort##n); }                               \
    status_t Uart##n##_StartReceive(void) { return UartPort_StartReceive(kUartPort##n); }    \
    status_t Uart##n##_Send(const uint8_t *data, size_t size)                               \
    { return UartPort_Send(kUartPort##n, data, size); }                                      \
    status_t Uart##n##_Reply(const uint8_t *data, size_t size)                              \
    { return UartPort_Reply(kUartPort##n, data, size); }                                     \
    void Uart##n##_Abort(void) { UartPort_Abort(kUartPort##n); }                             \
    bool Uart##n##_IsBusy(void) { return UartPort_IsBusy(kUartPort##n); }                    \
    bool Uart##n##_IsFrameAvailable(void) { return UartPort_IsFrameAvailable(kUartPort##n); } \
    const uint8_t *Uart##n##_GetFrame(size_t *length)                                       \
    { return UartPort_GetFrame(kUartPort##n, length); }                                      \
    status_t Uart##n##_ReleaseFrame(void) { return UartPort_ReleaseFrame(kUartPort##n); }    \
    uint32_t Uart##n##_GetAndClearErrors(void)                                               \
    { return UartPort_GetAndClearErrors(kUartPort##n); }                                     \
    void Uart##n##_GetDiagnostics(uart_diagnostics_t *diagnostics)                          \
    { UartPort_GetDiagnostics(kUartPort##n, diagnostics); }                                 \
    void Uart##n##_ClearDiagnostics(void) { UartPort_ClearDiagnostics(kUartPort##n); }

UART_DEFINE_PORT_API(0)
UART_DEFINE_PORT_API(1)
UART_DEFINE_PORT_API(2)
UART_DEFINE_PORT_API(3)

#undef UART_DEFINE_PORT_API

/* Generated callback entry points are adapters into the common core. */
void LPUART0_DMACallback(edma_handle_t *handle, void *userData, bool transferDone, uint32_t tcds)
{
    (void)handle;
    (void)userData;
    (void)tcds;
    UartCore_HandleSharedDmaDone(kUartPort0, transferDone);
}

void LPUART1_DMACallback(LPUART_Type *base, lpuart_edma_handle_t *handle, status_t status,
                         void *userData)
{
    (void)base;
    (void)handle;
    (void)userData;
    UartCore_HandleDualDmaCallback(kUartPort1, status);
}

void LPUART0_IRQHandler(void)
{
    APP_SmartDMALPUART0_HandleLpuartIrq();
    SDK_ISR_EXIT_BARRIER;
}

void LPUART1_IRQHandler(void)
{
    UartCore_HandleUartIrq(kUartPort1);
    SDK_ISR_EXIT_BARRIER;
}

void DMA_CH0_IRQHandler(void)
{
    UartCore_HandleSharedDmaIrq(kUartPort0);
    SDK_ISR_EXIT_BARRIER;
}

void DMA_CH5_IRQHandler(void)
{
    UartCore_HandleDualDmaIrq(kUartPort1, true);
    SDK_ISR_EXIT_BARRIER;
}

void DMA_CH6_IRQHandler(void)
{
    UartCore_HandleDualDmaIrq(kUartPort1, false);
    SDK_ISR_EXIT_BARRIER;
}
