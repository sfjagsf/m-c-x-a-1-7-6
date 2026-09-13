#include "UartEchoTask.h"

#include "UartDriver.h"
#include "cmsis_os2.h"

#define UART_ECHO_TASK_PERIOD_MS (1U)

/*
 * Enable echo after validating the RS485 direction GPIO. P0_19 is asserted
 * only for the duration of each reply, then the driver returns to RX mode.
 */
#define UART0_ECHO_TEST_ENABLE (1U)

static const uint8_t s_uart0TxTestFrame[] = "RS485 UART0 TX TEST\r\n";

status_t UartEchoTest_SendOnce(void)
{
    return Uart0_Send(s_uart0TxTestFrame, sizeof(s_uart0TxTestFrame) - 1U);
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
#if (UART0_ECHO_TEST_ENABLE != 0U)
            status_t status;

            /* Reply copies RX into UART0's private TX buffer before restarting DMA. */
            status = Uart0_Reply(frame, length);
            if (status != kStatus_Success)
            {
                /* Preserve the failure for the debugger instead of silently dropping it. */
                UartEcho_OnReplyFailed(status);
            }
#else
            /* Receive-only diagnostic mode: do not raise RS485_EN for TX. */
            (void)Uart0_ReleaseFrame();
#endif
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
