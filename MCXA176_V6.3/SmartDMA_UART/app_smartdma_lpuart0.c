/* LPUART0 RS485 transport backed by the application SmartDMA firmware. */
#include "app_smartdma_lpuart0.h"

#include <string.h>

#include "app_smartdma.h"
#include "fsl_edma.h"
#include "fsl_gpio.h"
#include "fsl_lpuart.h"
#include "peripherals.h"
#include "pin_mux.h"

typedef enum { kRxStopped, kRxRunning, kRxFrameReady, kRxAbortFrame, kRxAbortTx } rx_state_t;
typedef enum { kTxIdle, kTxPending, kTxRunning, kTxDraining } tx_state_t;

_Static_assert(APP_SMARTDMA_LPUART0_BUFFER_SIZE <= APP_SMARTDMA_MAX_TRANSFER_SIZE,
               "LPUART0 buffer exceeds SmartDMA firmware limit");
static uint8_t s_tx[APP_SMARTDMA_LPUART0_BUFFER_SIZE];
static uint8_t s_rx[APP_SMARTDMA_LPUART0_BUFFER_SIZE];
static volatile bool s_initialized;
static volatile rx_state_t s_rxState;
static volatile tx_state_t s_txState;
static volatile size_t s_rxLength;
static volatile size_t s_pendingTxLength;
static volatile bool s_restartAfterAbort;
static volatile uint32_t s_errors;
static volatile uint32_t s_rxStartCount;
static volatile uint32_t s_rxFrameCount;
static volatile uint32_t s_txRequestCount;
static volatile uint32_t s_txStartCount;
static volatile uint32_t s_txCompleteCount;
static volatile uint32_t s_txWireCompleteCount;
static volatile uint32_t s_abortCompleteCount;

static void SetDirection(bool transmit)
{
    GPIO_PinWrite(BOARD_INITPINS_RS485_EN_GPIO, BOARD_INITPINS_RS485_EN_GPIO_PIN,
                  transmit ? 1U : 0U);
}

static status_t BeginRx(void)
{
    LPUART_ClearStatusFlags(LPUART0, kLPUART_IdleLineFlag);
    s_rxLength = 0U;
    if (!APP_SmartDMAStartRx(s_rx, APP_SMARTDMA_LPUART0_BUFFER_SIZE)) return kStatus_Busy;
    s_rxState = kRxRunning;
    s_rxStartCount++;
    return kStatus_Success;
}

static void FinishRx(void)
{
    uint32_t remaining = APP_SmartDMAGetRxRemaining();
    if (remaining > APP_SMARTDMA_LPUART0_BUFFER_SIZE) remaining = APP_SMARTDMA_LPUART0_BUFFER_SIZE;
    s_rxLength = APP_SMARTDMA_LPUART0_BUFFER_SIZE - remaining;
    s_rxState = (s_rxLength != 0U) ? kRxFrameReady : kRxStopped;
    if (s_rxState == kRxFrameReady) s_rxFrameCount++;
    if (s_rxState == kRxStopped) (void)BeginRx();
}

static void BeginPendingTx(void)
{
    if ((s_txState != kTxPending) || APP_SmartDMAIsBusy()) return;
    SetDirection(true);
    if (!APP_SmartDMAStartTx(s_tx, (uint32_t)s_pendingTxLength))
    {
        SetDirection(false); s_txState = kTxIdle; (void)BeginRx(); return;
    }
    s_txState = kTxRunning;
    s_txStartCount++;
}

