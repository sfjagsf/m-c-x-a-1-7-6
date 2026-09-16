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
    uint16_t year;   /* 2000 through 2099. */
    uint8_t month;   /* 1 through 12. */
    uint8_t day;     /* 1 through 31. */
    uint8_t weekday; /* Sunday is 0, Saturday is 6. */
    uint8_t hour;    /* 0 through 23. */
    uint8_t minute;  /* 0 through 59. */
    uint8_t second;  /* 0 through 59. */
    bool clockValid; /* False when the PCF8563 VL flag reports a low-voltage time loss. */
} pcf8563_datetime_t;

/* Blocking APIs: do not call from an ISR. A concurrent caller receives kStatus_Busy. */
void Pcf8563_Init(void);
status_t Pcf8563_ReadDateTime(pcf8563_datetime_t *dateTime);
status_t Pcf8563_SetDateTime(const pcf8563_datetime_t *dateTime);
/* Reads only the implemented register span 0x00 through 0x0F. */
status_t Pcf8563_ReadRegisters(uint8_t startRegister, uint8_t *data, size_t size);
bool Pcf8563_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif /* PCF8563_RTC_DRIVER_H_ */
