#include "UartDriver.h"

#include <string.h>

#include "fsl_edma.h"
#include "fsl_gpio.h"
#include "fsl_lpuart.h"
#include "fsl_lpuart_edma.h"
#include "peripherals.h"
#include "pin_mux.h"

/*
 * Only UART line errors belong in the application error event. FIFO status is
 * intentionally excluded: after LPUART_GetStatusFlags() combines STAT and
 * FIFO, FIFO underflow/overflow is a controller diagnostic, not a malformed
 * RS485 frame, and must not make every received frame look erroneous.
 */
#define UART_LINE_ERROR_FLAGS                                                                \
    (kLPUART_RxOverrunFlag | kLPUART_NoiseErrorFlag | kLPUART_FramingErrorFlag |            \
     kLPUART_ParityErrorFlag)

typedef enum
{
    kUart0Idle,
    kUart0Receiving,
    kUart0TxDma,
    kUart0TxDraining,
} uart0_state_t;

typedef struct
{
    uint8_t rx[UART_BUFFER_SIZE];
    uint8_t tx[UART_BUFFER_SIZE];
} uart_port_storage_t;

/* Each logical UART owns permanently separate receive and transmit storage. */
static uart_port_storage_t s_uartPortStorage[UART_PORT_COUNT];

typedef struct
{
    volatile bool rxActive;
    volatile bool txBusy;
    volatile bool frameAvailable;
    volatile size_t rxLength;
    volatile uint32_t errors;
    volatile uart_diagnostics_t diagnostics;
} uart_full_duplex_runtime_t;

#define s_uart0RxBuffer (s_uartPortStorage[kUartPort0].rx)
#define s_uart0TxBuffer (s_uartPortStorage[kUartPort0].tx)
static volatile uart0_state_t s_uart0State;
static volatile size_t s_uart0RxLength;
static volatile uint32_t s_uart0Errors;
static volatile bool s_uart0FrameAvailable;
static volatile uart0_diagnostics_t s_uart0Diagnostics;

#define s_uart1RxBuffer (s_uartPortStorage[kUartPort1].rx)
#define s_uart1TxBuffer (s_uartPortStorage[kUartPort1].tx)
static uart_full_duplex_runtime_t s_uart1Runtime;

static status_t Uart0_StartReceiveInternal(void);
static status_t Uart0_StartTransmitInternal(const uint8_t *data, size_t size);

static void Uart0_SaveDriverStatus(status_t status)
{
    s_uart0Diagnostics.lastDriverStatus = status;
}

static void Uart0_SaveDmaError(void)
{
    if ((EDMA_GetChannelStatusFlags(DMA0, DMA0_CH0_DMA_CHANNEL) & kEDMA_ErrorFlag) != 0U)
    {
        /* Save CH_ES before clearing ERR; it contains the actual alignment/bus error cause. */
        s_uart0Diagnostics.dmaChannelErrors = DMA0->CH[DMA0_CH0_DMA_CHANNEL].CH_ES;
        s_uart0Diagnostics.dmaGlobalErrors  = EDMA_GetErrorStatusFlags(DMA0);
        s_uart0Diagnostics.dmaErrorCount++;
    }
}

static uint32_t Uart0_TakeErrors(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();
    const uint32_t errors = s_uart0Errors;

    s_uart0Errors = 0U;
    EnableGlobalIRQ(irqMask);
    return errors;
}

/*
 * U51 (SP3485) ties DE and /RE to P0_19 (RS485_EN):
 *   P0_19 = 0: DE=0 and /RE=0, transmitter disabled and receiver enabled.
 *   P0_19 = 1: DE=1 and /RE=1, transmitter enabled and receiver disabled.
 * Keep the two modes explicit; a generic "enable" boolean is easy to invert.
 */
static void Uart0_EnterReceiveMode(void)
{
    GPIO_PinWrite(BOARD_INITPINS_RS485_EN_GPIO, BOARD_INITPINS_RS485_EN_GPIO_PIN, 0U);
}

static void Uart0_EnterTransmitMode(void)
{
    GPIO_PinWrite(BOARD_INITPINS_RS485_EN_GPIO, BOARD_INITPINS_RS485_EN_GPIO_PIN, 1U);
}

