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
#include "fsl_clock.h"
#include "fsl_common.h"
//#include "fsl_debug_console.h"
#include "cmsis_os2.h"
#include "GpioDmaFilter.h"
#include "GpioInputTask.h"
#include "Crc16Driver.h"
#include "../Modbus/Inc/ModbusApp.h"
#include "UartDriver.h"
#include "UartEchoTask.h"
#include "AdcDma.h"
#include "I2cEepromDriver.h"
#include "Pcf8563RtcDriver.h"
/* #include "WatchdogDriver.h" */ /* Watchdog disabled for current build. */
#include "../SmartDMA_UART/app_smartdma_lpuart0.h"

/* TODO: insert other definitions and declarations here. */

/* Keep the target in a minimal state briefly after each reset for SWD attach. */
#define BOOT_DEBUG_HOLD_US (2000000U)

/*
 * @brief   Application entry point.
 */
int main(void) {

    /* Init board hardware. */
    BOARD_InitBootPins();
//#if defined(DEBUG)
    /*
     * The generated 180 MHz profile waits indefinitely for an 8 MHz external
     * oscillator/PLL lock. Use the internal 45 MHz FRO while debugging so SWD
     * remains attached even when that external clock is unavailable.
     */
//#else
    BOARD_InitBootClocks();
//#endif

    /*
     * The core clock is now stable.  Do not initialise board peripherals or
     * start application tasks during this interval, so LinkServer has a
     * predictable two-second window to attach after power-on/reset.
     */
//    SDK_DelayAtLeastUs(BOOT_DEBUG_HOLD_US, CLOCK_GetCoreSysClkFreq());

    BOARD_InitBootPeripherals();
    /* Firmware is resident but idle; UART0 remains owned by eDMA until its backend is replaced. */
//#if defined(DEBUG)
//    /* Stop here before the unverified SmartDMA image is started. */
//    __asm volatile ("bkpt #0");
//#endif
	if (!APP_SmartDMALPUART0_Init()) {
		for (;;) {
			__asm volatile ("nop");
		}
	}
//	APP_SmartDMALPUART0_Init();
    Pcf8563_Init();
    I2cEeprom_Init();
    AdcDma_Init();
    if (!AdcDmaStartContinuous())
    {
        for (;;)
        {
            __asm volatile ("nop");
        }
    }
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
