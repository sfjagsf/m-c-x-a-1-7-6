#include "Pcf8563RtcDriver.h"

#include "fsl_lpi2c.h"

#define PCF8563_REG_SECONDS             (0x02U)
#define PCF8563_DATETIME_REGISTER_COUNT (7U)
#define PCF8563_READ_CONSISTENCY_RETRIES (3U)

static volatile bool s_busy;

static bool Pcf8563_TryAcquire(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();
    const bool acquired = !s_busy;

    if (acquired)
    {
        s_busy = true;
    }
    EnableGlobalIRQ(irqMask);
    return acquired;
}

static void Pcf8563_Release(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    s_busy = false;
    EnableGlobalIRQ(irqMask);
}

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

static uint8_t Pcf8563_DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t daysPerMonth[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    if ((month == 2U) && ((year % 4U) == 0U))
    {
        return 29U;
    }
    return daysPerMonth[month - 1U];
}

static bool Pcf8563_IsDateTimeValid(const pcf8563_datetime_t *dateTime)
{
    if ((dateTime == NULL) || (dateTime->year < 2000U) || (dateTime->year > 2099U) ||
        (dateTime->month < 1U) || (dateTime->month > 12U) || (dateTime->day < 1U) ||
        (dateTime->weekday > 6U) || (dateTime->hour > 23U) || (dateTime->minute > 59U) ||
        (dateTime->second > 59U))
    {
        return false;
    }

    return dateTime->day <= Pcf8563_DaysInMonth(dateTime->year, dateTime->month);
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

static status_t Pcf8563_ReadRegistersLocked(uint8_t startRegister, uint8_t *data, size_t size)
{
    return Pcf8563_Transfer(startRegister, kLPI2C_Read, data, size);
}

static status_t Pcf8563_DecodeDateTime(const uint8_t data[PCF8563_DATETIME_REGISTER_COUNT],
                                       pcf8563_datetime_t *dateTime)
{
    pcf8563_datetime_t decoded;
    uint8_t seconds = data[0] & 0x7FU;
    uint8_t minutes = data[1] & 0x7FU;
    uint8_t hours = data[2] & 0x3FU;
    uint8_t days = data[3] & 0x3FU;
    uint8_t weekdays = data[4] & 0x07U;
    uint8_t months = data[5] & 0x1FU;
    uint8_t years = data[6];

    if (!Pcf8563_IsBcd(seconds, 59U) || !Pcf8563_IsBcd(minutes, 59U) || !Pcf8563_IsBcd(hours, 23U) ||
        !Pcf8563_IsBcd(days, 31U) || (weekdays > 6U) || !Pcf8563_IsBcd(months, 12U) ||
        !Pcf8563_IsBcd(years, 99U))
    {
        return kStatus_Fail;
    }

    decoded.second = Pcf8563_BcdToDecimal(seconds);
    decoded.minute = Pcf8563_BcdToDecimal(minutes);
    decoded.hour = Pcf8563_BcdToDecimal(hours);
    decoded.day = Pcf8563_BcdToDecimal(days);
    decoded.weekday = weekdays;
    decoded.month = Pcf8563_BcdToDecimal(months);
    decoded.year = (uint16_t)(2000U + Pcf8563_BcdToDecimal(years));
    decoded.clockValid = (data[0] & 0x80U) == 0U;
    if (!Pcf8563_IsDateTimeValid(&decoded))
    {
        return kStatus_Fail;
    }

    *dateTime = decoded;
    return kStatus_Success;
}

void Pcf8563_Init(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    /* Preserve date/time, alarm and timer registers that survive a MCU reset. */
    s_busy = false;
    EnableGlobalIRQ(irqMask);
}

status_t Pcf8563_ReadRegisters(uint8_t startRegister, uint8_t *data, size_t size)
{
    status_t status;

    if ((data == NULL) || (size == 0U) || (startRegister >= PCF8563_REGISTER_COUNT) ||
        (size > ((size_t)PCF8563_REGISTER_COUNT - (size_t)startRegister)))
    {
        return kStatus_InvalidArgument;
    }
    if (!Pcf8563_TryAcquire())
    {
        return kStatus_Busy;
    }

    status = Pcf8563_ReadRegistersLocked(startRegister, data, size);
    Pcf8563_Release();
    return status;
}

status_t Pcf8563_ReadDateTime(pcf8563_datetime_t *dateTime)
{
    uint8_t data[PCF8563_DATETIME_REGISTER_COUNT];
    uint8_t secondsAfterRead;
    status_t status = kStatus_Fail;
    bool consistent = false;
    uint32_t attempt;

    if (dateTime == NULL)
    {
        return kStatus_InvalidArgument;
    }
    if (!Pcf8563_TryAcquire())
    {
        return kStatus_Busy;
    }

    for (attempt = 0U; attempt < PCF8563_READ_CONSISTENCY_RETRIES; attempt++)
    {
        status = Pcf8563_ReadRegistersLocked(PCF8563_REG_SECONDS, data, sizeof(data));
        if (status != kStatus_Success)
        {
            break;
        }
        status = Pcf8563_ReadRegistersLocked(PCF8563_REG_SECONDS, &secondsAfterRead, sizeof(secondsAfterRead));
        if (status != kStatus_Success)
        {
            break;
        }
        if (secondsAfterRead == data[0])
        {
            status = Pcf8563_DecodeDateTime(data, dateTime);
            consistent = true;
            break;
        }
    }

    if (!consistent && (status == kStatus_Success))
    {
        status = kStatus_Fail;
    }
    Pcf8563_Release();
    return status;
}

status_t Pcf8563_SetDateTime(const pcf8563_datetime_t *dateTime)
{
    uint8_t data[PCF8563_DATETIME_REGISTER_COUNT];
    status_t status;

    if (!Pcf8563_IsDateTimeValid(dateTime))
    {
        return kStatus_InvalidArgument;
    }
    if (!Pcf8563_TryAcquire())
    {
        return kStatus_Busy;
    }

    data[0] = Pcf8563_DecimalToBcd(dateTime->second);
    data[1] = Pcf8563_DecimalToBcd(dateTime->minute);
    data[2] = Pcf8563_DecimalToBcd(dateTime->hour);
    data[3] = Pcf8563_DecimalToBcd(dateTime->day);
    data[4] = dateTime->weekday;
    data[5] = Pcf8563_DecimalToBcd(dateTime->month);
    data[6] = Pcf8563_DecimalToBcd((uint8_t)(dateTime->year - 2000U));
    status = Pcf8563_Transfer(PCF8563_REG_SECONDS, kLPI2C_Write, data, sizeof(data));
    Pcf8563_Release();
    return status;
}

bool Pcf8563_IsBusy(void)
{
    return s_busy;
}