static void Uart0_StopDma(void)
{
    Uart0_SaveDmaError();
    LPUART_EnableTxDMA(LPUART0, false);
    LPUART_EnableRxDMA(LPUART0, false);
    EDMA_AbortTransfer(&DMA0_CH0_Handle);
    EDMA_ClearChannelStatusFlags(DMA0, DMA0_CH0_DMA_CHANNEL,
                                 (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                     (uint32_t)kEDMA_InterruptFlag);
    EDMA_SetChannelMux(DMA0, DMA0_CH0_DMA_CHANNEL, kDma0RequestDisabled);
}

/* Returns true when the active RX frame must not be passed to the application. */
static bool Uart0_SaveAndClearErrors(uint32_t status)
{
    const uint32_t errors = status & UART_LINE_ERROR_FLAGS;

    if (errors != 0U)
    {
        LPUART_ClearStatusFlags(LPUART0, errors);
        s_uart0Errors |= errors;
        s_uart0Diagnostics.uartErrors |= errors;
        s_uart0Diagnostics.uartErrorCount++;
        return true;
    }

    return false;
}

/*
 * Leave RS485 transmit mode only after LPUART reports TC.  TC means the final
 * stop bit is on the wire, unlike DMA completion which only means DATA was
 * written to the UART FIFO/register.
 */
static void Uart0_CompleteTransmit(void)
{
    if (s_uart0State != kUart0TxDraining)
    {
        return;
    }

    LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
    Uart0_EnterReceiveMode();
    s_uart0State = kUart0Idle;
    Uart0_SaveDriverStatus(Uart0_StartReceiveInternal());
}

static status_t Uart0_StartReceiveInternal(void)
{
    edma_transfer_config_t transfer;
    status_t status;

    Uart0_StopDma();
    Uart0_EnterReceiveMode();
    LPUART_ClearStatusFlags(LPUART0, kLPUART_IdleLineFlag);

    EDMA_PrepareTransferConfig(&transfer, (void *)LPUART_GetDataRegisterAddress(LPUART0),
                               1U << kEDMA_TransferSize1Bytes, 0, s_uart0RxBuffer,
                               1U << kEDMA_TransferSize1Bytes, 1, 1U, UART0_RX_BUFFER_SIZE);
    status = EDMA_SubmitTransfer(&DMA0_CH0_Handle, &transfer);
    if (status != kStatus_Success)
    {
        Uart0_SaveDriverStatus(status);
        return status;
    }

    s_uart0RxLength = 0U;
    s_uart0State    = kUart0Receiving;
    /* CH_MUX must be selected before StartTransfer: mux=0 starts a software request. */
    EDMA_SetChannelMux(DMA0, DMA0_CH0_DMA_CHANNEL, kDma0RequestLPUART0Rx);
    EDMA_EnableChannelInterrupts(DMA0, DMA0_CH0_DMA_CHANNEL, kEDMA_ErrorInterruptEnable);
    EDMA_StartTransfer(&DMA0_CH0_Handle);
    LPUART_EnableRxDMA(LPUART0, true);
    Uart0_SaveDriverStatus(kStatus_Success);
    return kStatus_Success;
}

static void Uart0_FinishReceive(void)
{
    const uint32_t remaining = EDMA_GetRemainingMajorLoopCount(DMA0, DMA0_CH0_DMA_CHANNEL);

    s_uart0Diagnostics.lastRxRemaining = remaining;
    Uart0_StopDma();
    s_uart0State = kUart0Idle;
    s_uart0RxLength = UART0_RX_BUFFER_SIZE - remaining;
    s_uart0Diagnostics.lastRxLength = (uint32_t)s_uart0RxLength;
    if (s_uart0RxLength != 0U)
    {
        s_uart0FrameAvailable = true;
    }
}

void Uart0_Init(void)
{
    Uart0_StopDma();
    LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable |
                                         kLPUART_IdleLineInterruptEnable |
                                         kLPUART_RxOverrunInterruptEnable |
                                         kLPUART_NoiseErrorInterruptEnable |
                                         kLPUART_FramingErrorInterruptEnable |
                                         kLPUART_ParityErrorInterruptEnable);
    LPUART_ClearStatusFlags(LPUART0, kLPUART_AllClearFlags);
    /* One received byte must generate one DMA request for IDLE-delimited frames. */
    LPUART_SetRxFifoWatermark(LPUART0, 0U);
    Uart0_EnterReceiveMode();

    s_uart0State          = kUart0Idle;
    s_uart0RxLength       = 0U;
    s_uart0Errors         = 0U;
    s_uart0FrameAvailable = false;
    Uart0_ClearDiagnostics();

    NVIC_ClearPendingIRQ(LPUART0_IRQn);
    NVIC_SetPriority(LPUART0_IRQn, 5U);
    EnableIRQ(LPUART0_IRQn);
    LPUART_EnableInterrupts(LPUART0, kLPUART_IdleLineInterruptEnable |
                                         kLPUART_RxOverrunInterruptEnable |
                                         kLPUART_NoiseErrorInterruptEnable |
                                         kLPUART_FramingErrorInterruptEnable |
                                         kLPUART_ParityErrorInterruptEnable);
    Uart0_SaveDriverStatus(Uart0_StartReceiveInternal());
}

status_t Uart0_StartReceive(void)
{
    if (s_uart0FrameAvailable || (s_uart0State != kUart0Idle))
    {
        return kStatus_Busy;
    }

    return Uart0_StartReceiveInternal();
}

