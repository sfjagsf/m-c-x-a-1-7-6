/* 24C256 EEPROM driver on the board's LPI2C3 bus. */
#ifndef I2C_EEPROM_DRIVER_H_
#define I2C_EEPROM_DRIVER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_EEPROM_CAPACITY_BYTES   (32768U)
#define I2C_EEPROM_PAGE_SIZE        (64U)
#define I2C_EEPROM_READ_CHUNK_SIZE  (1024U)
#define I2C_EEPROM_WRITE_TIMEOUT_MS (10U)

typedef struct
{
    uint32_t readCount;
    uint32_t writeCount;
    uint32_t errorCount;
    status_t lastDriverStatus;
} i2c_eeprom_diagnostics_t;

void I2cEeprom_Init(void);
status_t I2cEeprom_Read(uint16_t address, uint8_t *data, size_t size);
status_t I2cEeprom_Write(uint16_t address, const uint8_t *data, size_t size);
status_t I2cEeprom_WaitReady(uint32_t timeoutMs);
bool I2cEeprom_IsBusy(void);
void I2cEeprom_GetDiagnostics(i2c_eeprom_diagnostics_t *diagnostics);
void I2cEeprom_ClearDiagnostics(void);

#ifdef __cplusplus
}
#endif

#endif /* I2C_EEPROM_DRIVER_H_ */
