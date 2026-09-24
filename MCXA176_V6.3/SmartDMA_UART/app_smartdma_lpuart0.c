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

#define UART0_RX_ERROR_INTERRUPTS (kLPUART_LinBreakInterruptEnable | kLPUART_RxOverrunInterruptEnable | \
    kLPUART_NoiseErrorInterruptEnable | kLPUART_FramingErrorInterruptEnable | \
    kLPUART_ParityErrorInterruptEnable)
#define UART0_EVENT_QUEUE_SIZE (32U)
/* UartPort_Service is called once per application tick (1 ms in the current tasks). */
#define UART0_STALL_SERVICE_LIMIT (500U)

_Static_assert(APP_SMARTDMA_LPUART0_BUFFER_SIZE <= APP_SMARTDMA_MAX_TRANSFER_SIZE,
               "LPUART0 buffer exceeds SmartDMA firmware limit");
static uint8_t s_tx[APP_SMARTDMA_LPUART0_BUFFER_SIZE];
static uint8_t s_rx[APP_SMARTDMA_LPUART0_BUFFER_SIZE];
static app_smartdma_config_t s_smartdmaConfig = {
    .txBuffer = s_tx, .txBufferSize = sizeof(s_tx),
    .rxBuffer = s_rx, .rxBufferSize = sizeof(s_rx),
};
static volatile bool s_initialized;
static volatile rx_state_t s_rxState;
static volatile tx_state_t s_txState;
static volatile app_smartdma_tx_result_t s_txResult;
static volatile size_t s_rxLength;
static volatile size_t s_pendingTxLength;
static size_t s_lastRxLength;
static size_t s_lastTxLength;
static uint8_t s_lastRxBytes[APP_SMARTDMA_LPUART0_DEBUG_BYTES];
static uint8_t s_lastTxBytes[APP_SMARTDMA_LPUART0_DEBUG_BYTES];
static volatile bool s_restartAfterAbort;
static volatile uint32_t s_errors;
static volatile uint32_t s_lineErrorCount;
static volatile uint32_t s_rxStartCount;
static volatile uint32_t s_rxFrameCount;
static volatile uint32_t s_txRequestCount;
static volatile uint32_t s_txStartCount;
static volatile uint32_t s_txCompleteCount;
static volatile uint32_t s_txWireCompleteCount;
static volatile uint32_t s_abortCompleteCount;
static volatile uint32_t s_breakCount;
static volatile uint32_t s_rxRecoveryCount;
static volatile bool s_rxErrorInterruptsMasked;
static volatile bool s_recoveryPending;
static volatile bool s_abortPending;
static uint32_t s_watchPhase;
static uint32_t s_watchPolls;
static app_uart0_event_t s_events[UART0_EVENT_QUEUE_SIZE];
static volatile uint32_t s_eventRead;
static volatile uint32_t s_eventWrite;
static volatile uint32_t s_eventNextId;
static volatile uint32_t s_eventDropCount;
static volatile uint32_t s_replyFailureCount;
static uint32_t s_lastReplyFailureFrame;

static void PushEvent(app_uart0_event_kind_t kind, uint32_t length, uint32_t flags,
                      const uint8_t *bytes)
{
    const uint32_t irqMask = DisableGlobalIRQ();
    const uint32_t next = (s_eventWrite + 1U) % UART0_EVENT_QUEUE_SIZE;
    app_uart0_event_t *event;

    if (next == s_eventRead)
    {
        s_eventDropCount++;
        EnableGlobalIRQ(irqMask);
        return;
    }
    event = &s_events[s_eventWrite];
    event->id = ++s_eventNextId;
    event->kind = (uint32_t)kind;
    event->rxFrameCount = s_rxFrameCount;
    event->txRequestCount = s_txRequestCount;
    event->recoveryCount = s_rxRecoveryCount;
    event->rxState = (uint32_t)s_rxState;
    event->txState = (uint32_t)s_txState;
    event->length = length;
    event->flags = flags;
    event->status = LPUART_GetStatusFlags(LPUART0);
    event->rxRemaining = APP_SmartDMAGetRxRemaining();
    event->direction = GPIO_PinRead(BOARD_INITPINS_RS485_EN_GPIO, BOARD_INITPINS_RS485_EN_GPIO_PIN);
    (void)memset(event->bytes, 0, sizeof(event->bytes));
    if (bytes != NULL)
    {
        const size_t count = (length < sizeof(event->bytes)) ? length : sizeof(event->bytes);
        (void)memcpy(event->bytes, bytes, count);
    }
    s_eventWrite = next;
    EnableGlobalIRQ(irqMask);
}

