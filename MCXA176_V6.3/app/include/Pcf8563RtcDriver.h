/* PCF8563 RTC driver on the board's LPI2C1 bus. */
#ifndef PCF8563_RTC_DRIVER_H_
#define PCF8563_RTC_DRIVER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCF8563_I2C_ADDRESS (0x51U)
#define PCF8563_REGISTER_COUNT (16U)

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool clockValid;
} pcf8563_datetime_t;

void Pcf8563_Init(void);
status_t Pcf8563_ReadDateTime(pcf8563_datetime_t *dateTime);
status_t Pcf8563_SetDateTime(const pcf8563_datetime_t *dateTime);
status_t Pcf8563_ReadRegisters(uint8_t startRegister, uint8_t *data, size_t size);
bool Pcf8563_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif /* PCF8563_RTC_DRIVER_H_ */
