# Embedded Modbus RTU port

This directory contains the MCU protocol layer adapted from libmodbus 3.2.0.
The POSIX/Win32 serial, socket, heap-allocation and blocking-I/O backends are
intentionally excluded. `ModbusRtu` owns protocol validation and register-map
semantics; `Src/ModbusTransport.c` owns the existing UART frame adapter; `ModbusApp`
provides the default UART0/RS485 slave task and static register storage.

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
