/*
 * Default application task creators.
 *
 * Keep all weak XxxTask_Create functions here.  A product application may
 * override any creator with a same-named non-weak definition.
 */
#include "GpioInputTask.h"
#include "PwmDacTestTask.h"
#include "UartEchoTask.h"

#include "cmsis_os2.h"
#include "fsl_common.h"

__WEAK bool GpioUpdateTask_Create(void)
{
    static const osThreadAttr_t gpioInputTaskAttributes = {
        .name       = "GpioInput",
        .priority   = osPriorityNormal,
        .stack_size = 512U,
    };

    return osThreadNew(GpioInputTask, NULL, &gpioInputTaskAttributes) != NULL;
}

bool UartEchoTask_Create(void)
{
    static const osThreadAttr_t uartEchoTaskAttributes = {
        .name       = "UartEcho",
        .priority   = osPriorityNormal,
        .stack_size = 1024U,
    };

    return osThreadNew(UartEchoTask, NULL, &uartEchoTaskAttributes) != NULL;
}

bool Uart1EchoTask_Create(void)
{
    static const osThreadAttr_t uart1EchoTaskAttributes = {
        .name       = "Uart1Echo",
        .priority   = osPriorityNormal,
        .stack_size = 1024U,
    };

    return osThreadNew(Uart1EchoTask, NULL, &uart1EchoTaskAttributes) != NULL;
}

bool PwmDacTestTask_Create(void)
{
    static const osThreadAttr_t pwmDacTestTaskAttributes = {
        .name       = "PwmDacTest",
        .priority   = osPriorityNormal,
        .stack_size = 512U,
    };

    return osThreadNew(PwmDacTestTask, NULL, &pwmDacTestTaskAttributes) != NULL;
}