static status_t BeginRx(void);
static void SetDirection(bool transmit);

static void FlushRxFifo(void)
{
    /* RXFLUSH is W1S; do not accidentally clear the FIFO W1C flags. */
    uint32_t fifo = LPUART0->FIFO;
    fifo &= ~(LPUART_FIFO_TXFLUSH_MASK | LPUART_FIFO_RXFLUSH_MASK |
              LPUART_FIFO_TXOF_MASK | LPUART_FIFO_RXUF_MASK);
    LPUART0->FIFO = fifo | LPUART_FIFO_RXFLUSH_MASK;
    (void)LPUART_ClearStatusFlags(LPUART0,
                                  kLPUART_LinBreakFlag | kLPUART_IdleLineFlag |
                                  kLPUART_RxOverrunFlag | kLPUART_NoiseErrorFlag |
                                  kLPUART_FramingErrorFlag | kLPUART_ParityErrorFlag);
}

static void RecoverRx(void)
{
    FlushRxFifo();
    s_rxRecoveryCount++;
    s_rxState = kRxStopped;
    s_recoveryPending = true;
    PushEvent(kAppUart0EventRecovery, 0U, s_errors, NULL);
}

static void EnableReceivePath(void)
{
    /* RO is high impedance while DE and /RE are high. Discard any bytes
     * sampled during turn-around before exposing the receiver again. */
    FlushRxFifo();
    SetDirection(false);
    LPUART_EnableRx(LPUART0, true);
    s_rxErrorInterruptsMasked = false;
    LPUART_EnableInterrupts(LPUART0, kLPUART_IdleLineInterruptEnable | UART0_RX_ERROR_INTERRUPTS);
}

static void DisableReceivePath(void)
{
    LPUART_DisableInterrupts(LPUART0, kLPUART_IdleLineInterruptEnable | UART0_RX_ERROR_INTERRUPTS);
    LPUART_EnableRx(LPUART0, false);
    FlushRxFifo();
}

static void SetDirection(bool transmit)
{
    GPIO_PinWrite(BOARD_INITPINS_RS485_EN_GPIO, BOARD_INITPINS_RS485_EN_GPIO_PIN,
                  transmit ? 1U : 0U);
}

static status_t BeginRx(void)
{
    LPUART_ClearStatusFlags(LPUART0, kLPUART_IdleLineFlag);
    s_rxLength = 0U;
    if (!APP_SmartDMAStartRx(s_rx, APP_SMARTDMA_LPUART0_BUFFER_SIZE))
    {
        s_rxState = kRxStopped;
        s_recoveryPending = true;
        return kStatus_Busy;
    }
    s_rxState = kRxRunning;
    s_recoveryPending = false;
    s_rxStartCount++;
    return kStatus_Success;
}

static void FinishRx(void)
{
    uint32_t remaining = APP_SmartDMAGetRxRemaining();
    if (remaining > APP_SMARTDMA_LPUART0_BUFFER_SIZE) remaining = APP_SMARTDMA_LPUART0_BUFFER_SIZE;
    s_rxLength = APP_SMARTDMA_LPUART0_BUFFER_SIZE - remaining;
    if (s_rxLength != 0U)
    {
        const size_t sampleLength = (s_rxLength < sizeof(s_lastRxBytes)) ? s_rxLength : sizeof(s_lastRxBytes);
        s_lastRxLength = s_rxLength;
        (void)memcpy(s_lastRxBytes, s_rx, sampleLength);
    }
    s_rxState = (s_rxLength != 0U) ? kRxFrameReady : kRxStopped;
    if (s_rxState == kRxFrameReady)
    {
        s_rxFrameCount++;
        PushEvent(kAppUart0EventRx, (uint32_t)s_rxLength, 0U, s_rx);
    }
    if (s_rxState == kRxStopped) (void)BeginRx();
}

