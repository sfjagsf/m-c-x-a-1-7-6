#include "UartEchoTask.h"

#include "UartDriver.h"
#include "cmsis_os2.h"
#include "../../SmartDMA_UART/app_smartdma_lpuart0.h"

#include <stdio.h>

#define UART_ECHO_TASK_PERIOD_MS (1U)
#define UART0_DEBUG_LOG_PERIOD_MS (500U)

/* Keep diagnostic storage out of the 1 KB UART task stack. */
static app_smartdma_lpuart0_debug_snapshot_t s_uart0DebugSnapshot;
static char s_uart0DebugLine[240];
static char s_uart0RxHex[2U * APP_SMARTDMA_LPUART0_DEBUG_BYTES + 1U];
static char s_uart0TxHex[2U * APP_SMARTDMA_LPUART0_DEBUG_BYTES + 1U];
static app_uart0_event_t s_uart0Event;
static char s_uart0EventHex[2U * APP_SMARTDMA_LPUART0_DEBUG_BYTES + 1U];

static void Uart0_FormatHexSample(char *output, const uint8_t *bytes, uint32_t frameLength)
{
    static const char hex[] = "0123456789ABCDEF";
    const uint32_t count = (frameLength < APP_SMARTDMA_LPUART0_DEBUG_BYTES) ?
                           frameLength : APP_SMARTDMA_LPUART0_DEBUG_BYTES;

    for (uint32_t i = 0U; i < count; i++)
    {
        output[2U * i] = hex[bytes[i] >> 4U];
        output[2U * i + 1U] = hex[bytes[i] & 0x0FU];
    }
    output[2U * count] = '\0';
}

static void Uart0_LogSmartDmaStatus(void)
{
    static uint32_t nextLogTick;
    static bool dataNext;
    int length;

    if ((int32_t)(osKernelGetTickCount() - nextLogTick) < 0) return;
    nextLogTick = osKernelGetTickCount() + UART0_DEBUG_LOG_PERIOD_MS;
    if (Uart1_IsBusy()) return;

    APP_SmartDMALPUART0_GetDebugSnapshot(&s_uart0DebugSnapshot);
    if (dataNext)
    {
        Uart0_FormatHexSample(s_uart0RxHex, s_uart0DebugSnapshot.lastRxBytes,
                              s_uart0DebugSnapshot.lastRxLength);
        Uart0_FormatHexSample(s_uart0TxHex, s_uart0DebugSnapshot.lastTxBytes,
                              s_uart0DebugSnapshot.lastTxLength);
        length = snprintf(s_uart0DebugLine, sizeof(s_uart0DebugLine),
                          "U0DATA C%lu/%lu Q%lu RX%lu:%s TX%lu:%s (first 16 bytes)\r\n",
                          (unsigned long)s_uart0DebugSnapshot.rxFrameCount,
                          (unsigned long)s_uart0DebugSnapshot.txRequestCount,
                          (unsigned long)s_uart0DebugSnapshot.eventDropCount,
                          (unsigned long)s_uart0DebugSnapshot.lastRxLength, s_uart0RxHex,
                          (unsigned long)s_uart0DebugSnapshot.lastTxLength, s_uart0TxHex);
    }
    else
    {
        length = snprintf(s_uart0DebugLine, sizeof(s_uart0DebugLine),
                          "U0SD I%lu R%lu T%lu E%08lX C%lu/%lu/%lu/%lu/%lu/%lu A%lu "
                          "B%lu/%lu/%lu Q%lu FAIL%lu CMD%lu/%lu/%lu rem%lu/%lu ST%08lX CT%08lX BD%08lX PC%08lX\r\n",
                          (unsigned long)s_uart0DebugSnapshot.initialized,
                          (unsigned long)s_uart0DebugSnapshot.rxState,
                          (unsigned long)s_uart0DebugSnapshot.txState,
                          (unsigned long)s_uart0DebugSnapshot.errors,
                          (unsigned long)s_uart0DebugSnapshot.rxStartCount,
                          (unsigned long)s_uart0DebugSnapshot.rxFrameCount,
                          (unsigned long)s_uart0DebugSnapshot.txRequestCount,
                          (unsigned long)s_uart0DebugSnapshot.txStartCount,
                          (unsigned long)s_uart0DebugSnapshot.txCompleteCount,
                          (unsigned long)s_uart0DebugSnapshot.txWireCompleteCount,
                          (unsigned long)s_uart0DebugSnapshot.abortCompleteCount,
                          (unsigned long)s_uart0DebugSnapshot.breakCount,
                          (unsigned long)s_uart0DebugSnapshot.rxRecoveryCount,
                          (unsigned long)s_uart0DebugSnapshot.recoveryPending,
                          (unsigned long)s_uart0DebugSnapshot.eventDropCount,
                          (unsigned long)s_uart0DebugSnapshot.replyFailureCount,
                          (unsigned long)s_uart0DebugSnapshot.smartdma.command,
                          (unsigned long)s_uart0DebugSnapshot.smartdma.activeCommand,
                          (unsigned long)s_uart0DebugSnapshot.smartdma.lastCommand,
                          (unsigned long)s_uart0DebugSnapshot.smartdma.rxRemaining,
                          (unsigned long)s_uart0DebugSnapshot.smartdma.txRemaining,
                          (unsigned long)s_uart0DebugSnapshot.lpuartStat,
                          (unsigned long)s_uart0DebugSnapshot.lpuartCtrl,
                          (unsigned long)s_uart0DebugSnapshot.lpuartBaud,
                          (unsigned long)s_uart0DebugSnapshot.smartdma.smartdmaPc);
    }
    dataNext = !dataNext;
    if ((length > 0) && ((size_t)length < sizeof(s_uart0DebugLine)))
    {
        (void)Uart1_Send((const uint8_t *)s_uart0DebugLine, (size_t)length);
    }
}

