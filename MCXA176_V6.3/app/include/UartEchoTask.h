/* Temporary RS485/UART0 loopback test task. */
#ifndef UART_ECHO_TASK_H_
#define UART_ECHO_TASK_H_

#include <stdbool.h>

#include "fsl_common.h"

void UartEchoTask(void *argument);
bool UartEchoTask_Create(void);
void UartEcho_OnReplyFailed(status_t status);

/* Sends one known frame after boot to verify the UART0/RS485 TX DMA chain. */
status_t UartEchoTest_SendOnce(void);

#endif /* UART_ECHO_TASK_H_ */
