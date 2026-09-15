#include "RtcDriver.h"

#include "fsl_rtc.h"
#include "peripherals.h"

static bool RtcDriverIsLeapYear(uint16_t year)
{
    return ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
}

static bool RtcDriverIsValidDatetime(const rtc_driver_datetime_t *datetime)
{
    static const uint8_t daysInMonth[] = {31U, 28U, 31U, 30U, 31U, 30U,
                                          31U, 31U, 30U, 31U, 30U, 31U};
    uint8_t maximumDay;

    if ((datetime == NULL) || (datetime->year < 1970U) || (datetime->year > 2099U) ||
        (datetime->month < 1U) || (datetime->month > 12U) || (datetime->hour > 23U) ||
        (datetime->minute > 59U) || (datetime->second > 59U))
    {
        return false;
    }
    maximumDay = daysInMonth[datetime->month - 1U];
    if ((datetime->month == 2U) && RtcDriverIsLeapYear(datetime->year))
    {
        maximumDay++;
    }
    return (datetime->day >= 1U) && (datetime->day <= maximumDay);
}

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

    if (!RtcDriverIsValidDatetime(datetime))
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
