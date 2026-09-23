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

static void Uart0_LogSmartDmaStatus(void)
{
    static uint32_t nextLogTick;
    int length;

    if ((int32_t)(osKernelGetTickCount() - nextLogTick) < 0) return;
    nextLogTick = osKernelGetTickCount() + UART0_DEBUG_LOG_PERIOD_MS;
    if (Uart1_IsBusy()) return;

    APP_SmartDMALPUART0_GetDebugSnapshot(&s_uart0DebugSnapshot);
    length = snprintf(s_uart0DebugLine, sizeof(s_uart0DebugLine),
                      "U0SD I%lu R%lu T%lu L%lu E%08lX C[%lu/%lu %lu/%lu/%lu/%lu A%lu] "
                       "B%lu/%lu/%lu CMD%lu/%lu FW%lu rem%lu/%lu done%lu/%lu/%lu TX%08lX/%02lX RX%08lX/%02lX "
                       "ST%08lX CT%08lX BD%08lX F%08lX W%08lX PC%08lX\r\n",
                      (unsigned long)s_uart0DebugSnapshot.initialized, (unsigned long)s_uart0DebugSnapshot.rxState,
                      (unsigned long)s_uart0DebugSnapshot.txState, (unsigned long)s_uart0DebugSnapshot.rxLength,
                      (unsigned long)s_uart0DebugSnapshot.errors, (unsigned long)s_uart0DebugSnapshot.rxStartCount,
                      (unsigned long)s_uart0DebugSnapshot.rxFrameCount, (unsigned long)s_uart0DebugSnapshot.txRequestCount,
                      (unsigned long)s_uart0DebugSnapshot.txStartCount, (unsigned long)s_uart0DebugSnapshot.txCompleteCount,
                      (unsigned long)s_uart0DebugSnapshot.txWireCompleteCount,
                       (unsigned long)s_uart0DebugSnapshot.abortCompleteCount,
                       (unsigned long)s_uart0DebugSnapshot.breakCount,
                       (unsigned long)s_uart0DebugSnapshot.rxRecoveryCount,
                       (unsigned long)s_uart0DebugSnapshot.recoveryPending,
                       (unsigned long)s_uart0DebugSnapshot.smartdma.command,
                       (unsigned long)s_uart0DebugSnapshot.smartdma.activeCommand, (unsigned long)s_uart0DebugSnapshot.smartdma.lastCommand,
                      (unsigned long)s_uart0DebugSnapshot.smartdma.rxRemaining,
                      (unsigned long)s_uart0DebugSnapshot.smartdma.txRemaining,
                      (unsigned long)s_uart0DebugSnapshot.smartdma.rxComplete,
                      (unsigned long)s_uart0DebugSnapshot.smartdma.txComplete,
                       (unsigned long)s_uart0DebugSnapshot.smartdma.abortComplete,
                       (unsigned long)s_uart0DebugSnapshot.smartdma.txLastStatus,
                       (unsigned long)s_uart0DebugSnapshot.smartdma.txLastData,
                       (unsigned long)s_uart0DebugSnapshot.smartdma.rxLastStatus,
                       (unsigned long)s_uart0DebugSnapshot.smartdma.rxLastData,
                      (unsigned long)s_uart0DebugSnapshot.lpuartStat, (unsigned long)s_uart0DebugSnapshot.lpuartCtrl,
                      (unsigned long)s_uart0DebugSnapshot.lpuartBaud,
                      (unsigned long)s_uart0DebugSnapshot.lpuartFifo,
                      (unsigned long)s_uart0DebugSnapshot.lpuartWater,
                      (unsigned long)s_uart0DebugSnapshot.smartdma.smartdmaPc);
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
                UartEcho_OnReplyFailed(status);
            }
        }

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