static status_t Uart0_StartTransmitInternal(const uint8_t *data, size_t size)
{
    edma_transfer_config_t transfer;
    status_t status;

    if ((data == NULL) || (size == 0U) || (size > UART0_TX_BUFFER_SIZE))
    {
        return kStatus_InvalidArgument;
    }
    if ((s_uart0State == kUart0TxDma) || (s_uart0State == kUart0TxDraining))
    {
        return kStatus_Busy;
    }

    (void)memcpy(s_uart0TxBuffer, data, size);
    Uart0_StopDma();
    /* Enable the line driver before the first DMA byte reaches LPUART0. */
    Uart0_EnterTransmitMode();
    EDMA_PrepareTransferConfig(&transfer, s_uart0TxBuffer, 1U << kEDMA_TransferSize1Bytes, 1,
                               (void *)LPUART_GetDataRegisterAddress(LPUART0),
                               1U << kEDMA_TransferSize1Bytes, 0, 1U, (uint32_t)size);
    status = EDMA_SubmitTransfer(&DMA0_CH0_Handle, &transfer);
    if (status != kStatus_Success)
    {
        Uart0_EnterReceiveMode();
        Uart0_SaveDriverStatus(status);
        if (Uart0_StartReceiveInternal() != kStatus_Success)
        {
            /* The original TX setup error remains the most useful reported status. */
            Uart0_SaveDriverStatus(status);
        }
        return status;
    }

    s_uart0State = kUart0TxDma;
    /* Select the peripheral request before enabling this shared DMA channel. */
    EDMA_SetChannelMux(DMA0, DMA0_CH0_DMA_CHANNEL, kDma0RequestLPUART0Tx);
    EDMA_EnableChannelInterrupts(DMA0, DMA0_CH0_DMA_CHANNEL, kEDMA_ErrorInterruptEnable);
    EDMA_StartTransfer(&DMA0_CH0_Handle);
    LPUART_EnableTxDMA(LPUART0, true);
    Uart0_SaveDriverStatus(kStatus_Success);
    return kStatus_Success;
}

status_t Uart0_Send(const uint8_t *data, size_t size)
{
    if (s_uart0FrameAvailable)
    {
        return kStatus_Busy;
    }

    return Uart0_StartTransmitInternal(data, size);
}

status_t Uart0_Reply(const uint8_t *data, size_t size)
{
    if (!s_uart0FrameAvailable)
    {
        return kStatus_NoTransferInProgress;
    }
    if ((data == NULL) || (size == 0U) || (size > UART0_TX_BUFFER_SIZE))
    {
        return kStatus_InvalidArgument;
    }

    /* Uart0_FinishReceive() already stopped RX DMA and left the port idle. */
    s_uart0FrameAvailable = false;
    return Uart0_StartTransmitInternal(data, size);
}

void Uart0_Abort(void)
{
    Uart0_StopDma();
    LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
    Uart0_EnterReceiveMode();
    s_uart0State          = kUart0Idle;
    s_uart0RxLength       = 0U;
    s_uart0FrameAvailable = false;
}

bool Uart0_IsBusy(void)
{
    return (s_uart0State == kUart0TxDma) || (s_uart0State == kUart0TxDraining);
}

bool Uart0_IsFrameAvailable(void)
{
    return s_uart0FrameAvailable;
}

const uint8_t *Uart0_GetFrame(size_t *length)
{
    if (length != NULL)
    {
        *length = s_uart0FrameAvailable ? s_uart0RxLength : 0U;
    }
    return s_uart0FrameAvailable ? s_uart0RxBuffer : NULL;
}

status_t Uart0_ReleaseFrame(void)
{
    if (!s_uart0FrameAvailable)
    {
        return kStatus_NoTransferInProgress;
    }

    status_t status = Uart0_StartReceiveInternal();
    if (status == kStatus_Success)
    {
        s_uart0FrameAvailable = false;
    }
    return status;
}

uint32_t Uart0_GetAndClearErrors(void)
{
    return Uart0_TakeErrors();
}

void Uart0_GetDiagnostics(uart0_diagnostics_t *diagnostics)
{
    uint32_t irqMask;

    if (diagnostics == NULL)
    {
        return;
    }

    irqMask = DisableGlobalIRQ();
    diagnostics->uartErrors       = s_uart0Diagnostics.uartErrors;
    diagnostics->uartErrorCount   = s_uart0Diagnostics.uartErrorCount;
    diagnostics->dmaChannelErrors = s_uart0Diagnostics.dmaChannelErrors;
    diagnostics->dmaGlobalErrors  = s_uart0Diagnostics.dmaGlobalErrors;
    diagnostics->dmaErrorCount    = s_uart0Diagnostics.dmaErrorCount;
    diagnostics->discardedFrameCount = s_uart0Diagnostics.discardedFrameCount;
    diagnostics->lastRxRemaining  = s_uart0Diagnostics.lastRxRemaining;
    diagnostics->lastRxLength     = s_uart0Diagnostics.lastRxLength;
    diagnostics->lastDriverStatus = s_uart0Diagnostics.lastDriverStatus;
    EnableGlobalIRQ(irqMask);
}

