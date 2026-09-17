#include "WatchdogDriver.h"

#include "fsl_device_registers.h"
#include "fsl_wwdt.h"
#include "peripherals.h"

/* Config Tools supplies WWDT0 at 1 MHz; hardware counts at WDCLK / 4. */
#define WATCHDOG_DRIVER_COUNTER_HZ       (250000U)
#define WATCHDOG_DRIVER_MILLISECONDS_SEC (1000U)

static bool s_initialized;
static uint32_t s_resetStatus;

bool WatchdogDriver_Init(void)
{
    const uint32_t requiredMode = WWDT_MOD_WDEN_MASK | WWDT_MOD_WDRESET_MASK;

    /* Capture the hardware reset cause before another reset can replace it. */
    s_resetStatus = CMC->SRS;

    /*
     * This driver is tied to the generated reset-mode, non-windowed, 7-second
     * configuration. Refuse to feed if Config Tools is changed incompatibly.
     */
    if (!WWDT0_config.enableWwdt || !WWDT0_config.enableWatchdogReset ||
        (WWDT0_config.windowValue != 0xFFFFFFU) ||
        ((WWDT0->MOD & requiredMode) != requiredMode))
    {
        return false;
    }

    s_initialized = true;
    return WatchdogDriver_Refresh();
}

bool WatchdogDriver_Refresh(void)
{
    if (!s_initialized)
    {
        return false;
    }

    /* SDK protects the two writes from interrupt interleaving. */
    WWDT_Refresh(WWDT0);
    return true;
}

uint32_t WatchdogDriver_GetRemainingMs(void)
{
    const uint32_t ticks = WWDT0->TV & WWDT_TV_COUNT_MASK;

    return (uint32_t)(((uint64_t)ticks * WATCHDOG_DRIVER_MILLISECONDS_SEC) /
                      WATCHDOG_DRIVER_COUNTER_HZ);
}

bool WatchdogDriver_WasWatchdogReset(void)
{
    return (s_resetStatus & CMC_SRS_WWDT0_MASK) != 0U;
}
