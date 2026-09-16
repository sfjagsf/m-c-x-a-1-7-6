#include "I2cEepromDriver.h"

#include <string.h>

#include "fsl_gpio.h"
#include "fsl_lpi2c.h"
#include "pin_mux.h"

extern uint32_t SystemCoreClock;

#define I2C_EEPROM_SLAVE_ADDRESS (0x50U)

static volatile bool s_busy;
static i2c_eeprom_diagnostics_t s_diagnostics;

static void I2cEeprom_SetWriteEnabled(bool enabled)
{
    GPIO_PinWrite(BOARD_INITPINS_I2C3_WP_GPIO, BOARD_INITPINS_I2C3_WP_GPIO_PIN,
                  enabled ? 0U : 1U);
}

static bool I2cEeprom_IsValidRange(uint16_t address, size_t size)
{
    return size <= ((size_t)I2C_EEPROM_CAPACITY_BYTES - (size_t)address);
}

static bool I2cEeprom_TryAcquire(void)
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

static void I2cEeprom_Release(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    s_busy = false;
    EnableGlobalIRQ(irqMask);
}

static void I2cEeprom_SaveStatus(status_t status)
{
    s_diagnostics.lastDriverStatus = status;
    if (status != kStatus_Success)
    {
        s_diagnostics.errorCount++;
    }
}

static status_t I2cEeprom_Transfer(uint16_t address,
                                   lpi2c_direction_t direction,
                                   uint8_t *data,
                                   size_t size)
{
    lpi2c_master_transfer_t transfer = {
        .flags = kLPI2C_TransferDefaultFlag,
        .slaveAddress = I2C_EEPROM_SLAVE_ADDRESS,
        .direction = direction,
        .subaddress = address,
        .subaddressSize = 2U,
        .data = data,
        .dataSize = size,
    };

    return LPI2C_MasterTransferBlocking(LPI2C3, &transfer);
}

static status_t I2cEeprom_ProbeReady(void)
{
    lpi2c_master_transfer_t transfer = {
        .flags = kLPI2C_TransferDefaultFlag,
        .slaveAddress = I2C_EEPROM_SLAVE_ADDRESS,
        .direction = kLPI2C_Write,
        .subaddress = 0U,
        .subaddressSize = 0U,
        .data = NULL,
        .dataSize = 0U,
    };

    return LPI2C_MasterTransferBlocking(LPI2C3, &transfer);
}

static status_t I2cEeprom_WaitReadyLocked(uint32_t timeoutMs)
{
    status_t status;

    do
    {
        status = I2cEeprom_ProbeReady();
        if (status == kStatus_Success)
        {
            return status;
        }
        if (status != kStatus_LPI2C_Nak)
        {
            return status;
        }
        SDK_DelayAtLeastUs(1000U, SystemCoreClock);
    } while (timeoutMs-- != 0U);

    return kStatus_LPI2C_Timeout;
}

void I2cEeprom_Init(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    (void)memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    s_diagnostics.lastDriverStatus = kStatus_Success;
    s_busy = false;
    EnableGlobalIRQ(irqMask);
    I2cEeprom_SetWriteEnabled(false);
}

status_t I2cEeprom_Read(uint16_t address, uint8_t *data, size_t size)
{
    status_t status = kStatus_Success;
    size_t chunk;

    if (((data == NULL) && (size != 0U)) || !I2cEeprom_IsValidRange(address, size))
    {
        return kStatus_InvalidArgument;
    }
    if (size == 0U)
    {
        return kStatus_Success;
    }
    if (!I2cEeprom_TryAcquire())
    {
        return kStatus_Busy;
    }

    while (size != 0U)
    {
        chunk = (size > I2C_EEPROM_READ_CHUNK_SIZE) ? I2C_EEPROM_READ_CHUNK_SIZE : size;
        status = I2cEeprom_Transfer(address, kLPI2C_Read, data, chunk);
        if (status != kStatus_Success)
        {
            break;
        }
        address = (uint16_t)(address + chunk);
        data += chunk;
        size -= chunk;
    }

    if (status == kStatus_Success)
    {
        s_diagnostics.readCount++;
    }
    I2cEeprom_SaveStatus(status);
    I2cEeprom_Release();
    return status;
}

status_t I2cEeprom_Write(uint16_t address, const uint8_t *data, size_t size)
{
    status_t status = kStatus_Success;
    size_t pageRemaining;
    size_t chunk;

    if (((data == NULL) && (size != 0U)) || !I2cEeprom_IsValidRange(address, size))
    {
        return kStatus_InvalidArgument;
    }
    if (size == 0U)
    {
        return kStatus_Success;
    }
    if (!I2cEeprom_TryAcquire())
    {
        return kStatus_Busy;
    }

    I2cEeprom_SetWriteEnabled(true);
    while (size != 0U)
    {
        pageRemaining = I2C_EEPROM_PAGE_SIZE - ((size_t)address % I2C_EEPROM_PAGE_SIZE);
        chunk = (size < pageRemaining) ? size : pageRemaining;
        status = I2cEeprom_Transfer(address, kLPI2C_Write, (uint8_t *)(uintptr_t)data, chunk);
        if (status != kStatus_Success)
        {
            break;
        }
        status = I2cEeprom_WaitReadyLocked(I2C_EEPROM_WRITE_TIMEOUT_MS);
        if (status != kStatus_Success)
        {
            break;
        }
        address = (uint16_t)(address + chunk);
        data += chunk;
        size -= chunk;
    }
    I2cEeprom_SetWriteEnabled(false);

    if (status == kStatus_Success)
    {
        s_diagnostics.writeCount++;
    }
    I2cEeprom_SaveStatus(status);
    I2cEeprom_Release();
    return status;
}

status_t I2cEeprom_WaitReady(uint32_t timeoutMs)
{
    status_t status;

    if (!I2cEeprom_TryAcquire())
    {
        return kStatus_Busy;
    }
    status = I2cEeprom_WaitReadyLocked(timeoutMs);
    I2cEeprom_SaveStatus(status);
    I2cEeprom_Release();
    return status;
}

bool I2cEeprom_IsBusy(void)
{
    return s_busy;
}

void I2cEeprom_GetDiagnostics(i2c_eeprom_diagnostics_t *diagnostics)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    if (diagnostics != NULL)
    {
        *diagnostics = s_diagnostics;
    }
    EnableGlobalIRQ(irqMask);
}

void I2cEeprom_ClearDiagnostics(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    (void)memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    s_diagnostics.lastDriverStatus = kStatus_Success;
    EnableGlobalIRQ(irqMask);
}
