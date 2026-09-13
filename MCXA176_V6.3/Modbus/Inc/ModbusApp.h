/* Default UART0/RS485 Modbus application binding. */
#ifndef MODBUS_APP_H_
#define MODBUS_APP_H_

#include <stdbool.h>
#include <stdint.h>

#include "ModbusTransport.h"

#define MODBUS_APP_SLAVE_ADDRESS          (1U)
#define MODBUS_APP_COIL_COUNT             (128U)
#define MODBUS_APP_DISCRETE_INPUT_COUNT   (128U)
#define MODBUS_APP_HOLDING_REGISTER_COUNT (128U)
#define MODBUS_APP_INPUT_REGISTER_COUNT   (128U)

bool ModbusApp_Init(void);
bool ModbusApp_Create(void);
void ModbusApp_Task(void *argument);
void ModbusApp_Process(void);

modbus_transport_t *ModbusApp_GetTransport(void);
uint8_t *ModbusApp_GetCoils(void);
uint8_t *ModbusApp_GetDiscreteInputs(void);
uint16_t *ModbusApp_GetHoldingRegisters(void);
uint16_t *ModbusApp_GetInputRegisters(void);

#endif /* MODBUS_APP_H_ */