static void SmartDmaDone(app_smartdma_event_t event, void *userData)
{
    (void)userData;
    if (event == kAppSmartDMAEventTxComplete)
    {
        s_txCompleteCount++;
        s_txState = kTxDraining;
        if ((LPUART_GetStatusFlags(LPUART0) & kLPUART_TransmissionCompleteFlag) != 0U)
        {
            s_txWireCompleteCount++;
            SetDirection(false); s_txState = kTxIdle; (void)BeginRx();
        }
        else LPUART_EnableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
    }
    else if (event == kAppSmartDMAEventRxComplete) FinishRx();
    else if (event == kAppSmartDMAEventAbortComplete)
    {
        s_abortCompleteCount++;
        if (s_rxState == kRxAbortFrame) FinishRx();
        else if (s_rxState == kRxAbortTx) { s_rxState = kRxStopped; BeginPendingTx(); }
        else { s_rxState = kRxStopped; if (s_restartAfterAbort) { s_restartAfterAbort = false; (void)BeginRx(); } }
    }
}

bool APP_SmartDMALPUART0_Init(void)
{
    const app_smartdma_config_t config = {
        .txBuffer = s_tx, .txBufferSize = sizeof(s_tx), .txDataRegister = &LPUART0->DATA,
        .rxBuffer = s_rx, .rxBufferSize = sizeof(s_rx), .rxDataRegister = &LPUART0->DATA,
    };
    if (s_initialized) return true;
    s_initialized = APP_SmartDMAInit(&config);
    if (s_initialized) APP_SmartDMASetCallback(SmartDmaDone, NULL);
    return s_initialized;
}

bool APP_SmartDMALPUART0_IsInitialized(void) { return s_initialized; }

void APP_SmartDMALPUART0_TransportInit(void)
{
    if (!APP_SmartDMALPUART0_Init()) return;
    LPUART_EnableTxDMA(LPUART0, false); LPUART_EnableRxDMA(LPUART0, false);
    EDMA_AbortTransfer(&DMA0_CH0_Handle);
    EDMA_SetChannelMux(DMA0, DMA0_CH0_DMA_CHANNEL, kDma0RequestDisabled);
    LPUART_DisableInterrupts(LPUART0, kLPUART_AllInterruptEnable);
    LPUART_ClearStatusFlags(LPUART0, kLPUART_AllClearFlags);
    LPUART_SetRxFifoWatermark(LPUART0, 0U);
    SetDirection(false); s_rxState = kRxStopped; s_txState = kTxIdle; s_errors = 0U;
    s_rxStartCount = 0U; s_rxFrameCount = 0U; s_txRequestCount = 0U;
    s_txStartCount = 0U; s_txCompleteCount = 0U; s_txWireCompleteCount = 0U; s_abortCompleteCount = 0U;
    NVIC_ClearPendingIRQ(LPUART0_IRQn); NVIC_SetPriority(LPUART0_IRQn, 5U); EnableIRQ(LPUART0_IRQn);
    LPUART_EnableInterrupts(LPUART0, kLPUART_IdleLineInterruptEnable | kLPUART_RxOverrunInterruptEnable |
        kLPUART_NoiseErrorInterruptEnable | kLPUART_FramingErrorInterruptEnable | kLPUART_ParityErrorInterruptEnable);
    (void)BeginRx();
}

status_t APP_SmartDMALPUART0_StartReceive(void)
{
    if (!s_initialized) return kStatus_Fail;
    if (s_rxState == kRxStopped) return BeginRx();
    if (s_rxState == kRxRunning) return kStatus_Success;
    return kStatus_Busy;
}

status_t APP_SmartDMALPUART0_Send(const uint8_t *data, size_t size, bool reply)
{
    (void)reply;
    if (!s_initialized || !data || !size || size > sizeof(s_tx) || s_txState != kTxIdle) return kStatus_Busy;
    (void)memcpy(s_tx, data, size); s_pendingTxLength = size; s_txState = kTxPending; s_txRequestCount++;
    if (s_rxState == kRxRunning)
    {
        s_rxState = kRxAbortTx;
        if (!APP_SmartDMAAbort()) { s_rxState = kRxStopped; BeginPendingTx(); }
    }
    else BeginPendingTx();
    return kStatus_Success;
}

