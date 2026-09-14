/* Temporary hardware bring-up task for PWM0_A0 and DAC0. */
#ifndef APP_PWM_DAC_TEST_TASK_H_
#define APP_PWM_DAC_TEST_TASK_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool g_pwmDacTestPassed;
extern volatile uint16_t g_pwmDacTestDacCode;

void PwmDacTestTask(void *argument);
bool PwmDacTestTask_Create(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PWM_DAC_TEST_TASK_H_ */
