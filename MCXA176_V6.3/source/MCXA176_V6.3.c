/*
 * Copyright 2016-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    MCXA176_V6.3.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
//#include "fsl_debug_console.h"
#include "cmsis_os2.h"
#include "GpioDmaFilter.h"
#include "GpioInputTask.h"
#include "Crc16Driver.h"
#include "../Modbus/Inc/ModbusApp.h"
#include "UartDriver.h"
#include "UartEchoTask.h"

/* TODO: insert other definitions and declarations here. */

/*
 * @brief   Application entry point.
 */
int main(void) {

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    Crc16Driver_Init();
    Uart0_Init();
    Uart1_Init();

    if (!ModbusApp_Init())
    {
        for (;;)
        {
            __asm volatile ("nop");
        }
    }

    if (!GpioDmaFilterStart())
    {
//        PRINTF("GPIO DMA filter start failed\r\n");
        for (;;)
        {
            __asm volatile ("nop");
        }
    }

    GpioInputInitStatus();

    if (osKernelInitialize() != osOK)
    {
        for (;;)
        {
            __asm volatile ("nop");
        }
    }

    if (!GpioUpdateTask_Create())
    {
        for (;;)
        {
            __asm volatile ("nop");
        }
    }

    /* UART0 is owned by the Modbus RTU service; do not also start its echo task. */
    if (!ModbusApp_Create())
    {
        for (;;)
        {
            __asm volatile ("nop");
        }
    }

    if (!Uart1EchoTask_Create())
    {
        for (;;)
        {
            __asm volatile ("nop");
        }
    }

    (void)osKernelStart();

    for (;;)
    {
        __asm volatile ("nop");
    }
    return 0 ;
}
