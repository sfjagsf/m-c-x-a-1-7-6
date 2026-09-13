/*
 * Modbus RTU master request/confirmation logic adapted from libmodbus 3.2.0
 * modbus.c and modbus-rtu.c.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../Inc/ModbusMaster.h"

#include <string.h>

#include "Crc16Driver.h"

#define FC_READ_COILS               (0x01U)
#define FC_READ_DISCRETE_INPUTS     (0x02U)
#define FC_READ_HOLDING_REGISTERS   (0x03U)
#define FC_READ_INPUT_REGISTERS     (0x04U)
#define FC_WRITE_SINGLE_COIL        (0x05U)
#define FC_WRITE_SINGLE_REGISTER    (0x06U)
#define FC_WRITE_MULTIPLE_COILS     (0x0FU)
#define FC_WRITE_MULTIPLE_REGISTERS (0x10U)
#define FC_REPORT_SLAVE_ID          (0x11U)
#define FC_MASK_WRITE_REGISTER      (0x16U)
#define FC_WRITE_AND_READ_REGISTERS (0x17U)

static void PutU16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value >> 8U);
    destination[1] = (uint8_t)value;
}

static bool SlaveValid(uint8_t slave)
{
    return slave <= 247U;
}

static bool UnicastSlaveValid(uint8_t slave)
{
    return (slave >= 1U) && SlaveValid(slave);
}

static bool FinalizeRequest(modbus_master_t *master, size_t payloadLength,
                            uint16_t expectedReadQuantity)
{
    uint16_t crc;

    if ((master == NULL) || (payloadLength > (MODBUS_RTU_MAX_ADU_LENGTH - 2U)) ||
        !Crc16Driver_Calculate(master->request, payloadLength, &crc))
    {
        return false;
    }
    master->request[payloadLength] = (uint8_t)crc;
    master->request[payloadLength + 1U] = (uint8_t)(crc >> 8U);
    master->requestLength = payloadLength + 2U;
    master->responseLength = 0U;
    master->expectedReadQuantity = expectedReadQuantity;
    master->exceptionCode = 0U;
    master->lastResult = kModbusMasterResultOk;
    return true;
}

static bool BuildBasis(modbus_master_t *master, uint8_t slave, uint8_t function,
                       uint16_t address, uint16_t value)
{
    if ((master == NULL) || !SlaveValid(slave))
    {
        return false;
    }
    master->request[0] = slave;
    master->request[1] = function;
    PutU16(&master->request[2], address);
    PutU16(&master->request[4], value);
    return true;
}

static bool BuildRead(modbus_master_t *master, uint8_t slave, uint8_t function,
                      uint16_t address, uint16_t quantity, uint16_t maximum)
{
    return UnicastSlaveValid(slave) && (quantity != 0U) && (quantity <= maximum) &&
           BuildBasis(master, slave, function, address, quantity) &&
           FinalizeRequest(master, 6U, quantity);
}

void ModbusMaster_Init(modbus_master_t *master)
{
    if (master != NULL)
    {
        (void)memset(master, 0, sizeof(*master));
    }
}

bool ModbusMaster_BuildReadCoils(modbus_master_t *master, uint8_t slave,
                                 uint16_t address, uint16_t quantity)
{
    return BuildRead(master, slave, FC_READ_COILS, address, quantity,
                     MODBUS_RTU_MAX_READ_BITS);
}

bool ModbusMaster_BuildReadDiscreteInputs(modbus_master_t *master, uint8_t slave,
                                          uint16_t address, uint16_t quantity)
{
    return BuildRead(master, slave, FC_READ_DISCRETE_INPUTS, address, quantity,
                     MODBUS_RTU_MAX_READ_BITS);
}

bool ModbusMaster_BuildReadHoldingRegisters(modbus_master_t *master, uint8_t slave,
                                            uint16_t address, uint16_t quantity)
{
    return BuildRead(master, slave, FC_READ_HOLDING_REGISTERS, address, quantity,
                     MODBUS_RTU_MAX_READ_REGISTERS);
}

bool ModbusMaster_BuildReadInputRegisters(modbus_master_t *master, uint8_t slave,
                                          uint16_t address, uint16_t quantity)
{
    return BuildRead(master, slave, FC_READ_INPUT_REGISTERS, address, quantity,
                     MODBUS_RTU_MAX_READ_REGISTERS);
}

bool ModbusMaster_BuildWriteCoil(modbus_master_t *master, uint8_t slave,
                                 uint16_t address, bool value)
{
    return BuildBasis(master, slave, FC_WRITE_SINGLE_COIL, address,
                      value ? 0xFF00U : 0U) && FinalizeRequest(master, 6U, 1U);
}

bool ModbusMaster_BuildWriteRegister(modbus_master_t *master, uint8_t slave,
                                     uint16_t address, uint16_t value)
{
    return BuildBasis(master, slave, FC_WRITE_SINGLE_REGISTER, address, value) &&
           FinalizeRequest(master, 6U, 1U);
}

bool ModbusMaster_BuildWriteCoils(modbus_master_t *master, uint8_t slave,
                                  uint16_t address, const uint8_t *values,
                                  uint16_t quantity)
{
    size_t byteCount;
    uint16_t bit;

    if ((values == NULL) || (quantity == 0U) ||
        (quantity > MODBUS_RTU_MAX_WRITE_BITS) ||
        !BuildBasis(master, slave, FC_WRITE_MULTIPLE_COILS, address, quantity))
    {
        return false;
    }
    byteCount = ((size_t)quantity + 7U) / 8U;
    master->request[6] = (uint8_t)byteCount;
    (void)memset(&master->request[7], 0, byteCount);
    for (bit = 0U; bit < quantity; bit++)
    {
        if (values[bit] != 0U)
        {
            master->request[7U + (bit / 8U)] |= (uint8_t)(1U << (bit % 8U));
        }
    }
    return FinalizeRequest(master, 7U + byteCount, quantity);
}

bool ModbusMaster_BuildWriteRegisters(modbus_master_t *master, uint8_t slave,
                                      uint16_t address, const uint16_t *values,
                                      uint16_t quantity)
{
    uint16_t index;

    if ((values == NULL) || (quantity == 0U) ||
        (quantity > MODBUS_RTU_MAX_WRITE_REGISTERS) ||
        !BuildBasis(master, slave, FC_WRITE_MULTIPLE_REGISTERS, address, quantity))
    {
        return false;
    }
    master->request[6] = (uint8_t)(quantity * 2U);
    for (index = 0U; index < quantity; index++)
    {
        PutU16(&master->request[7U + ((size_t)index * 2U)], values[index]);
    }
    return FinalizeRequest(master, 7U + ((size_t)quantity * 2U), quantity);
}

bool ModbusMaster_BuildMaskWriteRegister(modbus_master_t *master, uint8_t slave,
                                         uint16_t address, uint16_t andMask,
                                         uint16_t orMask)
{
    if ((master == NULL) || !SlaveValid(slave))
    {
        return false;
    }
    master->request[0] = slave;
    master->request[1] = FC_MASK_WRITE_REGISTER;
    PutU16(&master->request[2], address);
    PutU16(&master->request[4], andMask);
    PutU16(&master->request[6], orMask);
    return FinalizeRequest(master, 8U, 1U);
}

bool ModbusMaster_BuildWriteAndReadRegisters(modbus_master_t *master, uint8_t slave,
                                             uint16_t writeAddress,
                                             const uint16_t *writeValues,
                                             uint16_t writeQuantity,
                                             uint16_t readAddress,
                                             uint16_t readQuantity)
{
    uint16_t index;

    if ((master == NULL) || (writeValues == NULL) || !UnicastSlaveValid(slave) ||
        (writeQuantity == 0U) || (writeQuantity > MODBUS_RTU_MAX_RW_WRITE_REGS) ||
        (readQuantity == 0U) || (readQuantity > MODBUS_RTU_MAX_READ_REGISTERS))
    {
        return false;
    }
    master->request[0] = slave;
    master->request[1] = FC_WRITE_AND_READ_REGISTERS;
    PutU16(&master->request[2], readAddress);
    PutU16(&master->request[4], readQuantity);
    PutU16(&master->request[6], writeAddress);
    PutU16(&master->request[8], writeQuantity);
    master->request[10] = (uint8_t)(writeQuantity * 2U);
    for (index = 0U; index < writeQuantity; index++)
    {
        PutU16(&master->request[11U + ((size_t)index * 2U)], writeValues[index]);
    }
    return FinalizeRequest(master, 11U + ((size_t)writeQuantity * 2U), readQuantity);
}

bool ModbusMaster_BuildReportSlaveId(modbus_master_t *master, uint8_t slave)
{
    if ((master == NULL) || !UnicastSlaveValid(slave))
    {
        return false;
    }
    master->request[0] = slave;
    master->request[1] = FC_REPORT_SLAVE_ID;
    return FinalizeRequest(master, 2U, 0U);
}

static bool ResponseLengthValid(const modbus_master_t *master,
                                const uint8_t *response, size_t length)
{
    const uint8_t function = master->request[1];

    if ((function <= FC_READ_INPUT_REGISTERS) ||
        (function == FC_REPORT_SLAVE_ID) ||
        (function == FC_WRITE_AND_READ_REGISTERS))
    {
        return (length >= 5U) && (length == ((size_t)response[2] + 5U));
    }
    if ((function == FC_WRITE_SINGLE_COIL) ||
        (function == FC_WRITE_SINGLE_REGISTER) ||
        (function == FC_WRITE_MULTIPLE_COILS) ||
        (function == FC_WRITE_MULTIPLE_REGISTERS))
    {
        return length == 8U;
    }
    return (function == FC_MASK_WRITE_REGISTER) && (length == 10U);
}

modbus_master_result_t ModbusMaster_ParseResponse(modbus_master_t *master,
                                                  const uint8_t *response,
                                                  size_t responseLength)
{
    uint8_t function;
    size_t expectedBytes;

    if ((master == NULL) || (response == NULL) || (master->requestLength < 4U))
    {
        return kModbusMasterResultInvalidArgument;
    }
    master->responseLength = 0U;
    master->exceptionCode = 0U;
    if ((responseLength < 5U) || (responseLength > MODBUS_RTU_MAX_ADU_LENGTH))
    {
        master->lastResult = kModbusMasterResultBadResponse;
        return master->lastResult;
    }
    if (!Crc16Driver_VerifyFrame(response, responseLength))
    {
        master->lastResult = kModbusMasterResultBadCrc;
        return master->lastResult;
    }
    if ((master->request[0] != MODBUS_RTU_BROADCAST_ADDRESS) &&
        (response[0] != master->request[0]))
    {
        master->lastResult = kModbusMasterResultWrongSlave;
        return master->lastResult;
    }
    function = response[1];
    if (function == (uint8_t)(master->request[1] | 0x80U))
    {
        if (responseLength != 5U)
        {
            master->lastResult = kModbusMasterResultBadResponse;
            return master->lastResult;
        }
        master->exceptionCode = response[2];
        master->lastResult = kModbusMasterResultException;
        return master->lastResult;
    }
    if (function != master->request[1])
    {
        master->lastResult = kModbusMasterResultWrongFunction;
        return master->lastResult;
    }
    if (!ResponseLengthValid(master, response, responseLength))
    {
        master->lastResult = kModbusMasterResultBadResponse;
        return master->lastResult;
    }

    if ((function == FC_READ_COILS) || (function == FC_READ_DISCRETE_INPUTS))
    {
        expectedBytes = ((size_t)master->expectedReadQuantity + 7U) / 8U;
        if (response[2] != expectedBytes)
        {
            master->lastResult = kModbusMasterResultBadResponse;
            return master->lastResult;
        }
    }
    else if ((function == FC_READ_HOLDING_REGISTERS) ||
             (function == FC_READ_INPUT_REGISTERS) ||
             (function == FC_WRITE_AND_READ_REGISTERS))
    {
        expectedBytes = (size_t)master->expectedReadQuantity * 2U;
        if (response[2] != expectedBytes)
        {
            master->lastResult = kModbusMasterResultBadResponse;
            return master->lastResult;
        }
    }
    else if ((function == FC_WRITE_SINGLE_COIL) ||
             (function == FC_WRITE_SINGLE_REGISTER))
    {
        if (memcmp(&response[2], &master->request[2], 4U) != 0)
        {
            master->lastResult = kModbusMasterResultBadResponse;
            return master->lastResult;
        }
    }
    else if ((function == FC_WRITE_MULTIPLE_COILS) ||
             (function == FC_WRITE_MULTIPLE_REGISTERS))
    {
        if (memcmp(&response[2], &master->request[2], 4U) != 0)
        {
            master->lastResult = kModbusMasterResultBadResponse;
            return master->lastResult;
        }
    }
    else if ((function == FC_MASK_WRITE_REGISTER) &&
             (memcmp(&response[2], &master->request[2], 6U) != 0))
    {
        master->lastResult = kModbusMasterResultBadResponse;
        return master->lastResult;
    }

    (void)memcpy(master->response, response, responseLength);
    master->responseLength = responseLength;
    master->lastResult = kModbusMasterResultOk;
    return master->lastResult;
}

size_t ModbusMaster_GetBits(const modbus_master_t *master, uint8_t *values,
                            size_t capacity)
{
    size_t index;
    size_t count;

    if ((master == NULL) || (values == NULL) ||
        (master->lastResult != kModbusMasterResultOk) ||
        ((master->request[1] != FC_READ_COILS) &&
         (master->request[1] != FC_READ_DISCRETE_INPUTS)))
    {
        return 0U;
    }
    count = master->expectedReadQuantity;
    if (capacity < count)
    {
        return 0U;
    }
    for (index = 0U; index < count; index++)
    {
        values[index] = (uint8_t)((master->response[3U + (index / 8U)] >>
                                   (index % 8U)) & 1U);
    }
    return count;
}

size_t ModbusMaster_GetRegisters(const modbus_master_t *master, uint16_t *values,
                                 size_t capacity)
{
    size_t index;
    size_t count;

    if ((master == NULL) || (values == NULL) ||
        (master->lastResult != kModbusMasterResultOk) ||
        ((master->request[1] != FC_READ_HOLDING_REGISTERS) &&
         (master->request[1] != FC_READ_INPUT_REGISTERS) &&
         (master->request[1] != FC_WRITE_AND_READ_REGISTERS)))
    {
        return 0U;
    }
    count = master->expectedReadQuantity;
    if (capacity < count)
    {
        return 0U;
    }
    for (index = 0U; index < count; index++)
    {
        values[index] = (uint16_t)(((uint16_t)master->response[3U + index * 2U] << 8U) |
                                   master->response[4U + index * 2U]);
    }
    return count;
}

size_t ModbusMaster_GetReportSlaveId(const modbus_master_t *master, uint8_t *data,
                                     size_t capacity)
{
    size_t count;

    if ((master == NULL) || (data == NULL) ||
        (master->lastResult != kModbusMasterResultOk) ||
        (master->request[1] != FC_REPORT_SLAVE_ID))
    {
        return 0U;
    }
    count = master->response[2];
    if (capacity < count)
    {
        return 0U;
    }
    (void)memcpy(data, &master->response[3], count);
    return count;
}

bool ModbusMaster_SelfTest(void)
{
    static const uint8_t expectedRequest[] = {0x01U, 0x03U, 0x00U, 0x00U,
                                              0x00U, 0x01U, 0x84U, 0x0AU};
    static const uint8_t response[] = {0x01U, 0x03U, 0x02U, 0x12U,
                                       0x34U, 0xB5U, 0x33U};
    modbus_master_t master;
    uint16_t value;

    ModbusMaster_Init(&master);
    return ModbusMaster_BuildReadHoldingRegisters(&master, 1U, 0U, 1U) &&
           (master.requestLength == sizeof(expectedRequest)) &&
           (memcmp(master.request, expectedRequest, sizeof(expectedRequest)) == 0) &&
           (ModbusMaster_ParseResponse(&master, response, sizeof(response)) ==
            kModbusMasterResultOk) &&
           (ModbusMaster_GetRegisters(&master, &value, 1U) == 1U) &&
           (value == 0x1234U);
}