void Uart0_ClearDiagnostics(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    s_uart0Diagnostics.uartErrors       = 0U;
    s_uart0Diagnostics.uartErrorCount   = 0U;
    s_uart0Diagnostics.dmaChannelErrors = 0U;
    s_uart0Diagnostics.dmaGlobalErrors  = 0U;
    s_uart0Diagnostics.dmaErrorCount    = 0U;
    s_uart0Diagnostics.discardedFrameCount = 0U;
    s_uart0Diagnostics.lastRxRemaining  = UART0_RX_BUFFER_SIZE;
    s_uart0Diagnostics.lastRxLength     = 0U;
    s_uart0Diagnostics.lastDriverStatus = kStatus_Success;
    EnableGlobalIRQ(irqMask);
}

/*
 * Full-duplex LPUART/eDMA backend used by RS232 ports. TX and RX have separate
 * DMA channels, so receiving remains armed while a transmission is running.
 */
static void Uart1_SaveDriverStatus(status_t status)
{
    s_uart1Runtime.diagnostics.lastDriverStatus = status;
}

static void Uart1_SaveDmaError(uint32_t channel)
{
    if ((EDMA_GetChannelStatusFlags(DMA0, channel) & kEDMA_ErrorFlag) != 0U)
    {
        s_uart1Runtime.diagnostics.dmaChannelErrors = DMA0->CH[channel].CH_ES;
        s_uart1Runtime.diagnostics.dmaGlobalErrors  = EDMA_GetErrorStatusFlags(DMA0);
        s_uart1Runtime.diagnostics.dmaErrorCount++;
    }
}

static status_t Uart1_StartReceiveInternal(void)
{
    lpuart_transfer_t transfer = {
        .rxData   = s_uart1RxBuffer,
        .dataSize = UART_BUFFER_SIZE,
    };
    status_t status;

    LPUART_ClearStatusFlags(LPUART1, kLPUART_IdleLineFlag);
    s_uart1Runtime.rxLength = 0U;
    s_uart1Runtime.rxActive = true;
    status = LPUART_ReceiveEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle, &transfer);
    if (status != kStatus_Success)
    {
        /* NXP sets rxState busy before submitting the TCD; undo it on failure. */
        LPUART_TransferAbortReceiveEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
        s_uart1Runtime.rxActive = false;
        Uart1_SaveDmaError(LPUART1_RX_DMA_CHANNEL);
    }
    Uart1_SaveDriverStatus(status);
    return status;
}

static void Uart1_FinishReceive(void)
{
    uint32_t count = 0U;
    const status_t status =
        LPUART_TransferGetReceiveCountEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle, &count);

    if (status == kStatus_Success)
    {
        LPUART_TransferAbortReceiveEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
        s_uart1Runtime.rxActive = false;
        s_uart1Runtime.rxLength = (size_t)count;
        s_uart1Runtime.diagnostics.lastRxLength = count;
        s_uart1Runtime.diagnostics.lastRxRemaining = UART_BUFFER_SIZE - count;
        if (count != 0U)
        {
            s_uart1Runtime.frameAvailable = true;
        }
        else
        {
            (void)Uart1_StartReceiveInternal();
        }
    }
    else
    {
        Uart1_SaveDriverStatus(status);
    }
}

static bool Uart1_SaveAndClearErrors(uint32_t status)
{
    const uint32_t errors = status & UART_LINE_ERROR_FLAGS;

    if (errors == 0U)
    {
        return false;
    }

    LPUART_ClearStatusFlags(LPUART1, errors);
    s_uart1Runtime.errors |= errors;
    s_uart1Runtime.diagnostics.uartErrors |= errors;
    s_uart1Runtime.diagnostics.uartErrorCount++;
    return true;
}

