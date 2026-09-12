/* RTC0 hardware access service. */
#ifndef APP_RTC_DRIVER_H_
#define APP_RTC_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Calendar representation used by the application layer. */
typedef struct
{
    uint16_t year;   /* 1970 through 2099 */
    uint8_t month;   /* 1 through 12 */
    uint8_t day;     /* 1 through 31 */
    uint8_t hour;    /* 0 through 23 */
    uint8_t minute;  /* 0 through 59 */
    uint8_t second;  /* 0 through 59 */
} rtc_driver_datetime_t;

/* Reads RTC0's calendar value.  BOARD_InitBootPeripherals() must be complete. */
bool RtcDriverGetDatetime(rtc_driver_datetime_t *datetime);

/*
 * Sets RTC0's calendar value.  The RTC is stopped only for the hardware write
 * and restarted before this function returns, including invalid input cases.
 */
bool RtcDriverSetDatetime(const rtc_driver_datetime_t *datetime);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTC_DRIVER_H_ */