static status_t BeginPendingTx(void)
{
    if (s_txState != kTxPending) return kStatus_Fail;
    if (APP_SmartDMAIsBusy()) return kStatus_Busy;
    DisableReceivePath();
    SetDirection(true);
    if (!APP_SmartDMAStartTx(s_tx, (uint32_t)s_pendingTxLength))
    {
        s_txState = kTxIdle;
        s_txResult = kAppSmartDmaTxFailed;
        PushEvent(kAppUart0EventTxStartFailure, (uint32_t)s_pendingTxLength,
                  (uint32_t)kStatus_Fail, s_tx);
        EnableReceivePath();
        (void)BeginRx();
        return kStatus_Fail;
    }
    s_txState = kTxRunning;
    s_txStartCount++;
    PushEvent(kAppUart0EventTx, (uint32_t)s_pendingTxLength, 0U, s_tx);
    return kStatus_Success;
}

static void CompleteTransmit(void)
{
    LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
    s_txWireCompleteCount++;
    s_txResult = kAppSmartDmaTxComplete;
    PushEvent(kAppUart0EventTxWireComplete, (uint32_t)s_pendingTxLength, 0U, NULL);
    s_txState = kTxIdle;
    EnableReceivePath();
    (void)BeginRx();
}

static void SmartDmaDone(app_smartdma_event_t event, void *userData)
{
    (void)userData;
    /* A transfer may finish naturally while an explicit abort is being
     * requested. Either completion releases the abort gate exactly once. */
    if (s_abortPending)
    {
        if (event == kAppSmartDMAEventAbortComplete) s_abortCompleteCount++;
        s_abortPending = false;
        s_rxState = kRxStopped;
        s_txState = kTxIdle;
        RecoverRx();
        return;
    }
    if (event == kAppSmartDMAEventTxComplete)
    {
        s_txCompleteCount++;
        s_txState = kTxDraining;
        if ((LPUART_GetStatusFlags(LPUART0) & kLPUART_TransmissionCompleteFlag) != 0U)
        {
            CompleteTransmit();
        }
        else LPUART_EnableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
    }
    else if (event == kAppSmartDMAEventRxComplete)
    {
        if (s_restartAfterAbort)
        {
            s_restartAfterAbort = false;
            RecoverRx();
        }
        else if (s_rxState == kRxAbortTx)
        {
            s_rxState = kRxStopped;
            (void)BeginPendingTx();
        }
        else FinishRx();
    }
    else if (event == kAppSmartDMAEventAbortComplete)
    {
        s_abortCompleteCount++;
        if (s_rxState == kRxAbortFrame) FinishRx();
        else if (s_rxState == kRxAbortTx) { s_rxState = kRxStopped; (void)BeginPendingTx(); }
        else { s_rxState = kRxStopped; if (s_restartAfterAbort) { s_restartAfterAbort = false; RecoverRx(); } }
    }
}

