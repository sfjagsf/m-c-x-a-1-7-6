/* P3_0 / FLEXPWM0_SM0_A0 board PWM service. */
#ifndef APP_PWM_DRIVER_H_
#define APP_PWM_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PWM_DRIVER_TEST_FREQUENCY_HZ (3000U)
#define PWM_DRIVER_TEST_DUTY_PERCENT (50U)

/*
 * Updates the PWM0_A0 period and active (low) pulse width.
 * BOARD_InitBootPeripherals() must have completed before calling this API.
 * Valid with the generated divide-by-2 clock: 1374 Hz to 90 kHz.
 */
bool PwmDriverSetFrequencyDuty(uint32_t frequencyHz, uint8_t dutyPercent);

/* Starts the requested bring-up waveform: 2 kHz and 50% active-low duty. */
bool PwmDriverStartTestOutput(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PWM_DRIVER_H_ */
