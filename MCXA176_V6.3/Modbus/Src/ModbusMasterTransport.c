/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "../Inc/ModbusMasterTransport.h"

#include <string.h>

bool ModbusMasterTransport_Init(modbus_master_transport_t *transport,
                                uart_port_id_t port)
{
    if ((transport == NULL) || ((uint32_t)port >= UART_PORT_COUNT))
    {
        return false;
    }
    (void)memset(transport, 0, sizeof(*transport));
    transport->port = port;
    transport->lastTransportStatus = kStatus_Success;
    ModbusMaster_Init(&transport->protocol);
    return ModbusMaster_SelfTest();
}

status_t ModbusMasterTransport_Submit(modbus_master_transport_t *transport,
                                     uint32_t currentTick, uint32_t timeoutTicks)
{
    status_t status;

    if ((transport == NULL) || (transport->protocol.requestLength < 4U) ||
        (timeoutTicks == 0U))
    {
        return kStatus_InvalidArgument;
    }
    if ((transport->state == kModbusMasterTransmitting) ||
        (transport->state == kModbusMasterWaitingResponse) ||
        (transport->state == kModbusMasterBroadcastSending) ||
        UartPort_IsBusy(transport->port))
    {
        return kStatus_Busy;
    }
    status = UartPort_Send(transport->port, transport->protocol.request,
                           transport->protocol.requestLength);
    transport->lastTransportStatus = status;
    if (status != kStatus_Success)
    {
        transport->transportErrorCount++;
        transport->state = kModbusMasterTransportError;
        return status;
    }
    transport->startTick = currentTick;
    transport->timeoutTicks = timeoutTicks;
    transport->state =
        (transport->protocol.request[0] == MODBUS_RTU_BROADCAST_ADDRESS) ?
            kModbusMasterBroadcastSending : kModbusMasterTransmitting;
    return kStatus_Success;
}

void ModbusMasterTransport_Poll(modbus_master_transport_t *transport,
                                uint32_t currentTick)
{
    const uint8_t *frame;
    size_t frameLength;
    modbus_master_result_t result;
    status_t status;

    if (transport == NULL)
    {
        return;
    }
    if (transport->state == kModbusMasterBroadcastSending)
    {
        if (!UartPort_IsBusy(transport->port))
        {
            transport->state = kModbusMasterComplete;
            transport->completedCount++;
        }
        return;
    }
    if (transport->state == kModbusMasterTransmitting)
    {
        /* libmodbus starts response_timeout after the complete request is sent. */
        if (!UartPort_IsBusy(transport->port))
        {
            transport->startTick = currentTick;
            transport->state = kModbusMasterWaitingResponse;
        }
        return;
    }
    if (transport->state != kModbusMasterWaitingResponse)
    {
        return;
    }

    frame = UartPort_GetFrame(transport->port, &frameLength);
    if ((frame != NULL) && (frameLength != 0U))
    {
        result = ModbusMaster_ParseResponse(&transport->protocol, frame, frameLength);
        status = UartPort_ReleaseFrame(transport->port);
        transport->lastTransportStatus = status;
        if (status != kStatus_Success)
        {
            transport->transportErrorCount++;
            transport->state = kModbusMasterTransportError;
        }
        else if (result == kModbusMasterResultOk)
        {
            transport->completedCount++;
            transport->state = kModbusMasterComplete;
        }
        else if (result == kModbusMasterResultException)
        {
            transport->state = kModbusMasterException;
        }
        else
        {
            transport->protocolErrorCount++;
            transport->state = kModbusMasterProtocolError;
        }
        return;
    }
    if ((uint32_t)(currentTick - transport->startTick) >= transport->timeoutTicks)
    {
        transport->timeoutCount++;
        transport->state = kModbusMasterTimeout;
        UartPort_Abort(transport->port);
        transport->lastTransportStatus = UartPort_StartReceive(transport->port);
    }
}

void ModbusMasterTransport_Reset(modbus_master_transport_t *transport)
{
    if (transport != NULL)
    {
        transport->state = kModbusMasterIdle;
        transport->protocol.requestLength = 0U;
        transport->protocol.responseLength = 0U;
        transport->protocol.exceptionCode = 0U;
    }
}

bool ModbusMasterTransport_IsFinished(const modbus_master_transport_t *transport)
{
    return (transport != NULL) && (transport->state >= kModbusMasterComplete);
}
