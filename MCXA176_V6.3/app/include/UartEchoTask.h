/* Temporary UART0/RS485 and UART1/RS232 loopback test tasks. */
#ifndef UART_ECHO_TASK_H_
#define UART_ECHO_TASK_H_

#include <stdbool.h>

#include "fsl_common.h"

void UartEchoTask(void *argument);
bool UartEchoTask_Create(void);
void UartEcho_OnReplyFailed(status_t status);

/* UART1/RS232 full-duplex DMA echo test. */
void Uart1EchoTask(void *argument);
bool Uart1EchoTask_Create(void);
void Uart1Echo_OnReplyFailed(status_t status);

/* Sends one known frame after boot to verify the UART0/RS485 TX DMA chain. */
status_t UartEchoTest_SendOnce(void);

#endif /* UART_ECHO_TASK_H_ */
