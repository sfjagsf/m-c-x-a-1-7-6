/* UART0/RS485 and UART1/RS232 frame echo tasks. */
#ifndef UART_ECHO_TASK_H_
#define UART_ECHO_TASK_H_

#include <stdbool.h>

#include "fsl_common.h"

void UartEchoTask(void *argument);
bool UartEchoTask_Create(void);
void UartEcho_OnReplyFailed(status_t status);

/* UART1/RS232 full-duplex frame echo task. */
void Uart1EchoTask(void *argument);
bool Uart1EchoTask_Create(void);
void Uart1Echo_OnReplyFailed(status_t status);

#endif /* UART_ECHO_TASK_H_ */
