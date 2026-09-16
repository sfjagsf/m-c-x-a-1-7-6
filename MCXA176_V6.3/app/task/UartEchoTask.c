#include "UartEchoTask.h"

#include "UartDriver.h"
#include "cmsis_os2.h"

#define UART_ECHO_TASK_PERIOD_MS (1U)

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
