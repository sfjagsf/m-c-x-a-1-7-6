/*
 * DAC0 hardware access service.
 *
 * This module deliberately accepts only the DAC's native 12-bit code.  The
 * board-specific 0-10 V calibration and any laser/power policy belong in a
 * higher-level application module.
 */
#ifndef APP_DAC_DRIVER_H_
#define APP_DAC_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DAC_DRIVER_CODE_MIN (0U)
#define DAC_DRIVER_CODE_MAX (4095U)

/*
 * Board-level 0-10 V command calibration inherited from the STM32 project.
 * The DAC0 pin itself is not a 10 V pin; this assumes the board's external
 * analogue conditioning stage remains electrically equivalent.
 */
#define DAC_DRIVER_OUTPUT_MIN_MV        (0U)
#define DAC_DRIVER_OUTPUT_MAX_MV        (10000U)
#define DAC_DRIVER_OUTPUT_10V_CODE      (3896U)

/*
 * Writes one DAC0 sample and performs the configured software trigger.
 * BOARD_InitBootPeripherals() must have completed before this function is
 * called.  Returns false when code is outside the 12-bit DAC range.
 */
bool DacDriverWriteCode(uint16_t code);

/*
 * Commands the board's calibrated 0-10 V analogue-output range.
 * Returns false when the requested voltage is outside 0 to 10000 mV.
 */
bool DacDriverSetOutputMilliVolts(uint16_t millivolts);

/* Reads the last code written to the DAC DATA register. */
uint16_t DacDriverReadCode(void);

/* Safely requests the minimum raw DAC output. */
void DacDriverWriteZero(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DAC_DRIVER_H_ */
