/* Default Modbus RTU slave application for UART0/RS485. */
#include "../Inc/ModbusApp.h"

#include <string.h>

#include "cmsis_os2.h"

#define MODBUS_APP_TASK_PERIOD_MS (1U)

static uint8_t s_coils[MODBUS_APP_COIL_COUNT];
static uint8_t s_discreteInputs[MODBUS_APP_DISCRETE_INPUT_COUNT];
static uint16_t s_holdingRegisters[MODBUS_APP_HOLDING_REGISTER_COUNT];
static uint16_t s_inputRegisters[MODBUS_APP_INPUT_REGISTER_COUNT];
static modbus_transport_t s_transport;

bool ModbusApp_Init(void)
{
    const modbus_mapping_t mapping = {
        .coilStart = 0U,
        .coilCount = MODBUS_APP_COIL_COUNT,
        .coils = s_coils,
        .discreteInputStart = 0U,
        .discreteInputCount = MODBUS_APP_DISCRETE_INPUT_COUNT,
        .discreteInputs = s_discreteInputs,
        .holdingRegisterStart = 0U,
        .holdingRegisterCount = MODBUS_APP_HOLDING_REGISTER_COUNT,
        .holdingRegisters = s_holdingRegisters,
        .inputRegisterStart = 0U,
        .inputRegisterCount = MODBUS_APP_INPUT_REGISTER_COUNT,
        .inputRegisters = s_inputRegisters,
    };

    if (!ModbusRtu_SelfTest())
    {
        return false;
    }

    (void)memset(s_coils, 0, sizeof(s_coils));
    (void)memset(s_discreteInputs, 0, sizeof(s_discreteInputs));
    (void)memset(s_holdingRegisters, 0, sizeof(s_holdingRegisters));
    (void)memset(s_inputRegisters, 0, sizeof(s_inputRegisters));
    return ModbusTransport_Init(&s_transport, kUartPort0,
                                MODBUS_APP_SLAVE_ADDRESS, &mapping);
}

bool ModbusApp_Create(void)
{
    static const osThreadAttr_t attributes = {
        .name = "ModbusRtu",
        .priority = osPriorityNormal,
        .stack_size = 768U,
    };

    return osThreadNew(ModbusApp_Task, NULL, &attributes) != NULL;
}

void ModbusApp_Task(void *argument)
{
    (void)argument;
    for (;;)
    {
        ModbusApp_Process();
        (void)osDelay(MODBUS_APP_TASK_PERIOD_MS);
    }
}

void ModbusApp_Process(void)
{
    ModbusTransport_Poll(&s_transport);
}

modbus_transport_t *ModbusApp_GetTransport(void)
{
    return &s_transport;
}

uint8_t *ModbusApp_GetCoils(void)
{
    return s_coils;
}

uint8_t *ModbusApp_GetDiscreteInputs(void)
{
    return s_discreteInputs;
}

uint16_t *ModbusApp_GetHoldingRegisters(void)
{
    return s_holdingRegisters;
}

uint16_t *ModbusApp_GetInputRegisters(void)
{
    return s_inputRegisters;
}
