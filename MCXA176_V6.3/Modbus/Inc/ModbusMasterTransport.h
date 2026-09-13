/* Non-blocking UART/DMA transport for Modbus RTU master requests. */
#ifndef MODBUS_MASTER_TRANSPORT_H_
#define MODBUS_MASTER_TRANSPORT_H_

#include "ModbusMaster.h"
#include "UartDriver.h"

typedef enum
{
    kModbusMasterIdle = 0,
    kModbusMasterTransmitting,
    kModbusMasterWaitingResponse,
    kModbusMasterBroadcastSending,
    kModbusMasterComplete,
    kModbusMasterException,
    kModbusMasterProtocolError,
    kModbusMasterTimeout,
    kModbusMasterTransportError,
} modbus_master_state_t;

typedef struct
{
    modbus_master_t protocol;
    uart_port_id_t port;
    volatile modbus_master_state_t state;
    uint32_t startTick;
    uint32_t timeoutTicks;
    volatile status_t lastTransportStatus;
    volatile uint32_t completedCount;
    volatile uint32_t timeoutCount;
    volatile uint32_t protocolErrorCount;
    volatile uint32_t transportErrorCount;
} modbus_master_transport_t;

bool ModbusMasterTransport_Init(modbus_master_transport_t *transport,
                                uart_port_id_t port);

/* Call a ModbusMaster_Build* function, then submit its prepared request. */
status_t ModbusMasterTransport_Submit(modbus_master_transport_t *transport,
                                     uint32_t currentTick, uint32_t timeoutTicks);
void ModbusMasterTransport_Poll(modbus_master_transport_t *transport,
                                uint32_t currentTick);
void ModbusMasterTransport_Reset(modbus_master_transport_t *transport);
bool ModbusMasterTransport_IsFinished(const modbus_master_transport_t *transport);

#endif /* MODBUS_MASTER_TRANSPORT_H_ */