static void Uart0_LogEvent(void)
{
    int length;

    if (Uart1_IsBusy() || !APP_SmartDMALPUART0_PopEvent(&s_uart0Event)) return;
    Uart0_FormatHexSample(s_uart0EventHex, s_uart0Event.bytes, s_uart0Event.length);
    length = snprintf(s_uart0DebugLine, sizeof(s_uart0DebugLine),
                      "U0EV N%lu K%lu C%lu/%lu B%lu R%lu T%lu L%lu F%08lX S%08lX rem%lu D%lu HEX%s\r\n",
                      (unsigned long)s_uart0Event.id, (unsigned long)s_uart0Event.kind,
                      (unsigned long)s_uart0Event.rxFrameCount,
                      (unsigned long)s_uart0Event.txRequestCount,
                      (unsigned long)s_uart0Event.recoveryCount,
                      (unsigned long)s_uart0Event.rxState,
                      (unsigned long)s_uart0Event.txState,
                      (unsigned long)s_uart0Event.length,
                      (unsigned long)s_uart0Event.flags,
                      (unsigned long)s_uart0Event.status,
                      (unsigned long)s_uart0Event.rxRemaining,
                      (unsigned long)s_uart0Event.direction, s_uart0EventHex);
    if ((length > 0) && ((size_t)length < sizeof(s_uart0DebugLine)))
    {
        (void)Uart1_Send((const uint8_t *)s_uart0DebugLine, (size_t)length);
    }
}

void UartEchoTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        const uint8_t *frame;
        size_t length;

        APP_SmartDMALPUART0_Service();
        frame = Uart0_GetFrame(&length);
        if ((frame != NULL) && (length != 0U))
        {
            const status_t status = Uart0_Reply(frame, length);

            if (status != kStatus_Success)
            {
                APP_SmartDMALPUART0_RecordReplyFailure(status);
                UartEcho_OnReplyFailed(status);
            }
        }

        Uart0_LogEvent();
        Uart0_LogSmartDmaStatus();

        (void)osDelay(UART_ECHO_TASK_PERIOD_MS);
    }
}

void Uart1EchoTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        const uint8_t *frame;
        size_t length;

        frame = Uart1_GetFrame(&length);
        if ((frame != NULL) && (length != 0U))
        {
            const status_t status = Uart1_Reply(frame, length);

            if (status != kStatus_Success)
            {
                Uart1Echo_OnReplyFailed(status);
            }
        }

        (void)osDelay(UART_ECHO_TASK_PERIOD_MS);
    }
}

__WEAK void UartEcho_OnReplyFailed(status_t status)
{
    (void)status;
}

__WEAK void Uart1Echo_OnReplyFailed(status_t status)
{
    (void)status;
}
