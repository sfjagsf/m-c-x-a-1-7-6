/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include "fsl_common.h"
#include "board.h"

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

/* Initialize debug console. */
void BOARD_InitDebugConsole(void)
{
    /* attach 12 MHz clock to LPUART0 (debug console) */
    CLOCK_SetClockDiv(kCLOCK_DivLPUART0, 1u);

}
