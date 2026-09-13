/*
 * Embedded, allocation-free Modbus RTU server core.
 * Protocol behavior is adapted from libmodbus 3.2.0.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../Inc/ModbusRtu.h"

#include <string.h>

#include "Crc16Driver.h"

#define MODBUS_FC_READ_COILS                (0x01U)
#define MODBUS_FC_READ_DISCRETE_INPUTS      (0x02U)
#define MODBUS_FC_READ_HOLDING_REGISTERS    (0x03U)
#define MODBUS_FC_READ_INPUT_REGISTERS      (0x04U)
#define MODBUS_FC_WRITE_SINGLE_COIL         (0x05U)
#define MODBUS_FC_WRITE_SINGLE_REGISTER     (0x06U)
#define MODBUS_FC_WRITE_MULTIPLE_COILS      (0x0FU)
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS  (0x10U)
#define MODBUS_FC_REPORT_SLAVE_ID            (0x11U)
#define MODBUS_FC_MASK_WRITE_REGISTER       (0x16U)
#define MODBUS_FC_WRITE_AND_READ_REGISTERS  (0x17U)
#define MODBUS_REPORT_SLAVE_ID               (180U)

static uint16_t ModbusRtu_GetU16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static void ModbusRtu_PutU16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8U);
    data[1] = (uint8_t)value;
}

static bool ModbusRtu_MappingValid(const modbus_mapping_t *mapping)
{
    return (mapping != NULL) &&
           ((mapping->coilCount == 0U) || (mapping->coils != NULL)) &&
           ((mapping->discreteInputCount == 0U) || (mapping->discreteInputs != NULL)) &&
           ((mapping->holdingRegisterCount == 0U) ||
            (mapping->holdingRegisters != NULL)) &&
           ((mapping->inputRegisterCount == 0U) || (mapping->inputRegisters != NULL));
}

static bool ModbusRtu_GetMappingIndex(uint16_t mappingStart, uint16_t mappingCount,
                                      uint16_t address, uint16_t quantity,
                                      uint16_t *mappingIndex)
{
    const uint32_t first = address;
    const uint32_t end = first + quantity;
    const uint32_t mapFirst = mappingStart;
    const uint32_t mapEnd = mapFirst + mappingCount;

    if ((mappingIndex == NULL) || (quantity == 0U) || (first < mapFirst) ||
        (end > mapEnd))
    {
        return false;
    }
    *mappingIndex = (uint16_t)(first - mapFirst);
    return true;
}

static modbus_result_t ModbusRtu_AppendCrc(uint8_t *response, size_t responseCapacity,
                                           size_t payloadLength, size_t *responseLength)
{
    uint16_t crc;

    if ((payloadLength > (MODBUS_RTU_MAX_ADU_LENGTH - 2U)) ||
        (responseCapacity < (payloadLength + 2U)) ||
        !Crc16Driver_Calculate(response, payloadLength, &crc))
    {
        return kModbusResultResponseTooLarge;
    }
    response[payloadLength] = (uint8_t)crc;
    response[payloadLength + 1U] = (uint8_t)(crc >> 8U);
    *responseLength = payloadLength + 2U;
    return kModbusResultResponseReady;
}

static modbus_result_t ModbusRtu_BuildException(modbus_rtu_server_t *server,
                                                 uint8_t function,
                                                 modbus_exception_t exception,
                                                 bool broadcast, uint8_t *response,
                                                 size_t responseCapacity,
                                                 size_t *responseLength)
{
    modbus_result_t result;

    if (exception == kModbusExceptionIllegalDataValue)
    {
        server->diagnostics.malformedFrames++;
    }
    if (broadcast)
    {
        server->diagnostics.broadcastFrames++;
        return kModbusResultBroadcastProcessed;
    }
    if (responseCapacity < 5U)
    {
        return kModbusResultResponseTooLarge;
    }
    response[0] = server->slaveAddress;
    response[1] = function | 0x80U;
    response[2] = (uint8_t)exception;
    server->diagnostics.exceptionResponses++;
    result = ModbusRtu_AppendCrc(response, responseCapacity, 3U, responseLength);
    if (result == kModbusResultResponseReady)
    {
        server->diagnostics.repliedFrames++;
    }
    return result;
}

static modbus_result_t ModbusRtu_Finalize(modbus_rtu_server_t *server, bool broadcast,
                                          uint8_t *response, size_t responseCapacity,
                                          size_t payloadLength, size_t *responseLength)
{
    modbus_result_t result;

    if (broadcast)
    {
        server->diagnostics.broadcastFrames++;
        return kModbusResultBroadcastProcessed;
    }
    result = ModbusRtu_AppendCrc(response, responseCapacity, payloadLength, responseLength);
    if (result == kModbusResultResponseReady)
    {
        server->diagnostics.repliedFrames++;
    }
    return result;
}

static modbus_result_t ModbusRtu_ReadBits(modbus_rtu_server_t *server,
                                          const uint8_t *request, size_t requestLength,
                                          bool inputs, bool broadcast, uint8_t *response,
                                          size_t responseCapacity, size_t *responseLength)
{
    uint16_t address;
    uint16_t quantity;
    const uint8_t *table = inputs ? server->mapping.discreteInputs : server->mapping.coils;
    const uint16_t start = inputs ? server->mapping.discreteInputStart :
                                    server->mapping.coilStart;
    const uint16_t count = inputs ? server->mapping.discreteInputCount :
                                    server->mapping.coilCount;
    uint16_t index;
    size_t byteCount;
    uint16_t bit;

    if (requestLength != 8U)
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    address = ModbusRtu_GetU16(&request[2]);
    quantity = ModbusRtu_GetU16(&request[4]);
    if ((quantity == 0U) || (quantity > MODBUS_RTU_MAX_READ_BITS))
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    if (!ModbusRtu_GetMappingIndex(start, count, address, quantity, &index))
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataAddress, broadcast,
                                        response, responseCapacity, responseLength);
    }

    byteCount = ((size_t)quantity + 7U) / 8U;
    if (responseCapacity < (byteCount + 5U))
    {
        return kModbusResultResponseTooLarge;
    }
    response[0] = server->slaveAddress;
    response[1] = request[1];
    response[2] = (uint8_t)byteCount;
    (void)memset(&response[3], 0, byteCount);
    for (bit = 0U; bit < quantity; bit++)
    {
        if (table[index + bit] != 0U)
        {
            response[3U + (bit / 8U)] |= (uint8_t)(1U << (bit % 8U));
        }
    }
    return ModbusRtu_Finalize(server, broadcast, response, responseCapacity,
                              3U + byteCount, responseLength);
}

static modbus_result_t ModbusRtu_ReadRegisters(modbus_rtu_server_t *server,
                                               const uint8_t *request,
                                               size_t requestLength, bool inputs,
                                               bool broadcast, uint8_t *response,
                                               size_t responseCapacity,
                                               size_t *responseLength)
{
    uint16_t address;
    uint16_t quantity;
    const uint16_t *table = inputs ? server->mapping.inputRegisters :
                                     server->mapping.holdingRegisters;
    const uint16_t start = inputs ? server->mapping.inputRegisterStart :
                                    server->mapping.holdingRegisterStart;
    const uint16_t count = inputs ? server->mapping.inputRegisterCount :
                                    server->mapping.holdingRegisterCount;
    uint16_t index;
    uint16_t item;
    size_t byteCount;

    if (requestLength != 8U)
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    address = ModbusRtu_GetU16(&request[2]);
    quantity = ModbusRtu_GetU16(&request[4]);
    byteCount = (size_t)quantity * 2U;
    if ((quantity == 0U) || (quantity > MODBUS_RTU_MAX_READ_REGISTERS))
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    if (!ModbusRtu_GetMappingIndex(start, count, address, quantity, &index))
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataAddress, broadcast,
                                        response, responseCapacity, responseLength);
    }
    if (responseCapacity < (byteCount + 5U))
    {
        return kModbusResultResponseTooLarge;
    }
    response[0] = server->slaveAddress;
    response[1] = request[1];
    response[2] = (uint8_t)byteCount;
    for (item = 0U; item < quantity; item++)
    {
        ModbusRtu_PutU16(&response[3U + ((size_t)item * 2U)], table[index + item]);
    }
    return ModbusRtu_Finalize(server, broadcast, response, responseCapacity,
                              3U + byteCount, responseLength);
}

static modbus_result_t ModbusRtu_WriteSingle(modbus_rtu_server_t *server,
                                             const uint8_t *request,
                                             size_t requestLength, bool coil,
                                             bool broadcast, uint8_t *response,
                                             size_t responseCapacity,
                                             size_t *responseLength)
{
    uint16_t address;
    uint16_t value;
    uint16_t index;

    if (requestLength != 8U)
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    address = ModbusRtu_GetU16(&request[2]);
    value = ModbusRtu_GetU16(&request[4]);
    if (coil)
    {
        if ((value != 0xFF00U) && (value != 0x0000U))
        {
            return ModbusRtu_BuildException(server, request[1],
                                            kModbusExceptionIllegalDataValue, broadcast,
                                            response, responseCapacity, responseLength);
        }
        if (!ModbusRtu_GetMappingIndex(server->mapping.coilStart,
                                       server->mapping.coilCount, address, 1U, &index))
        {
            return ModbusRtu_BuildException(server, request[1],
                                            kModbusExceptionIllegalDataAddress, broadcast,
                                            response, responseCapacity, responseLength);
        }
        server->mapping.coils[index] = (value == 0xFF00U) ? 1U : 0U;
    }
    else
    {
        if (!ModbusRtu_GetMappingIndex(server->mapping.holdingRegisterStart,
                                       server->mapping.holdingRegisterCount, address, 1U,
                                       &index))
        {
            return ModbusRtu_BuildException(server, request[1],
                                            kModbusExceptionIllegalDataAddress, broadcast,
                                            response, responseCapacity, responseLength);
        }
        server->mapping.holdingRegisters[index] = value;
    }
    if (!broadcast)
    {
        (void)memcpy(response, request, 6U);
    }
    return ModbusRtu_Finalize(server, broadcast, response, responseCapacity, 6U,
                              responseLength);
}

static modbus_result_t ModbusRtu_WriteMultiple(modbus_rtu_server_t *server,
                                               const uint8_t *request,
                                               size_t requestLength, bool coils,
                                               bool broadcast, uint8_t *response,
                                               size_t responseCapacity,
                                               size_t *responseLength)
{
    uint16_t address;
    uint16_t quantity;
    uint8_t byteCount;
    uint16_t index;
    uint16_t item;

    if (requestLength < 9U)
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    address = ModbusRtu_GetU16(&request[2]);
    quantity = ModbusRtu_GetU16(&request[4]);
    byteCount = request[6];
    if (requestLength != (9U + byteCount))
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    if (coils)
    {
        if ((quantity == 0U) || (quantity > MODBUS_RTU_MAX_WRITE_BITS) ||
            (byteCount != (uint8_t)(((uint32_t)quantity + 7U) / 8U)))
        {
            return ModbusRtu_BuildException(server, request[1],
                                            kModbusExceptionIllegalDataValue, broadcast,
                                            response, responseCapacity, responseLength);
        }
        if (!ModbusRtu_GetMappingIndex(server->mapping.coilStart,
                                       server->mapping.coilCount, address, quantity,
                                       &index))
        {
            return ModbusRtu_BuildException(server, request[1],
                                            kModbusExceptionIllegalDataAddress, broadcast,
                                            response, responseCapacity, responseLength);
        }
        for (item = 0U; item < quantity; item++)
        {
            server->mapping.coils[index + item] =
                (uint8_t)((request[7U + (item / 8U)] >> (item % 8U)) & 1U);
        }
    }
    else
    {
        if ((quantity == 0U) || (quantity > MODBUS_RTU_MAX_WRITE_REGISTERS) ||
            (byteCount != (uint8_t)(quantity * 2U)))
        {
            return ModbusRtu_BuildException(server, request[1],
                                            kModbusExceptionIllegalDataValue, broadcast,
                                            response, responseCapacity, responseLength);
        }
        if (!ModbusRtu_GetMappingIndex(server->mapping.holdingRegisterStart,
                                       server->mapping.holdingRegisterCount, address,
                                       quantity, &index))
        {
            return ModbusRtu_BuildException(server, request[1],
                                            kModbusExceptionIllegalDataAddress, broadcast,
                                            response, responseCapacity, responseLength);
        }
        for (item = 0U; item < quantity; item++)
        {
            server->mapping.holdingRegisters[index + item] =
                ModbusRtu_GetU16(&request[7U + ((size_t)item * 2U)]);
        }
    }

    if (!broadcast)
    {
        response[0] = server->slaveAddress;
        response[1] = request[1];
        (void)memcpy(&response[2], &request[2], 4U);
    }
    return ModbusRtu_Finalize(server, broadcast, response, responseCapacity, 6U,
                              responseLength);
}

static modbus_result_t ModbusRtu_MaskWrite(modbus_rtu_server_t *server,
                                           const uint8_t *request, size_t requestLength,
                                           bool broadcast, uint8_t *response,
                                           size_t responseCapacity, size_t *responseLength)
{
    uint16_t address;
    uint16_t index;

    if (requestLength != 10U)
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    address = ModbusRtu_GetU16(&request[2]);
    if (!ModbusRtu_GetMappingIndex(server->mapping.holdingRegisterStart,
                                   server->mapping.holdingRegisterCount, address, 1U,
                                   &index))
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataAddress, broadcast,
                                        response, responseCapacity, responseLength);
    }
    server->mapping.holdingRegisters[index] =
        (uint16_t)((server->mapping.holdingRegisters[index] & ModbusRtu_GetU16(&request[4])) |
                   (ModbusRtu_GetU16(&request[6]) &
                    (uint16_t)~ModbusRtu_GetU16(&request[4])));
    if (!broadcast)
    {
        (void)memcpy(response, request, 8U);
    }
    return ModbusRtu_Finalize(server, broadcast, response, responseCapacity, 8U,
                              responseLength);
}

static modbus_result_t ModbusRtu_WriteAndRead(modbus_rtu_server_t *server,
                                              const uint8_t *request,
                                              size_t requestLength, bool broadcast,
                                              uint8_t *response,
                                              size_t responseCapacity,
                                              size_t *responseLength)
{
    uint16_t readAddress;
    uint16_t readQuantity;
    uint16_t writeAddress;
    uint16_t writeQuantity;
    uint8_t byteCount;
    uint16_t readIndex;
    uint16_t writeIndex;
    uint16_t item;

    if (requestLength < 13U)
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    readAddress = ModbusRtu_GetU16(&request[2]);
    readQuantity = ModbusRtu_GetU16(&request[4]);
    writeAddress = ModbusRtu_GetU16(&request[6]);
    writeQuantity = ModbusRtu_GetU16(&request[8]);
    byteCount = request[10];
    if ((requestLength != (13U + byteCount)) || (readQuantity == 0U) ||
        (readQuantity > MODBUS_RTU_MAX_READ_REGISTERS) || (writeQuantity == 0U) ||
        (writeQuantity > MODBUS_RTU_MAX_RW_WRITE_REGS) ||
        (byteCount != (uint8_t)(writeQuantity * 2U)))
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    if (!ModbusRtu_GetMappingIndex(server->mapping.holdingRegisterStart,
                                   server->mapping.holdingRegisterCount, readAddress,
                                   readQuantity, &readIndex) ||
        !ModbusRtu_GetMappingIndex(server->mapping.holdingRegisterStart,
                                   server->mapping.holdingRegisterCount, writeAddress,
                                   writeQuantity, &writeIndex))
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataAddress, broadcast,
                                        response, responseCapacity, responseLength);
    }

    for (item = 0U; item < writeQuantity; item++)
    {
        server->mapping.holdingRegisters[writeIndex + item] =
            ModbusRtu_GetU16(&request[11U + ((size_t)item * 2U)]);
    }
    if (broadcast)
    {
        return ModbusRtu_Finalize(server, true, response, responseCapacity, 0U,
                                  responseLength);
    }
    if (responseCapacity < (5U + ((size_t)readQuantity * 2U)))
    {
        return kModbusResultResponseTooLarge;
    }
    response[0] = server->slaveAddress;
    response[1] = request[1];
    response[2] = (uint8_t)(readQuantity * 2U);
    for (item = 0U; item < readQuantity; item++)
    {
        ModbusRtu_PutU16(&response[3U + ((size_t)item * 2U)],
                         server->mapping.holdingRegisters[readIndex + item]);
    }
    return ModbusRtu_Finalize(server, false, response, responseCapacity,
                              3U + ((size_t)readQuantity * 2U), responseLength);
}

static modbus_result_t ModbusRtu_ReportSlaveId(modbus_rtu_server_t *server,
                                               const uint8_t *request,
                                               size_t requestLength, bool broadcast,
                                               uint8_t *response,
                                               size_t responseCapacity,
                                               size_t *responseLength)
{
    static const uint8_t identity[] = "MCXA176";
    const size_t byteCount = 2U + sizeof(identity) - 1U;

    if (requestLength != 4U)
    {
        return ModbusRtu_BuildException(server, request[1],
                                        kModbusExceptionIllegalDataValue, broadcast,
                                        response, responseCapacity, responseLength);
    }
    if (responseCapacity < (byteCount + 5U))
    {
        return kModbusResultResponseTooLarge;
    }
    response[0] = server->slaveAddress;
    response[1] = request[1];
    response[2] = (uint8_t)byteCount;
    response[3] = MODBUS_REPORT_SLAVE_ID;
    response[4] = 0xFFU;
    (void)memcpy(&response[5], identity, sizeof(identity) - 1U);
    return ModbusRtu_Finalize(server, broadcast, response, responseCapacity,
                              5U + sizeof(identity) - 1U, responseLength);
}

bool ModbusRtu_Init(modbus_rtu_server_t *server, uint8_t slaveAddress,
                    const modbus_mapping_t *mapping)
{
    if ((server == NULL) || !ModbusRtu_MappingValid(mapping) ||
        (slaveAddress == MODBUS_RTU_BROADCAST_ADDRESS) || (slaveAddress > 247U))
    {
        return false;
    }
    (void)memset(server, 0, sizeof(*server));
    server->slaveAddress = slaveAddress;
    server->mapping = *mapping;
    return true;
}

modbus_result_t ModbusRtu_Process(modbus_rtu_server_t *server,
                                  const uint8_t *request, size_t requestLength,
                                  uint8_t *response, size_t responseCapacity,
                                  size_t *responseLength)
{
    bool broadcast;
    modbus_result_t result;

    if ((responseLength != NULL))
    {
        *responseLength = 0U;
    }
    if ((server == NULL) || (request == NULL) || (response == NULL) ||
        (responseLength == NULL) || (requestLength < 4U) ||
        (requestLength > MODBUS_RTU_MAX_ADU_LENGTH) ||
        (responseCapacity < 5U))
    {
        return kModbusResultInvalidArgument;
    }
    server->diagnostics.receivedFrames++;
    if (!Crc16Driver_VerifyFrame(request, requestLength))
    {
        server->diagnostics.crcErrors++;
        return kModbusResultBadCrc;
    }
    broadcast = request[0] == MODBUS_RTU_BROADCAST_ADDRESS;
    if (!broadcast && (request[0] != server->slaveAddress))
    {
        server->diagnostics.ignoredAddressFrames++;
        return kModbusResultIgnored;
    }

    switch (request[1])
    {
        case MODBUS_FC_READ_COILS:
            result = ModbusRtu_ReadBits(server, request, requestLength, false, broadcast,
                                        response, responseCapacity, responseLength);
            break;
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            result = ModbusRtu_ReadBits(server, request, requestLength, true, broadcast,
                                        response, responseCapacity, responseLength);
            break;
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            result = ModbusRtu_ReadRegisters(server, request, requestLength, false,
                                             broadcast, response, responseCapacity,
                                             responseLength);
            break;
        case MODBUS_FC_READ_INPUT_REGISTERS:
            result = ModbusRtu_ReadRegisters(server, request, requestLength, true,
                                             broadcast, response, responseCapacity,
                                             responseLength);
            break;
        case MODBUS_FC_WRITE_SINGLE_COIL:
            result = ModbusRtu_WriteSingle(server, request, requestLength, true, broadcast,
                                           response, responseCapacity, responseLength);
            break;
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            result = ModbusRtu_WriteSingle(server, request, requestLength, false, broadcast,
                                           response, responseCapacity, responseLength);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            result = ModbusRtu_WriteMultiple(server, request, requestLength, true, broadcast,
                                             response, responseCapacity, responseLength);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            result = ModbusRtu_WriteMultiple(server, request, requestLength, false, broadcast,
                                             response, responseCapacity, responseLength);
            break;
        case MODBUS_FC_REPORT_SLAVE_ID:
            result = ModbusRtu_ReportSlaveId(server, request, requestLength, broadcast,
                                             response, responseCapacity, responseLength);
            break;
        case MODBUS_FC_MASK_WRITE_REGISTER:
            result = ModbusRtu_MaskWrite(server, request, requestLength, broadcast, response,
                                         responseCapacity, responseLength);
            break;
        case MODBUS_FC_WRITE_AND_READ_REGISTERS:
            result = ModbusRtu_WriteAndRead(server, request, requestLength, broadcast,
                                            response, responseCapacity, responseLength);
            break;
        default:
            result = ModbusRtu_BuildException(server, request[1],
                                              kModbusExceptionIllegalFunction, broadcast,
                                              response, responseCapacity, responseLength);
            break;
    }
    return result;
}

void ModbusRtu_GetDiagnostics(const modbus_rtu_server_t *server,
                              modbus_diagnostics_t *diagnostics)
{
    if ((server != NULL) && (diagnostics != NULL))
    {
        *diagnostics = server->diagnostics;
    }
}

void ModbusRtu_ClearDiagnostics(modbus_rtu_server_t *server)
{
    if (server != NULL)
    {
        (void)memset(&server->diagnostics, 0, sizeof(server->diagnostics));
    }
}

bool ModbusRtu_SelfTest(void)
{
    static const uint8_t readRequest[] = {0x01U, 0x03U, 0x00U, 0x00U,
                                          0x00U, 0x01U, 0x84U, 0x0AU};
    static const uint8_t readResponse[] = {0x01U, 0x03U, 0x02U, 0x12U,
                                           0x34U, 0xB5U, 0x33U};
    static const uint8_t writeRequest[] = {0x01U, 0x06U, 0x00U, 0x01U,
                                           0x55U, 0xAAU, 0x67U, 0x25U};
    uint16_t registers[2] = {0x1234U, 0U};
    const modbus_mapping_t mapping = {
        .holdingRegisterCount = 2U,
        .holdingRegisters = registers,
    };
    modbus_rtu_server_t server;
    uint8_t response[8];
    uint8_t invalidRequest[sizeof(readRequest)];
    size_t responseLength;

    if (!ModbusRtu_Init(&server, 1U, &mapping) ||
        (ModbusRtu_Process(&server, readRequest, sizeof(readRequest), response,
                           sizeof(response), &responseLength) !=
         kModbusResultResponseReady) ||
        (responseLength != sizeof(readResponse)) ||
        (memcmp(response, readResponse, sizeof(readResponse)) != 0))
    {
        return false;
    }
    if ((ModbusRtu_Process(&server, writeRequest, sizeof(writeRequest), response,
                           sizeof(response), &responseLength) !=
         kModbusResultResponseReady) ||
        (responseLength != sizeof(writeRequest)) ||
        (memcmp(response, writeRequest, sizeof(writeRequest)) != 0) ||
        (registers[1] != 0x55AAU))
    {
        return false;
    }
    (void)memcpy(invalidRequest, readRequest, sizeof(invalidRequest));
    invalidRequest[sizeof(invalidRequest) - 1U] ^= 1U;
    return ModbusRtu_Process(&server, invalidRequest, sizeof(invalidRequest), response,
                             sizeof(response), &responseLength) == kModbusResultBadCrc;
}
