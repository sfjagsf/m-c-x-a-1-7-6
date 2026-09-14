#include "PwmDriver.h"

#include "fsl_pwm.h"
#include "peripherals.h"

bool PwmDriverSetFrequencyDuty(uint32_t frequencyHz, uint8_t dutyPercent)
{
    uint32_t periodCounts;
    uint32_t duty16;
    if ((frequencyHz == 0U) || (dutyPercent > 100U))
    {
        return false;
    }

    periodCounts = FLEXPWM0_SM0_COUNTER_CLK_SOURCE_FREQ_HZ / frequencyHz;
    if ((periodCounts == 0U) || (periodCounts > UINT16_MAX))
    {
        return false;
    }

    duty16 = ((uint32_t)dutyPercent * UINT16_MAX + 50U) / 100U;

    PWM_UpdatePwmPeriodAndDutycycle(FLEXPWM0_PERIPHERAL,
                                    FLEXPWM0_SM0,
                                    FLEXPWM0_SM0_A,
                                    kPWM_EdgeAligned,
                                    (uint16_t)periodCounts,
                                    (uint16_t)duty16);

    /* VAL registers are double-buffered; LDOK applies them on the next reload. */
    PWM_SetPwmLdok(FLEXPWM0_PERIPHERAL, kPWM_Control_Module_0, true);
    PWM_StartTimer(FLEXPWM0_PERIPHERAL, kPWM_Control_Module_0);
    return true;
}

bool PwmDriverStartTestOutput(void)
{
    return PwmDriverSetFrequencyDuty(PWM_DRIVER_TEST_FREQUENCY_HZ,
                                     PWM_DRIVER_TEST_DUTY_PERCENT);
}