void Uart1_Init(void)
{
    LPUART_TransferAbortSendEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
    LPUART_TransferAbortReceiveEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
    LPUART_DisableInterrupts(LPUART1, kLPUART_AllInterruptEnable);
    LPUART_ClearStatusFlags(LPUART1, kLPUART_AllClearFlags);
    /* One byte in RX FIFO must be sufficient to issue one CH6 DMA request. */
    LPUART_SetRxFifoWatermark(LPUART1, 0U);

    (void)memset((void *)&s_uart1Runtime, 0, sizeof(s_uart1Runtime));
    s_uart1Runtime.diagnostics.lastRxRemaining = UART_BUFFER_SIZE;
    s_uart1Runtime.diagnostics.lastDriverStatus = kStatus_Success;

    NVIC_ClearPendingIRQ(LPUART1_IRQn);
    NVIC_SetPriority(LPUART1_IRQn, 5U);
    NVIC_SetPriority(DMA_CH5_IRQn, 5U);
    NVIC_SetPriority(DMA_CH6_IRQn, 5U);
    EnableIRQ(LPUART1_IRQn);
    EnableIRQ(DMA_CH5_IRQn);
    EnableIRQ(DMA_CH6_IRQn);
    LPUART_EnableInterrupts(LPUART1, kLPUART_IdleLineInterruptEnable |
                                        kLPUART_RxOverrunInterruptEnable |
                                        kLPUART_NoiseErrorInterruptEnable |
                                        kLPUART_FramingErrorInterruptEnable |
                                        kLPUART_ParityErrorInterruptEnable);
    Uart1_SaveDriverStatus(Uart1_StartReceiveInternal());
}

status_t Uart1_StartReceive(void)
{
    if (s_uart1Runtime.frameAvailable || s_uart1Runtime.rxActive)
    {
        return kStatus_Busy;
    }
    return Uart1_StartReceiveInternal();
}

status_t Uart1_Send(const uint8_t *data, size_t size)
{
    lpuart_transfer_t transfer;
    status_t status;

    if ((data == NULL) || (size == 0U) || (size > UART_BUFFER_SIZE))
    {
        return kStatus_InvalidArgument;
    }
    if (s_uart1Runtime.txBusy)
    {
        return kStatus_Busy;
    }

    (void)memcpy(s_uart1TxBuffer, data, size);
    transfer.txData   = s_uart1TxBuffer;
    transfer.dataSize = size;
    s_uart1Runtime.txBusy = true;
    status = LPUART_SendEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle, &transfer);
    if (status != kStatus_Success)
    {
        /* NXP sets txState busy before submitting the TCD; undo it on failure. */
        LPUART_TransferAbortSendEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
        s_uart1Runtime.txBusy = false;
        Uart1_SaveDmaError(LPUART1_TX_DMA_CHANNEL);
    }
    Uart1_SaveDriverStatus(status);
    return status;
}

status_t Uart1_Reply(const uint8_t *data, size_t size)
{
    status_t status;

    if (!s_uart1Runtime.frameAvailable)
    {
        return kStatus_NoTransferInProgress;
    }

    /* Uart1_Send copies the frame before CH6 is restarted and overwrites RX. */
    status = Uart1_Send(data, size);
    if (status == kStatus_Success)
    {
        const status_t rxStatus = Uart1_StartReceiveInternal();
        if (rxStatus == kStatus_Success)
        {
            s_uart1Runtime.frameAvailable = false;
        }
    }
    return status;
}

void Uart1_Abort(void)
{
    LPUART_TransferAbortSendEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
    LPUART_TransferAbortReceiveEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
    s_uart1Runtime.txBusy = false;
    s_uart1Runtime.rxActive = false;
    s_uart1Runtime.frameAvailable = false;
    s_uart1Runtime.rxLength = 0U;
}

bool Uart1_IsBusy(void)
{
    return s_uart1Runtime.txBusy;
}

bool Uart1_IsFrameAvailable(void)
{
    return s_uart1Runtime.frameAvailable;
}

const uint8_t *Uart1_GetFrame(size_t *length)
{
    if (length != NULL)
    {
        *length = s_uart1Runtime.frameAvailable ? s_uart1Runtime.rxLength : 0U;
    }
    return s_uart1Runtime.frameAvailable ? s_uart1RxBuffer : NULL;
}

status_t Uart1_ReleaseFrame(void)
{
    status_t status;

    if (!s_uart1Runtime.frameAvailable)
    {
        return kStatus_NoTransferInProgress;
    }

    status = Uart1_StartReceiveInternal();
    if (status == kStatus_Success)
    {
        s_uart1Runtime.frameAvailable = false;
    }
    return status;
}

uint32_t Uart1_GetAndClearErrors(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();
    const uint32_t errors = s_uart1Runtime.errors;

    s_uart1Runtime.errors = 0U;
    EnableGlobalIRQ(irqMask);
    return errors;
}

void Uart1_GetDiagnostics(uart_diagnostics_t *diagnostics)
{
    uint32_t irqMask;

    if (diagnostics == NULL)
    {
        return;
    }

    irqMask = DisableGlobalIRQ();
    diagnostics->uartErrors = s_uart1Runtime.diagnostics.uartErrors;
    diagnostics->uartErrorCount = s_uart1Runtime.diagnostics.uartErrorCount;
    diagnostics->dmaChannelErrors = s_uart1Runtime.diagnostics.dmaChannelErrors;
    diagnostics->dmaGlobalErrors = s_uart1Runtime.diagnostics.dmaGlobalErrors;
    diagnostics->dmaErrorCount = s_uart1Runtime.diagnostics.dmaErrorCount;
    diagnostics->discardedFrameCount = s_uart1Runtime.diagnostics.discardedFrameCount;
    diagnostics->lastRxRemaining = s_uart1Runtime.diagnostics.lastRxRemaining;
    diagnostics->lastRxLength = s_uart1Runtime.diagnostics.lastRxLength;
    diagnostics->lastDriverStatus = s_uart1Runtime.diagnostics.lastDriverStatus;
    EnableGlobalIRQ(irqMask);
}

