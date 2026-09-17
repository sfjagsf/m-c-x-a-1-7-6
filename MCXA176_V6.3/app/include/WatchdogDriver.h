/* Application-facing driver for the Config Tools generated WWDT0 instance. */
#ifndef APP_WATCHDOG_DRIVER_H_
#define APP_WATCHDOG_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool WatchdogDriver_Init(void);
bool WatchdogDriver_Refresh(void);
uint32_t WatchdogDriver_GetRemainingMs(void);
bool WatchdogDriver_WasWatchdogReset(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WATCHDOG_DRIVER_H_ */