bool APP_SmartDMALPUART0_Init(void)
{
    if (s_initialized) return true;
    s_smartdmaConfig.txDataRegister =
        (volatile uint32_t *)LPUART_GetDataRegisterAddress(LPUART0);
    s_smartdmaConfig.rxDataRegister =
        (volatile uint32_t *)LPUART_GetDataRegisterAddress(LPUART0);
    s_initialized = APP_SmartDMAInit(&s_smartdmaConfig);
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
    FlushRxFifo();
    LPUART_SetRxFifoWatermark(LPUART0, 0U);
    SetDirection(false); s_rxState = kRxStopped; s_txState = kTxIdle;
    s_txResult = kAppSmartDmaTxNone; s_errors = 0U; s_lineErrorCount = 0U;
    s_rxStartCount = 0U; s_rxFrameCount = 0U; s_txRequestCount = 0U;
    s_txStartCount = 0U; s_txCompleteCount = 0U; s_txWireCompleteCount = 0U; s_abortCompleteCount = 0U;
    s_breakCount = 0U; s_rxRecoveryCount = 0U; s_rxErrorInterruptsMasked = false;
    s_recoveryPending = false; s_restartAfterAbort = false; s_abortPending = false;
    s_watchPhase = 0U; s_watchPolls = 0U;
    s_lastRxLength = 0U; s_lastTxLength = 0U;
    (void)memset(s_lastRxBytes, 0, sizeof(s_lastRxBytes));
    (void)memset(s_lastTxBytes, 0, sizeof(s_lastTxBytes));
    s_eventRead = 0U; s_eventWrite = 0U; s_eventNextId = 0U; s_eventDropCount = 0U;
    s_replyFailureCount = 0U; s_lastReplyFailureFrame = 0U;
    NVIC_ClearPendingIRQ(LPUART0_IRQn); NVIC_SetPriority(LPUART0_IRQn, 5U); EnableIRQ(LPUART0_IRQn);
    EnableReceivePath();
    (void)BeginRx();
}

status_t APP_SmartDMALPUART0_StartReceive(void)
{
    if (!s_initialized) return kStatus_Fail;
    if (s_recoveryPending || s_abortPending || s_txState != kTxIdle) return kStatus_Busy;
    if (s_rxState == kRxStopped) return BeginRx();
    if (s_rxState == kRxRunning) return kStatus_Success;
    return kStatus_Busy;
}

status_t APP_SmartDMALPUART0_Send(const uint8_t *data, size_t size, bool reply)
{
    if (!s_initialized || !data || !size || size > sizeof(s_tx) || s_txState != kTxIdle ||
        s_abortPending || s_recoveryPending) return kStatus_Busy;
    if (reply && s_rxState != kRxFrameReady) return kStatus_NoTransferInProgress;
    if (!reply && s_rxState == kRxFrameReady) return kStatus_Busy;
    (void)memcpy(s_tx, data, size);
    s_lastTxLength = size;
    (void)memcpy(s_lastTxBytes, s_tx, (size < sizeof(s_lastTxBytes)) ? size : sizeof(s_lastTxBytes));
    s_pendingTxLength = size; s_txState = kTxPending;
    s_txResult = kAppSmartDmaTxPending; s_txRequestCount++;
    /* TX owns a copy now: do not expose the same RX frame again while the wire is busy. */
    if (reply) s_rxState = kRxStopped;
    if (s_rxState == kRxRunning)
    {
        s_rxState = kRxAbortTx;
        if (!APP_SmartDMAAbort())
        {
            s_rxState = kRxStopped;
            return BeginPendingTx();
        }
    }
    else return BeginPendingTx();
    return kStatus_Success;
}