void Uart1_ClearDiagnostics(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    (void)memset((void *)&s_uart1Runtime.diagnostics, 0, sizeof(s_uart1Runtime.diagnostics));
    s_uart1Runtime.diagnostics.lastRxRemaining = UART_BUFFER_SIZE;
    s_uart1Runtime.diagnostics.lastDriverStatus = kStatus_Success;
    EnableGlobalIRQ(irqMask);
}

/* This name is selected in MCUXpresso Config Tools for DMA0 CH0. */
void LPUART0_DMACallback(edma_handle_t *handle, void *userData, bool transferDone, uint32_t tcds)
{
    (void)handle;
    (void)userData;
    (void)tcds;

    if (!transferDone)
    {
        return;
    }

    if (s_uart0State == kUart0TxDma)
    {
        LPUART_EnableTxDMA(LPUART0, false);
        EDMA_SetChannelMux(DMA0, DMA0_CH0_DMA_CHANNEL, kDma0RequestDisabled);
        s_uart0State = kUart0TxDraining;

        /* Avoid missing TC when the final byte completed before TCIE was set. */
        if ((LPUART_GetStatusFlags(LPUART0) & kLPUART_TransmissionCompleteFlag) != 0U)
        {
            Uart0_CompleteTransmit();
        }
        else
        {
            LPUART_EnableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
        }
    }
    else if (s_uart0State == kUart0Receiving)
    {
        /* The RX buffer is full; treat it as one frame. */
        LPUART_EnableRxDMA(LPUART0, false);
        EDMA_SetChannelMux(DMA0, DMA0_CH0_DMA_CHANNEL, kDma0RequestDisabled);
        s_uart0State          = kUart0Idle;
        s_uart0RxLength       = UART0_RX_BUFFER_SIZE;
        s_uart0FrameAvailable = true;
    }
}

void LPUART1_DMACallback(LPUART_Type *base, lpuart_edma_handle_t *handle, status_t status, void *userData)
{
    (void)base;
    (void)handle;
    (void)userData;

    Uart1_SaveDriverStatus(status);
    if (status == kStatus_LPUART_TxIdle)
    {
        /* NXP reports TxIdle only from the UART TC stage, after the stop bit. */
        s_uart1Runtime.txBusy = false;
    }
    else if (status == kStatus_LPUART_RxIdle)
    {
        /* CH6 reached 256 bytes; the NXP callback has already aborted RX DMA. */
        s_uart1Runtime.rxActive = false;
        s_uart1Runtime.rxLength = UART_BUFFER_SIZE;
        s_uart1Runtime.frameAvailable = true;
        s_uart1Runtime.diagnostics.lastRxRemaining = 0U;
        s_uart1Runtime.diagnostics.lastRxLength = UART_BUFFER_SIZE;
    }
}

void LPUART0_IRQHandler(void)
{
    const uint32_t status = LPUART_GetStatusFlags(LPUART0);
    const bool lineError = Uart0_SaveAndClearErrors(status);

    /*
     * A line error invalidates the bytes already copied by RX DMA.  Do not
     * process a coincident IDLE flag, otherwise a corrupt partial frame could
     * become visible through Uart0_GetFrame().  TX has /RE disabled, so only
     * RX owns this recovery path.
     */
    if (lineError && (s_uart0State == kUart0Receiving))
    {
        Uart0_StopDma();
        Uart0_EnterReceiveMode();
        s_uart0State          = kUart0Idle;
        s_uart0RxLength       = 0U;
        s_uart0FrameAvailable = false;
        s_uart0Diagnostics.discardedFrameCount++;
        Uart0_SaveDriverStatus(Uart0_StartReceiveInternal());
        SDK_ISR_EXIT_BARRIER;
        return;
    }

    if (((status & kLPUART_TransmissionCompleteFlag) != 0U) && (s_uart0State == kUart0TxDraining))
    {
        Uart0_CompleteTransmit();
    }

    if ((status & kLPUART_IdleLineFlag) != 0U)
    {
        LPUART_ClearStatusFlags(LPUART0, kLPUART_IdleLineFlag);
        if (s_uart0State == kUart0Receiving)
        {
            Uart0_FinishReceive();
            if (!s_uart0FrameAvailable)
            {
                (void)Uart0_StartReceiveInternal();
            }
        }
    }

    SDK_ISR_EXIT_BARRIER;
}

