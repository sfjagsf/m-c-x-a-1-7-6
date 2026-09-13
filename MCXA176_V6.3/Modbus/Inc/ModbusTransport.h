/* UART/DMA transport adapter for the embedded Modbus RTU core. */
#ifndef MODBUS_TRANSPORT_H_
#define MODBUS_TRANSPORT_H_

#include "ModbusRtu.h"
#include "UartDriver.h"

typedef struct
{
    modbus_rtu_server_t server;
    uart_port_id_t port;
    uint8_t response[MODBUS_RTU_MAX_ADU_LENGTH];
    volatile modbus_result_t lastProtocolResult;
    volatile status_t lastTransportStatus;
    volatile uint32_t transportErrorCount;
} modbus_transport_t;

bool ModbusTransport_Init(modbus_transport_t *transport, uart_port_id_t port,
                          uint8_t slaveAddress, const modbus_mapping_t *mapping);

/* Non-blocking: processes at most one complete frame already owned by UartDriver. */
void ModbusTransport_Poll(modbus_transport_t *transport);

#endif /* MODBUS_TRANSPORT_H_ */