void APP_SmartDMALPUART0_Abort(void)
{
    s_restartAfterAbort = false; s_rxState = kRxStopped; s_txState = kTxIdle; s_rxLength = 0U;
    if (APP_SmartDMAIsBusy()) (void)APP_SmartDMAAbort();
    LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable); SetDirection(false);
}
bool APP_SmartDMALPUART0_IsBusy(void) { return s_txState != kTxIdle; }
bool APP_SmartDMALPUART0_IsFrameAvailable(void) { return s_rxState == kRxFrameReady; }
const uint8_t *APP_SmartDMALPUART0_GetFrame(size_t *length)
{ if (length) *length = APP_SmartDMALPUART0_IsFrameAvailable() ? s_rxLength : 0U; return APP_SmartDMALPUART0_IsFrameAvailable() ? s_rx : NULL; }
status_t APP_SmartDMALPUART0_ReleaseFrame(void)
{ if (s_rxState != kRxFrameReady) return kStatus_NoTransferInProgress; s_rxState = kRxStopped; return BeginRx(); }
uint32_t APP_SmartDMALPUART0_GetAndClearErrors(void) { uint32_t value=s_errors; s_errors=0U; return value; }

void APP_SmartDMALPUART0_GetDebugSnapshot(app_smartdma_lpuart0_debug_snapshot_t *snapshot)
{
    uint32_t irqMask;

    if (snapshot == NULL) return;

    irqMask = DisableGlobalIRQ();
    snapshot->initialized = s_initialized ? 1U : 0U;
    snapshot->rxState = (uint32_t)s_rxState;
    snapshot->txState = (uint32_t)s_txState;
    snapshot->rxLength = (uint32_t)s_rxLength;
    snapshot->pendingTxLength = (uint32_t)s_pendingTxLength;
    snapshot->errors = s_errors;
    snapshot->rxStartCount = s_rxStartCount;
    snapshot->rxFrameCount = s_rxFrameCount;
    snapshot->txRequestCount = s_txRequestCount;
    snapshot->txStartCount = s_txStartCount;
    snapshot->txCompleteCount = s_txCompleteCount;
    snapshot->txWireCompleteCount = s_txWireCompleteCount;
    snapshot->abortCompleteCount = s_abortCompleteCount;
    snapshot->lpuartStat = LPUART0->STAT;
    snapshot->lpuartCtrl = LPUART0->CTRL;
    snapshot->lpuartBaud = LPUART0->BAUD;
    snapshot->lpuartFifo = LPUART0->FIFO;
    snapshot->lpuartWater = LPUART0->WATER;
    EnableGlobalIRQ(irqMask);
    APP_SmartDMAGetDebugSnapshot(&snapshot->smartdma);
}

void APP_SmartDMALPUART0_HandleLpuartIrq(void)
{
    const uint32_t flags = LPUART_GetStatusFlags(LPUART0);
    const uint32_t errors = flags & (kLPUART_RxOverrunFlag | kLPUART_NoiseErrorFlag | kLPUART_FramingErrorFlag | kLPUART_ParityErrorFlag);
    if (errors) { LPUART_ClearStatusFlags(LPUART0, errors); s_errors |= errors; if (s_rxState == kRxRunning) { s_rxState=kRxStopped; s_restartAfterAbort=true; (void)APP_SmartDMAAbort(); } }
    if ((flags & kLPUART_IdleLineFlag) != 0U) { LPUART_ClearStatusFlags(LPUART0, kLPUART_IdleLineFlag); if (s_rxState == kRxRunning) { s_rxState=kRxAbortFrame; if (!APP_SmartDMAAbort()) FinishRx(); } }
    if ((flags & kLPUART_TransmissionCompleteFlag) && s_txState == kTxDraining) { LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable); s_txWireCompleteCount++; SetDirection(false); s_txState=kTxIdle; (void)BeginRx(); }
}
