/*
 * Allocation-free Modbus RTU master protocol core, adapted from libmodbus 3.2.0.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef MODBUS_MASTER_H_
#define MODBUS_MASTER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ModbusRtu.h"

typedef enum
{
    kModbusMasterResultOk = 0,
    kModbusMasterResultInvalidArgument = -1,
    kModbusMasterResultBadCrc = -2,
    kModbusMasterResultWrongSlave = -3,
    kModbusMasterResultWrongFunction = -4,
    kModbusMasterResultBadResponse = -5,
    kModbusMasterResultException = -6,
} modbus_master_result_t;

typedef struct
{
    uint8_t request[MODBUS_RTU_MAX_ADU_LENGTH];
    size_t requestLength;
    uint8_t response[MODBUS_RTU_MAX_ADU_LENGTH];
    size_t responseLength;
    uint16_t expectedReadQuantity;
    uint8_t exceptionCode;
    modbus_master_result_t lastResult;
} modbus_master_t;

void ModbusMaster_Init(modbus_master_t *master);

bool ModbusMaster_BuildReadCoils(modbus_master_t *master, uint8_t slave,
                                 uint16_t address, uint16_t quantity);
bool ModbusMaster_BuildReadDiscreteInputs(modbus_master_t *master, uint8_t slave,
                                          uint16_t address, uint16_t quantity);
bool ModbusMaster_BuildReadHoldingRegisters(modbus_master_t *master, uint8_t slave,
                                            uint16_t address, uint16_t quantity);
bool ModbusMaster_BuildReadInputRegisters(modbus_master_t *master, uint8_t slave,
                                          uint16_t address, uint16_t quantity);
bool ModbusMaster_BuildWriteCoil(modbus_master_t *master, uint8_t slave,
                                 uint16_t address, bool value);
bool ModbusMaster_BuildWriteRegister(modbus_master_t *master, uint8_t slave,
                                     uint16_t address, uint16_t value);
/* Coil values follow libmodbus: one byte per logical coil, zero/non-zero. */
bool ModbusMaster_BuildWriteCoils(modbus_master_t *master, uint8_t slave,
                                  uint16_t address, const uint8_t *values,
                                  uint16_t quantity);
bool ModbusMaster_BuildWriteRegisters(modbus_master_t *master, uint8_t slave,
                                      uint16_t address, const uint16_t *values,
                                      uint16_t quantity);
bool ModbusMaster_BuildMaskWriteRegister(modbus_master_t *master, uint8_t slave,
                                         uint16_t address, uint16_t andMask,
                                         uint16_t orMask);
bool ModbusMaster_BuildWriteAndReadRegisters(modbus_master_t *master, uint8_t slave,
                                             uint16_t writeAddress,
                                             const uint16_t *writeValues,
                                             uint16_t writeQuantity,
                                             uint16_t readAddress,
                                             uint16_t readQuantity);
bool ModbusMaster_BuildReportSlaveId(modbus_master_t *master, uint8_t slave);

modbus_master_result_t ModbusMaster_ParseResponse(modbus_master_t *master,
                                                  const uint8_t *response,
                                                  size_t responseLength);

/* Extraction functions return the number of values copied, or zero on mismatch. */
size_t ModbusMaster_GetBits(const modbus_master_t *master, uint8_t *values,
                            size_t capacity);
size_t ModbusMaster_GetRegisters(const modbus_master_t *master, uint16_t *values,
                                 size_t capacity);
size_t ModbusMaster_GetReportSlaveId(const modbus_master_t *master, uint8_t *data,
                                     size_t capacity);

bool ModbusMaster_SelfTest(void);

#endif /* MODBUS_MASTER_H_ */
