#include "UartEchoTask.h"

#include "UartDriver.h"
#include "cmsis_os2.h"
#include "../../SmartDMA_UART/app_smartdma_lpuart0.h"

#include <stdio.h>

#define UART_ECHO_TASK_PERIOD_MS (1U)
#define UART0_DEBUG_LOG_PERIOD_MS (500U)

static void Uart0_LogSmartDmaStatus(void)
{
    static uint32_t nextLogTick;
    app_smartdma_lpuart0_debug_snapshot_t snapshot;
    char line[240];
    int length;

    if ((int32_t)(osKernelGetTickCount() - nextLogTick) < 0) return;
    nextLogTick = osKernelGetTickCount() + UART0_DEBUG_LOG_PERIOD_MS;
    if (Uart1_IsBusy()) return;

    APP_SmartDMALPUART0_GetDebugSnapshot(&snapshot);
    length = snprintf(line, sizeof(line),
                      "U0SD I%lu R%lu T%lu L%lu E%08lX C[%lu/%lu %lu/%lu/%lu/%lu A%lu] "
                       "CMD%lu/%lu FW%lu rem%lu/%lu done%lu/%lu/%lu TX%08lX/%02lX RX%08lX/%02lX "
                       "ST%08lX CT%08lX BD%08lX PC%08lX\\r\\n",
                      (unsigned long)snapshot.initialized, (unsigned long)snapshot.rxState,
                      (unsigned long)snapshot.txState, (unsigned long)snapshot.rxLength,
                      (unsigned long)snapshot.errors, (unsigned long)snapshot.rxStartCount,
                      (unsigned long)snapshot.rxFrameCount, (unsigned long)snapshot.txRequestCount,
                      (unsigned long)snapshot.txStartCount, (unsigned long)snapshot.txCompleteCount,
                      (unsigned long)snapshot.txWireCompleteCount,
                       (unsigned long)snapshot.abortCompleteCount, (unsigned long)snapshot.smartdma.command,
                       (unsigned long)snapshot.smartdma.activeCommand, (unsigned long)snapshot.smartdma.lastCommand,
                      (unsigned long)snapshot.smartdma.rxRemaining,
                      (unsigned long)snapshot.smartdma.txRemaining,
                      (unsigned long)snapshot.smartdma.rxComplete,
                      (unsigned long)snapshot.smartdma.txComplete,
                       (unsigned long)snapshot.smartdma.abortComplete,
                       (unsigned long)snapshot.smartdma.txLastStatus,
                       (unsigned long)snapshot.smartdma.txLastData,
                       (unsigned long)snapshot.smartdma.rxLastStatus,
                       (unsigned long)snapshot.smartdma.rxLastData,
                      (unsigned long)snapshot.lpuartStat, (unsigned long)snapshot.lpuartCtrl,
                      (unsigned long)snapshot.lpuartBaud, (unsigned long)snapshot.smartdma.smartdmaPc);
    if ((length > 0) && ((size_t)length < sizeof(line)))
    {
        (void)Uart1_Send((const uint8_t *)line, (size_t)length);
    }
}

void UartEchoTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        const uint8_t *frame;
        size_t length;

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