void APP_SmartDMALPUART0_Abort(void)
{
    uint32_t irqMask;
    bool abortStarted;

    if (!s_initialized) return;
    irqMask = DisableGlobalIRQ();
    if (s_txResult == kAppSmartDmaTxPending) s_txResult = kAppSmartDmaTxFailed;
    s_restartAfterAbort = false; s_recoveryPending = false;
    s_rxState = kRxStopped; s_txState = kTxIdle; s_rxLength = 0U;
    DisableReceivePath();
    LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
    SetDirection(false);
    s_abortPending = true;
    abortStarted = APP_SmartDMAIsBusy() && APP_SmartDMAAbort();
    s_abortPending = abortStarted;
    EnableGlobalIRQ(irqMask);
    if (!abortStarted) RecoverRx();
}
bool APP_SmartDMALPUART0_IsBusy(void) { return s_txState != kTxIdle || s_abortPending; }
app_smartdma_tx_result_t APP_SmartDMALPUART0_GetTxResult(void) { return s_txResult; }
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
    snapshot->lastRxLength = (uint32_t)s_lastRxLength;
    snapshot->lastTxLength = (uint32_t)s_lastTxLength;
    (void)memcpy(snapshot->lastRxBytes, s_lastRxBytes, sizeof(s_lastRxBytes));
    (void)memcpy(snapshot->lastTxBytes, s_lastTxBytes, sizeof(s_lastTxBytes));
    snapshot->errors = s_errors;
    snapshot->lineErrorCount = s_lineErrorCount;
    snapshot->rxStartCount = s_rxStartCount;
    snapshot->rxFrameCount = s_rxFrameCount;
    snapshot->txRequestCount = s_txRequestCount;
    snapshot->txStartCount = s_txStartCount;
    snapshot->txCompleteCount = s_txCompleteCount;
    snapshot->txWireCompleteCount = s_txWireCompleteCount;
    snapshot->abortCompleteCount = s_abortCompleteCount;
    snapshot->breakCount = s_breakCount;
    snapshot->rxRecoveryCount = s_rxRecoveryCount;
    snapshot->recoveryPending = s_recoveryPending ? 1U : 0U;
    snapshot->eventDropCount = s_eventDropCount;
    snapshot->replyFailureCount = s_replyFailureCount;
    snapshot->lpuartStat = LPUART0->STAT;
    snapshot->lpuartCtrl = LPUART0->CTRL;
    snapshot->lpuartBaud = LPUART0->BAUD;
    snapshot->lpuartFifo = LPUART0->FIFO;
    snapshot->lpuartWater = LPUART0->WATER;
    EnableGlobalIRQ(irqMask);
    APP_SmartDMAGetDebugSnapshot(&snapshot->smartdma);
}

void APP_SmartDMALPUART0_ClearDiagnostics(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();
    s_errors = 0U;
    s_lineErrorCount = 0U;
    s_breakCount = 0U;
    s_rxRecoveryCount = 0U;
    s_eventDropCount = 0U;
    EnableGlobalIRQ(irqMask);
}

bool APP_SmartDMALPUART0_PopEvent(app_uart0_event_t *event)
{
    const uint32_t irqMask = DisableGlobalIRQ();
    if ((event == NULL) || (s_eventRead == s_eventWrite))
    {
        EnableGlobalIRQ(irqMask);
        return false;
    }
    *event = s_events[s_eventRead];
    s_eventRead = (s_eventRead + 1U) % UART0_EVENT_QUEUE_SIZE;
    EnableGlobalIRQ(irqMask);
    return true;
}

void APP_SmartDMALPUART0_RecordReplyFailure(status_t status)
{
    s_replyFailureCount++;
    if (s_lastReplyFailureFrame != s_rxFrameCount)
    {
        s_lastReplyFailureFrame = s_rxFrameCount;
        PushEvent(kAppUart0EventReplyFailure, (uint32_t)s_rxLength, (uint32_t)status, s_rx);
    }
}

static void RestartStalledSmartDma(void)
{
    uint32_t fifo;
    const uint32_t irqMask = DisableGlobalIRQ();

    /* APP_SmartDMAInit is the firmware reset path. Stop the UART shifter and
     * discard a partial TX before reusing the shared parameter block. */
    DisableReceivePath();
    LPUART_DisableInterrupts(LPUART0, kLPUART_TransmissionCompleteInterruptEnable);
    LPUART_EnableTx(LPUART0, false);
    fifo = LPUART0->FIFO & ~(LPUART_FIFO_TXFLUSH_MASK | LPUART_FIFO_RXFLUSH_MASK |
                              LPUART_FIFO_TXOF_MASK | LPUART_FIFO_RXUF_MASK);
    LPUART0->FIFO = fifo | LPUART_FIFO_TXFLUSH_MASK;
    SetDirection(false);
    s_txState = kTxIdle;
    s_rxState = kRxStopped;
    s_abortPending = false;
    s_restartAfterAbort = false;
    s_recoveryPending = true;
    s_txResult = kAppSmartDmaTxFailed;
    s_initialized = APP_SmartDMAInit(&s_smartdmaConfig);
    if (s_initialized)
    {
        APP_SmartDMASetCallback(SmartDmaDone, NULL);
        LPUART_EnableTx(LPUART0, true);
        EnableReceivePath();
        (void)BeginRx();
    }
    EnableGlobalIRQ(irqMask);
}