void LPUART1_IRQHandler(void)
{
    const uint32_t status = LPUART_GetStatusFlags(LPUART1);
    const bool lineError = Uart1_SaveAndClearErrors(status);

    if (lineError && s_uart1Runtime.rxActive)
    {
        LPUART_TransferAbortReceiveEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
        s_uart1Runtime.rxActive = false;
        s_uart1Runtime.rxLength = 0U;
        s_uart1Runtime.frameAvailable = false;
        s_uart1Runtime.diagnostics.discardedFrameCount++;
        Uart1_SaveDriverStatus(Uart1_StartReceiveInternal());
    }
    else if (((status & kLPUART_IdleLineFlag) != 0U) && s_uart1Runtime.rxActive)
    {
        LPUART_ClearStatusFlags(LPUART1, kLPUART_IdleLineFlag);
        Uart1_FinishReceive();
    }
    else if ((status & kLPUART_IdleLineFlag) != 0U)
    {
        LPUART_ClearStatusFlags(LPUART1, kLPUART_IdleLineFlag);
    }

    /* Continue the NXP eDMA TX chain only when its TC interrupt is armed. */
    if (((status & kLPUART_TransmissionCompleteFlag) != 0U) &&
        ((LPUART1->CTRL & LPUART_CTRL_TCIE_MASK) != 0U))
    {
        LPUART_TransferEdmaHandleIRQ(LPUART1, &LPUART1_LPUART_eDMA_Handle);
    }

    SDK_ISR_EXIT_BARRIER;
}

/* Explicit channel-0 ISR keeps the transactional callback independent of the startup weak wrapper. */
void DMA_CH0_IRQHandler(void)
{
    if ((EDMA_GetChannelStatusFlags(DMA0, DMA0_CH0_DMA_CHANNEL) & kEDMA_ErrorFlag) != 0U)
    {
        /* A channel error has no transactional completion callback; recover explicitly. */
        if (s_uart0State == kUart0Receiving)
        {
            s_uart0Diagnostics.discardedFrameCount++;
        }
        Uart0_StopDma();
        LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
        Uart0_EnterReceiveMode();
        s_uart0State          = kUart0Idle;
        s_uart0RxLength       = 0U;
        s_uart0FrameAvailable = false;
        Uart0_SaveDriverStatus(Uart0_StartReceiveInternal());
        SDK_ISR_EXIT_BARRIER;
        return;
    }

    EDMA_HandleIRQ(&DMA0_CH0_Handle);
    SDK_ISR_EXIT_BARRIER;
}

