/* Application-facing driver for the Config Tools generated WWDT0 instance. */
#ifndef APP_WATCHDOG_DRIVER_H_
#define APP_WATCHDOG_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Verifies that BOARD_InitBootPeripherals() has started WWDT0 in reset mode.
 * Call once immediately after board peripheral initialization.
 */
bool WatchdogDriver_Init(void);

/*
 * Feed WWDT0 using the required 0xAA, 0x55 sequence. Call this only after
 * the caller has confirmed the subsystem(s) it represents are healthy.
 */
bool WatchdogDriver_Refresh(void);

/* Approximate number of milliseconds remaining before a watchdog timeout. */
uint32_t WatchdogDriver_GetRemainingMs(void);

/* True if the reset source captured at initialization was WWDT0. */
bool WatchdogDriver_WasWatchdogReset(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WATCHDOG_DRIVER_H_ */
