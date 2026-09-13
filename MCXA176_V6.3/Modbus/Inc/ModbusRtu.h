/*
 * Embedded Modbus RTU server API.
 *
 * Protocol behavior is adapted from libmodbus 3.2.0.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef MODBUS_RTU_H_
#define MODBUS_RTU_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODBUS_RTU_MAX_ADU_LENGTH       (256U)
#define MODBUS_RTU_BROADCAST_ADDRESS    (0U)
#define MODBUS_RTU_MAX_READ_BITS        (2000U)
#define MODBUS_RTU_MAX_WRITE_BITS       (1968U)
#define MODBUS_RTU_MAX_READ_REGISTERS   (125U)
#define MODBUS_RTU_MAX_WRITE_REGISTERS  (123U)
#define MODBUS_RTU_MAX_RW_WRITE_REGS    (121U)

typedef enum
{
    kModbusExceptionIllegalFunction = 0x01U,
    kModbusExceptionIllegalDataAddress = 0x02U,
    kModbusExceptionIllegalDataValue = 0x03U,
    kModbusExceptionServerFailure = 0x04U,
    kModbusExceptionServerBusy = 0x06U,
} modbus_exception_t;

typedef enum
{
    kModbusResultIgnored = 0,
    kModbusResultResponseReady,
    kModbusResultBroadcastProcessed,
    kModbusResultInvalidArgument = -1,
    kModbusResultBadCrc = -2,
    kModbusResultMalformedRequest = -3,
    kModbusResultResponseTooLarge = -4,
} modbus_result_t;

/* Counts are element counts. A zero count permits a NULL table pointer. */
typedef struct
{
    uint16_t coilStart;
    uint16_t coilCount;
    uint8_t *coils;
    uint16_t discreteInputStart;
    uint16_t discreteInputCount;
    const uint8_t *discreteInputs;
    uint16_t holdingRegisterStart;
    uint16_t holdingRegisterCount;
    uint16_t *holdingRegisters;
    uint16_t inputRegisterStart;
    uint16_t inputRegisterCount;
    const uint16_t *inputRegisters;
} modbus_mapping_t;

typedef struct
{
    uint32_t receivedFrames;
    uint32_t repliedFrames;
    uint32_t broadcastFrames;
    uint32_t ignoredAddressFrames;
    uint32_t crcErrors;
    uint32_t malformedFrames;
    uint32_t exceptionResponses;
} modbus_diagnostics_t;

typedef struct
{
    uint8_t slaveAddress;
    modbus_mapping_t mapping;
    modbus_diagnostics_t diagnostics;
} modbus_rtu_server_t;

bool ModbusRtu_Init(modbus_rtu_server_t *server, uint8_t slaveAddress,
                    const modbus_mapping_t *mapping);

/*
 * Validates and executes one complete RTU ADU. responseLength is always set to
 * zero unless a response is ready. The request and response buffers may not
 * overlap. Broadcast writes update the mapping but deliberately return no ADU.
 */
modbus_result_t ModbusRtu_Process(modbus_rtu_server_t *server,
                                  const uint8_t *request, size_t requestLength,
                                  uint8_t *response, size_t responseCapacity,
                                  size_t *responseLength);

void ModbusRtu_GetDiagnostics(const modbus_rtu_server_t *server,
                              modbus_diagnostics_t *diagnostics);
void ModbusRtu_ClearDiagnostics(modbus_rtu_server_t *server);

/* Covers CRC, FC03 and FC06 with fixed Modbus RTU reference frames. */
bool ModbusRtu_SelfTest(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RTU_H_ */
