#include "RtcDriver.h"

#include "fsl_rtc.h"
#include "peripherals.h"

static void RtcDriverToSdk(const rtc_driver_datetime_t *source, rtc_datetime_t *destination)
{
    destination->year   = source->year;
    destination->month  = source->month;
    destination->day    = source->day;
    destination->hour   = source->hour;
    destination->minute = source->minute;
    destination->second = source->second;
}

static void RtcDriverFromSdk(const rtc_datetime_t *source, rtc_driver_datetime_t *destination)
{
    destination->year   = source->year;
    destination->month  = source->month;
    destination->day    = source->day;
    destination->hour   = source->hour;
    destination->minute = source->minute;
    destination->second = source->second;
}

bool RtcDriverGetDatetime(rtc_driver_datetime_t *datetime)
{
    rtc_datetime_t sdkDatetime;

    if (datetime == NULL)
    {
        return false;
    }

    RTC_GetDatetime(RTC0_PERIPHERAL, &sdkDatetime);
    RtcDriverFromSdk(&sdkDatetime, datetime);
    return true;
}

bool RtcDriverSetDatetime(const rtc_driver_datetime_t *datetime)
{
    rtc_datetime_t sdkDatetime;
    status_t status;

    if (datetime == NULL)
    {
        return false;
    }

    RtcDriverToSdk(datetime, &sdkDatetime);

    /* The SDK requires TSR to be written while the counter is stopped. */
    RTC_StopTimer(RTC0_PERIPHERAL);
    status = RTC_SetDatetime(RTC0_PERIPHERAL, &sdkDatetime);
    RTC_StartTimer(RTC0_PERIPHERAL);

    return status == kStatus_Success;
}