void APP_SmartDMALPUART0_Service(void)
{
    uint32_t phase;

    if (!s_initialized)
    {
        /* A failed firmware restart must not leave the port permanently idle. */
        if (s_recoveryPending && ++s_watchPolls >= UART0_STALL_SERVICE_LIMIT)
        {
            s_watchPolls = 0U;
            RestartStalledSmartDma();
        }
        return;
    }
    phase = s_abortPending ? 4U : (uint32_t)s_txState;
    if (phase != s_watchPhase)
    {
        s_watchPhase = phase;
        s_watchPolls = 0U;
    }
    else if (phase != 0U && ++s_watchPolls >= UART0_STALL_SERVICE_LIMIT)
    {
        s_watchPolls = 0U;
        PushEvent(kAppUart0EventWatchdog, 0U, phase, NULL);
        if ((s_abortPending ? 4U : (uint32_t)s_txState) != phase) return;
        if (s_abortPending) RestartStalledSmartDma();
        else APP_SmartDMALPUART0_Abort();
        return;
    }
    if ((LPUART_GetStatusFlags(LPUART0) & kLPUART_RxActiveFlag) != 0U) return;

    if (s_txState == kTxPending && s_rxState == kRxStopped && !s_abortPending &&
        !APP_SmartDMAIsBusy())
    {
        (void)BeginPendingTx();
    }
    if (s_recoveryPending && !s_abortPending && s_txState == kTxIdle && !APP_SmartDMAIsBusy())
    {
        /* Discard anything received while SmartDMA RX was stopped. */
        EnableReceivePath();
        (void)BeginRx();
    }
    if (s_rxErrorInterruptsMasked && !s_recoveryPending && !s_restartAfterAbort &&
        (s_rxState == kRxRunning))
    {
        (void)LPUART_ClearStatusFlags(LPUART0,
            kLPUART_LinBreakFlag | kLPUART_RxOverrunFlag |
            kLPUART_NoiseErrorFlag | kLPUART_FramingErrorFlag | kLPUART_ParityErrorFlag);
        s_rxErrorInterruptsMasked = false;
        LPUART_EnableInterrupts(LPUART0, UART0_RX_ERROR_INTERRUPTS);
    }
}

void APP_SmartDMALPUART0_HandleLpuartIrq(void)
{
    const uint32_t flags = LPUART_GetStatusFlags(LPUART0);
    const uint32_t errors = flags & (kLPUART_LinBreakFlag | kLPUART_RxOverrunFlag |
        kLPUART_NoiseErrorFlag | kLPUART_FramingErrorFlag | kLPUART_ParityErrorFlag);
    if (errors)
    {
        s_lineErrorCount++;
        PushEvent(kAppUart0EventError, 0U, errors, NULL);
        if ((errors & kLPUART_LinBreakFlag) != 0U) s_breakCount++;
        s_rxErrorInterruptsMasked = true;
        LPUART_DisableInterrupts(LPUART0, UART0_RX_ERROR_INTERRUPTS);
        (void)LPUART_ClearStatusFlags(LPUART0, errors);
        s_errors |= errors;
        if ((s_rxState == kRxRunning) || (s_rxState == kRxAbortFrame))
        {
            s_rxState = kRxStopped;
            s_restartAfterAbort = true;
            if (!APP_SmartDMAAbort())
            {
                s_restartAfterAbort = false;
                if (!APP_SmartDMAIsBusy()) RecoverRx();
            }
        }
    }
    if ((flags & kLPUART_IdleLineFlag) != 0U) { LPUART_ClearStatusFlags(LPUART0, kLPUART_IdleLineFlag); if (s_rxState == kRxRunning) { s_rxState=kRxAbortFrame; if (!APP_SmartDMAAbort()) FinishRx(); } }
    if ((flags & kLPUART_TransmissionCompleteFlag) && s_txState == kTxDraining) CompleteTransmit();
}