void DMA_CH5_IRQHandler(void)
{
    if ((EDMA_GetChannelStatusFlags(DMA0, LPUART1_TX_DMA_CHANNEL) & kEDMA_ErrorFlag) != 0U)
    {
        Uart1_SaveDmaError(LPUART1_TX_DMA_CHANNEL);
        LPUART_TransferAbortSendEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
        EDMA_ClearChannelStatusFlags(DMA0, LPUART1_TX_DMA_CHANNEL,
                                     (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                         (uint32_t)kEDMA_InterruptFlag);
        s_uart1Runtime.txBusy = false;
        Uart1_SaveDriverStatus(kStatus_Fail);
    }
    else
    {
        EDMA_HandleIRQ(&LPUART1_TX_Handle);
    }
    SDK_ISR_EXIT_BARRIER;
}

void DMA_CH6_IRQHandler(void)
{
    if ((EDMA_GetChannelStatusFlags(DMA0, LPUART1_RX_DMA_CHANNEL) & kEDMA_ErrorFlag) != 0U)
    {
        Uart1_SaveDmaError(LPUART1_RX_DMA_CHANNEL);
        LPUART_TransferAbortReceiveEDMA(LPUART1, &LPUART1_LPUART_eDMA_Handle);
        EDMA_ClearChannelStatusFlags(DMA0, LPUART1_RX_DMA_CHANNEL,
                                     (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                         (uint32_t)kEDMA_InterruptFlag);
        s_uart1Runtime.rxActive = false;
        s_uart1Runtime.rxLength = 0U;
        s_uart1Runtime.frameAvailable = false;
        s_uart1Runtime.diagnostics.discardedFrameCount++;
        Uart1_SaveDriverStatus(Uart1_StartReceiveInternal());
    }
    else
    {
        EDMA_HandleIRQ(&LPUART1_RX_Handle);
    }
    SDK_ISR_EXIT_BARRIER;
}

/*
 * Port facade.  Application code uses UartN_*; protocol-independent code can
 * use UartPort_* when it is deliberately written to serve several ports.
 *
 * UART0 and UART1 have generated hardware descriptors today. Returning
 * kStatus_Fail for UART2..3 is deliberate: a port must never touch
 * an invented DMA channel or LPUART instance.
 */
void UartPort_Init(uart_port_id_t port)
{
    if (port == kUartPort0)
    {
        Uart0_Init();
    }
    else if (port == kUartPort1)
    {
        Uart1_Init();
    }
}

status_t UartPort_StartReceive(uart_port_id_t port)
{
    return (port == kUartPort0) ? Uart0_StartReceive() :
           (port == kUartPort1) ? Uart1_StartReceive() : kStatus_Fail;
}

status_t UartPort_Send(uart_port_id_t port, const uint8_t *data, size_t size)
{
    return (port == kUartPort0) ? Uart0_Send(data, size) :
           (port == kUartPort1) ? Uart1_Send(data, size) : kStatus_Fail;
}

status_t UartPort_Reply(uart_port_id_t port, const uint8_t *data, size_t size)
{
    return (port == kUartPort0) ? Uart0_Reply(data, size) :
           (port == kUartPort1) ? Uart1_Reply(data, size) : kStatus_Fail;
}

void UartPort_Abort(uart_port_id_t port)
{
    if (port == kUartPort0)
    {
        Uart0_Abort();
    }
    else if (port == kUartPort1)
    {
        Uart1_Abort();
    }
}

bool UartPort_IsBusy(uart_port_id_t port)
{
    return ((port == kUartPort0) && Uart0_IsBusy()) ||
           ((port == kUartPort1) && Uart1_IsBusy());
}

bool UartPort_IsFrameAvailable(uart_port_id_t port)
{
    return ((port == kUartPort0) && Uart0_IsFrameAvailable()) ||
           ((port == kUartPort1) && Uart1_IsFrameAvailable());
}

const uint8_t *UartPort_GetFrame(uart_port_id_t port, size_t *length)
{
    if (port == kUartPort0)
    {
        return Uart0_GetFrame(length);
    }
    if (port == kUartPort1)
    {
        return Uart1_GetFrame(length);
    }
    if (length != NULL)
    {
        *length = 0U;
    }
    return NULL;
}

status_t UartPort_ReleaseFrame(uart_port_id_t port)
{
    return (port == kUartPort0) ? Uart0_ReleaseFrame() :
           (port == kUartPort1) ? Uart1_ReleaseFrame() : kStatus_Fail;
}

uint32_t UartPort_GetAndClearErrors(uart_port_id_t port)
{
    return (port == kUartPort0) ? Uart0_GetAndClearErrors() :
           (port == kUartPort1) ? Uart1_GetAndClearErrors() : 0U;
}

void UartPort_GetDiagnostics(uart_port_id_t port, uart_diagnostics_t *diagnostics)
{
    if ((port == kUartPort0) && (diagnostics != NULL))
    {
        Uart0_GetDiagnostics(diagnostics);
    }
    else if ((port == kUartPort1) && (diagnostics != NULL))
    {
        Uart1_GetDiagnostics(diagnostics);
    }
}

void UartPort_ClearDiagnostics(uart_port_id_t port)
{
    if (port == kUartPort0)
    {
        Uart0_ClearDiagnostics();
    }
    else if (port == kUartPort1)
    {
        Uart1_ClearDiagnostics();
    }
}

#define UART_DEFINE_FUTURE_PORT_API(n)                                                     \
    void Uart##n##_Init(void) { UartPort_Init(kUartPort##n); }                              \
    status_t Uart##n##_StartReceive(void) { return UartPort_StartReceive(kUartPort##n); }   \
    status_t Uart##n##_Send(const uint8_t *data, size_t size)                              \
    { return UartPort_Send(kUartPort##n, data, size); }                                     \
    status_t Uart##n##_Reply(const uint8_t *data, size_t size)                             \
    { return UartPort_Reply(kUartPort##n, data, size); }                                    \
    void Uart##n##_Abort(void) { UartPort_Abort(kUartPort##n); }                            \
    bool Uart##n##_IsBusy(void) { return UartPort_IsBusy(kUartPort##n); }                   \
    bool Uart##n##_IsFrameAvailable(void) { return UartPort_IsFrameAvailable(kUartPort##n); } \
    const uint8_t *Uart##n##_GetFrame(size_t *length)                                      \
    { return UartPort_GetFrame(kUartPort##n, length); }                                     \
    status_t Uart##n##_ReleaseFrame(void) { return UartPort_ReleaseFrame(kUartPort##n); }   \
    uint32_t Uart##n##_GetAndClearErrors(void)                                              \
    { return UartPort_GetAndClearErrors(kUartPort##n); }                                    \
    void Uart##n##_GetDiagnostics(uart_diagnostics_t *diagnostics)                         \
    { UartPort_GetDiagnostics(kUartPort##n, diagnostics); }                                \
    void Uart##n##_ClearDiagnostics(void) { UartPort_ClearDiagnostics(kUartPort##n); }

UART_DEFINE_FUTURE_PORT_API(2)
UART_DEFINE_FUTURE_PORT_API(3)

#undef UART_DEFINE_FUTURE_PORT_API
