/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "../Inc/ModbusTransport.h"

#include <string.h>

bool ModbusTransport_Init(modbus_transport_t *transport, uart_port_id_t port,
                          uint8_t slaveAddress, const modbus_mapping_t *mapping)
{
    if ((transport == NULL) || ((uint32_t)port >= UART_PORT_COUNT))
    {
        return false;
    }
    (void)memset(transport, 0, sizeof(*transport));
    transport->port = port;
    transport->lastTransportStatus = kStatus_Success;
    return ModbusRtu_Init(&transport->server, slaveAddress, mapping);
}

void ModbusTransport_Poll(modbus_transport_t *transport)
{
    const uint8_t *request;
    size_t requestLength;
    size_t responseLength = 0U;
    status_t status;

    if (transport == NULL)
    {
        return;
    }
    request = UartPort_GetFrame(transport->port, &requestLength);
    if ((request == NULL) || (requestLength == 0U))
    {
        return;
    }

    transport->lastProtocolResult =
        ModbusRtu_Process(&transport->server, request, requestLength,
                          transport->response, sizeof(transport->response),
                          &responseLength);
    if ((transport->lastProtocolResult == kModbusResultResponseReady) &&
        (responseLength != 0U))
    {
        /* Reply copies into the selected UART's private TX buffer before RX restarts. */
        status = UartPort_Reply(transport->port, transport->response, responseLength);
    }
    else
    {
        /* CRC errors, foreign addresses and broadcasts never produce a response. */
        status = UartPort_ReleaseFrame(transport->port);
    }
    transport->lastTransportStatus = status;
    if (status != kStatus_Success)
    {
        transport->transportErrorCount++;
        UartPort_Abort(transport->port);
        (void)UartPort_StartReceive(transport->port);
    }
}
