# Embedded Modbus RTU port

This directory contains the MCU master and slave protocol layers adapted from
libmodbus 3.2.0 (commit a9b025d12289855490b10d77461c99e001abfc0f).
The POSIX/Win32 serial, socket, heap-allocation and blocking-I/O backends are
intentionally excluded. `ModbusRtu` owns protocol validation and register-map
semantics; `Src/ModbusTransport.c` owns the existing UART frame adapter; `ModbusApp`
provides the default UART0/RS485 slave task and static register storage.
`ModbusMaster` provides the official master request/confirmation behavior and
`ModbusMasterTransport` provides a non-blocking UART/DMA transaction state
machine. No master transport is started by default because UART0 is occupied by
the slave and UART1 is occupied by its echo test.

Default settings:

- RTU slave address: 1
- UART: UART0 (RS485, shared DMA channel 0)
- Coils/discrete inputs/holding registers/input registers: 128 each, address 0
- Supported function codes: 01, 02, 03, 04, 05, 06, 0F, 10, 11, 16 and 17
- Maximum RTU ADU: 256 bytes
- CRC: `Crc16Driver` hardware-first with software lookup-table fallback
- Startup self-test: fixed FC03/FC06 request and response vectors

Headers are kept in `Inc`; sources are kept in `Src`. Change the
`MODBUS_APP_*` macros in `Inc/ModbusApp.h` before connecting the arrays
to application data. Do not run the UART0 echo task at the same time as the
Modbus task because both would consume the same received frame.

The upstream project is <https://libmodbus.org/>. This embedded adaptation is
distributed under LGPL-2.1-or-later; see `COPYING.LESSER`.

## Upstream mapping

The RTU roles use the local checkout at `D:/99.my_Project/Modbus/libmodbus` as
their protocol baseline:

- `modbus_reply()` -> `ModbusRtu_Process()` (slave request validation, mapping
  access, exceptions and broadcast handling).
- `modbus_read_*()` / `modbus_write_*()` -> `ModbusMaster_Build*()` plus
  `ModbusMaster_ParseResponse()` (master request and confirmation checks).
- `_modbus_rtu_build_request_basis()` / `_modbus_rtu_send_msg_pre()` -> request
  builders plus the project CRC16 driver.
- libmodbus `send` / `receive` / `select` backend calls ->
  `ModbusTransport` and `ModbusMasterTransport`, using the existing NXP
  UART/eDMA frame API without blocking or dynamic allocation.

The port covers both RTU master and RTU slave roles. Modbus TCP, POSIX/Win32
device management, sockets, proxy mode and dynamic mapping allocation are host
features and are deliberately not copied into the MCU firmware.

For a master transaction, initialize one `modbus_master_transport_t`, call the
required `ModbusMaster_Build*()` function on its `protocol` member, submit the
request, then call `ModbusMasterTransport_Poll()` from the owning task until
`ModbusMasterTransport_IsFinished()` is true. One physical UART must have one
owner: do not run a master and slave transport on the same UART concurrently.
