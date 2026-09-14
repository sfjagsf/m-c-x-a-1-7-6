#include "PwmDacTestTask.h"

#include "DacDriver.h"
#include "PwmDriver.h"
#include "cmsis_os2.h"

volatile bool g_pwmDacTestPassed;
volatile uint16_t g_pwmDacTestDacCode;

void PwmDacTestTask(void *argument)
{
    (void)argument;

    /* P3_0: 2 kHz, 50% low-active PWM. P2_2: board-level 10.000 V command. */
    g_pwmDacTestPassed = PwmDriverStartTestOutput() &&
                         DacDriverSetOutputMilliVolts(DAC_DRIVER_OUTPUT_MAX_MV);
    g_pwmDacTestDacCode = DacDriverReadCode();

    for (;;)
    {
        (void)osDelay(1000U);
    }
}
