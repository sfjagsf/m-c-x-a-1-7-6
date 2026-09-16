#include "Pcf8563RtcDriver.h"

#include "fsl_lpi2c.h"

#define PCF8563_REG_SECONDS             (0x02U)
#define PCF8563_DATETIME_REGISTER_COUNT (7U)

static bool Pcf8563_IsBcd(uint8_t value, uint8_t maximum)
{
    const uint8_t decimal = (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));

    return ((value & 0x0FU) <= 9U) && (decimal <= maximum);
}

static uint8_t Pcf8563_BcdToDecimal(uint8_t value)
{
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t Pcf8563_DecimalToBcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static bool Pcf8563_IsDateTimeValid(const pcf8563_datetime_t *dateTime)
{
    return (dateTime != NULL) && (dateTime->year >= 2000U) && (dateTime->year <= 2099U) &&
           (dateTime->month >= 1U) && (dateTime->month <= 12U) && (dateTime->day >= 1U) &&
           (dateTime->day <= 31U) && (dateTime->weekday <= 6U) && (dateTime->hour <= 23U) &&
           (dateTime->minute <= 59U) && (dateTime->second <= 59U);
}

static status_t Pcf8563_Transfer(uint8_t startRegister,
                                 lpi2c_direction_t direction,
                                 uint8_t *data,
                                 size_t size)
{
    lpi2c_master_transfer_t transfer = {
        .flags = kLPI2C_TransferDefaultFlag,
        .slaveAddress = PCF8563_I2C_ADDRESS,
        .direction = direction,
        .subaddress = startRegister,
        .subaddressSize = 1U,
        .data = data,
        .dataSize = size,
    };

    return LPI2C_MasterTransferBlocking(LPI2C1, &transfer);
}

void Pcf8563_Init(void)
{
    /* Preserve date/time, alarm and timer registers that survive a MCU reset. */
}

status_t Pcf8563_ReadRegisters(uint8_t startRegister, uint8_t *data, size_t size)
{
    if ((data == NULL) || (size == 0U))
    {
        return kStatus_InvalidArgument;
    }

    return Pcf8563_Transfer(startRegister, kLPI2C_Read, data, size);
}

status_t Pcf8563_ReadDateTime(pcf8563_datetime_t *dateTime)
{
    uint8_t data[PCF8563_DATETIME_REGISTER_COUNT];
    status_t status;
    bool clockValid;

    if (dateTime == NULL)
    {
        return kStatus_InvalidArgument;
    }
    status = Pcf8563_ReadRegisters(PCF8563_REG_SECONDS, data, sizeof(data));
    if (status != kStatus_Success)
    {
        return status;
    }

    clockValid = (data[0] & 0x80U) == 0U;
    data[0] &= 0x7FU;
    data[2] &= 0x3FU;
    data[5] &= 0x1FU;
    if (!Pcf8563_IsBcd(data[0], 59U) || !Pcf8563_IsBcd(data[1], 59U) ||
        !Pcf8563_IsBcd(data[2], 23U) || !Pcf8563_IsBcd(data[3], 31U) ||
        (data[4] > 6U) || !Pcf8563_IsBcd(data[5], 12U) || !Pcf8563_IsBcd(data[6], 99U))
    {
        return kStatus_Fail;
    }

    dateTime->second = Pcf8563_BcdToDecimal(data[0]);
    dateTime->minute = Pcf8563_BcdToDecimal(data[1]);
    dateTime->hour = Pcf8563_BcdToDecimal(data[2]);
    dateTime->day = Pcf8563_BcdToDecimal(data[3]);
    dateTime->weekday = data[4];
    dateTime->month = Pcf8563_BcdToDecimal(data[5]);
    dateTime->year = (uint16_t)(2000U + Pcf8563_BcdToDecimal(data[6]));
    dateTime->clockValid = clockValid;
    return kStatus_Success;
}

status_t Pcf8563_SetDateTime(const pcf8563_datetime_t *dateTime)
{
    uint8_t data[PCF8563_DATETIME_REGISTER_COUNT];

    if (!Pcf8563_IsDateTimeValid(dateTime))
    {
        return kStatus_InvalidArgument;
    }

    data[0] = Pcf8563_DecimalToBcd(dateTime->second);
    data[1] = Pcf8563_DecimalToBcd(dateTime->minute);
    data[2] = Pcf8563_DecimalToBcd(dateTime->hour);
    data[3] = Pcf8563_DecimalToBcd(dateTime->day);
    data[4] = dateTime->weekday;
    data[5] = Pcf8563_DecimalToBcd(dateTime->month);
    data[6] = Pcf8563_DecimalToBcd((uint8_t)(dateTime->year - 2000U));
    return Pcf8563_Transfer(PCF8563_REG_SECONDS, kLPI2C_Write, data, sizeof(data));
}
