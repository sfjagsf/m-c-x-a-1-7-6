/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "app_smartdma.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_device_registers.h"
#include "fsl_reset.h"
#include "pin_mux.h"

/* Prebuilt firmware image exported by app_smartdma_firmware.c. */
extern const uint8_t g_appSmartDMAFirmware[];
extern const uint32_t g_appSmartDMAFirmwareSize;

#define SMARTDMA_FIRMWARE_ADDRESS 0x04000000U
#define SMARTDMA_FIRMWARE_API     0U
#define SMARTDMA_SRAMX0_SIZE      8192U
#define SMARTDMA_START_WAIT_LIMIT 1000000U

#define SMARTDMA_CTRL_SYNC (SMARTDMA_CTRL_WKEY(0xC0DEU) | SMARTDMA_CTRL_SYNCEN_MASK)
#define SMARTDMA_CTRL_RUN  (SMARTDMA_CTRL_SYNC | SMARTDMA_CTRL_START_MASK)

/* Command values are part of the interface between the Arm core and the firmware. */
enum
{
    kSmartDmaCommandIdle  = 0U,
    kSmartDmaCommandTx    = 1U,
    kSmartDmaCommandRx    = 2U,
    kSmartDmaCommandAbort = 3U,
};

/*
 * Firmware diagnostic buffer layout. Words 0 through 9 mirror the parameter
 * block at startup. The remaining words retain the latest transfer state.
 */
enum
{
    kDebugParamFirst = 0U,
    kDebugParamLast  = 9U,
    kDebugLastCommand = 10U, /* Last command read by SmartDMA. */
    kDebugTxLastStatus,      /* Last LPUART status read by the TX path. */
    kDebugTxLastData,        /* Last byte written to LPUART DATA. */
    kDebugTxRemaining,       /* Remaining TX bytes. */
    kDebugTxComplete,        /* TX completion marker. */
    kDebugRxLastStatus,      /* Last LPUART status read by the RX path. */
    kDebugRxLastData,        /* Last value read from LPUART DATA. */
    kDebugRxRemaining,       /* Remaining RX bytes. */
    kDebugRxComplete,        /* RX completion marker. */
    kDebugAbortComplete,     /* Abort completion marker. */
    kSmartDmaDebugWordCount,
};

/* Fixed ten-word parameter block consumed by the prebuilt firmware. */
typedef struct
{
    uint32_t *stack;                       /* Word 0: SmartDMA stack top. */
    volatile uint32_t *debugBuffer;        /* Word 1: diagnostic buffer. */
    uint8_t *txBuffer;                     /* Word 2: active TX buffer. */
    uint32_t txCount;                      /* Word 3: remaining TX bytes. */
    volatile uint32_t *txDataRegister;     /* Word 4: LPUART TX DATA address. */
    uint8_t *rxBuffer;                     /* Word 5: active RX buffer. */
    uint32_t rxCount;                      /* Word 6: remaining RX bytes. */
    volatile uint32_t *rxDataRegister;     /* Word 7: LPUART RX DATA address. */
    volatile uint32_t *ready;              /* Word 8: startup-ready flag. */
    volatile uint32_t *command;            /* Word 9: shared command. */
} smartdma_parameter_block_t;

static uint32_t s_smartdmaStack[32];
static volatile uint32_t s_smartdmaDebug[kSmartDmaDebugWordCount];
static volatile uint32_t s_smartdmaReady;
static volatile uint32_t s_smartdmaCommand;
static volatile uint32_t s_smartdmaActiveCommand;
static volatile smartdma_parameter_block_t s_smartdmaParameters;

/*
 * SRAMX0 is the SmartDMA code memory. The image is position dependent: word 0
 * holds the absolute entry address 0x04000005, so it only runs from the base of
 * SRAMX0. Claiming the whole region here keeps the linker from placing anything
 * else in it, so a second SRAMX0 object becomes a link error instead of a silent
 * overlap with the firmware.
 */
static uint32_t s_smartdmaCodeRegion[SMARTDMA_SRAMX0_SIZE / sizeof(uint32_t)] __attribute__((section(".bss.$RAM3")));

/* The parameter block is an ABI shared with the firmware; ten words, no padding. */
_Static_assert(sizeof(smartdma_parameter_block_t) == (10U * sizeof(uint32_t)), "SmartDMA parameter layout changed");
_Static_assert(offsetof(smartdma_parameter_block_t, command) == (9U * sizeof(uint32_t)), "command offset");

/* Confirm that SmartDMA read the same ten startup words that the Arm core wrote. */
static bool APP_SmartDMAValidateParameters(void)
{
    /* Safe because the parameter block is asserted to be ten packed words. */
    const volatile uint32_t *written = (const volatile uint32_t *)&s_smartdmaParameters;

    for (uint32_t index = kDebugParamFirst; index <= kDebugParamLast; index++)
    {
        if (s_smartdmaDebug[index] != written[index])
        {
            return false;
        }
    }

    return true;
}

/* Configure only the observation pins selected in app_smartdma.h. */
static void APP_SmartDMAInitObservationPins(void)
{
#if defined(APP_SMARTDMA_ENABLE_CMD_SCAN_PIN)
    BOARD_InitSmartDMACmdScanPin();
#endif
#if defined(APP_SMARTDMA_ENABLE_TX_SCAN_PIN)
    BOARD_InitSmartDMATxScanPin();
#endif
#if defined(APP_SMARTDMA_ENABLE_RX_SCAN_PIN)
    BOARD_InitSmartDMARxScanPin();
#endif
#if defined(APP_SMARTDMA_ENABLE_BYTE_TOGGLE_PIN)
    BOARD_InitSmartDMAByteTogglePin();
#endif
}

bool APP_SmartDMAInit(const app_smartdma_config_t *config)
{
    uint32_t firmwareEntry;
    uint32_t waitCount = SMARTDMA_START_WAIT_LIMIT;

    if ((config == NULL) || (config->txBuffer == NULL) || (config->txDataRegister == NULL) ||
        (config->rxBuffer == NULL) || (config->rxDataRegister == NULL) || (config->txBufferSize == 0U) ||
        (config->rxBufferSize == 0U) || (config->txBufferSize > APP_SMARTDMA_MAX_TRANSFER_SIZE) ||
        (config->rxBufferSize > APP_SMARTDMA_MAX_TRANSFER_SIZE) || (g_appSmartDMAFirmwareSize < sizeof(uint32_t)) ||
        (g_appSmartDMAFirmwareSize > SMARTDMA_SRAMX0_SIZE) || ((g_appSmartDMAFirmwareSize & 0x3U) != 0U))
    {
        return false;
    }

    /* The firmware only runs from the base of SRAMX0. */
    if ((uint32_t)s_smartdmaCodeRegion != SMARTDMA_FIRMWARE_ADDRESS)
    {
        return false;
    }

    /* This function is also the recovery path, so stop any running instance first. */
    DisableIRQ(SMARTDMA_IRQn);
    CLOCK_EnableClock(kCLOCK_Smartdma);
    RESET_PeripheralReset(kSMART_DMA_RST_SHIFT_RSTn);

    s_smartdmaParameters.stack = &s_smartdmaStack[32];
    s_smartdmaParameters.debugBuffer = s_smartdmaDebug;
    s_smartdmaParameters.txBuffer = config->txBuffer;
    s_smartdmaParameters.txCount = config->txBufferSize;
    s_smartdmaParameters.txDataRegister = config->txDataRegister;
    s_smartdmaParameters.rxBuffer = config->rxBuffer;
    s_smartdmaParameters.rxCount = config->rxBufferSize;
    s_smartdmaParameters.rxDataRegister = config->rxDataRegister;
    s_smartdmaParameters.ready = &s_smartdmaReady;
    s_smartdmaParameters.command = &s_smartdmaCommand;

    for (uint32_t index = 0U; index < kSmartDmaDebugWordCount; index++)
    {
        s_smartdmaDebug[index] = 0U;
    }
    s_smartdmaReady = 0U;
    s_smartdmaCommand = kSmartDmaCommandIdle;
    s_smartdmaActiveCommand = kSmartDmaCommandIdle;

    APP_SmartDMAInitObservationPins();

    /* Install the image in SRAMX0, then read API entry zero from the copied table. */
    (void)memcpy(s_smartdmaCodeRegion, g_appSmartDMAFirmware, g_appSmartDMAFirmwareSize);
    __DSB();
    __ISB();

    firmwareEntry = s_smartdmaCodeRegion[SMARTDMA_FIRMWARE_API] & SMARTDMA_BOOTADR_ADDR_MASK;
    if ((firmwareEntry < SMARTDMA_FIRMWARE_ADDRESS) ||
        (firmwareEntry >= (SMARTDMA_FIRMWARE_ADDRESS + g_appSmartDMAFirmwareSize)))
    {
        return false;
    }

    /* Enable synchronized Arm/SmartDMA access and completion interrupts. */
    SMARTDMA0->CTRL = SMARTDMA_CTRL_SYNC;
    SMARTDMA0->PENDTRAP = 0U;
    NVIC_ClearPendingIRQ(SMARTDMA_IRQn);
    NVIC_SetPriority(SMARTDMA_IRQn, 3U);
    EnableIRQ(SMARTDMA_IRQn);
    SMARTDMA0->ARM2EZH = (((uint32_t)&s_smartdmaParameters) & ~0x3U) | 0x2U;
    SMARTDMA0->BOOTADR = firmwareEntry;
    SMARTDMA0->CTRL = SMARTDMA_CTRL_RUN;

    while ((s_smartdmaReady == 0U) && (waitCount > 0U))
    {
        waitCount--;
    }
    __DSB();

    return (s_smartdmaReady != 0U) && APP_SmartDMAValidateParameters();
}

bool APP_SmartDMAStartTx(uint8_t *txBuffer, uint32_t txCount)
{
    if ((txBuffer == NULL) || (txCount == 0U) || (txCount > APP_SMARTDMA_MAX_TRANSFER_SIZE) ||
        (s_smartdmaReady == 0U) || (s_smartdmaCommand != kSmartDmaCommandIdle) ||
        (s_smartdmaActiveCommand != kSmartDmaCommandIdle))
    {
        return false;
    }

    s_smartdmaParameters.txBuffer = txBuffer;
    s_smartdmaParameters.txCount = txCount;
    s_smartdmaDebug[kDebugTxComplete] = 0U;
    s_smartdmaActiveCommand = kSmartDmaCommandTx;
    __DMB();
    s_smartdmaCommand = kSmartDmaCommandTx;
    __DSB();
    return true;
}

bool APP_SmartDMAStartRx(uint8_t *rxBuffer, uint32_t rxCount)
{
    if ((rxBuffer == NULL) || (rxCount == 0U) || (rxCount > APP_SMARTDMA_MAX_TRANSFER_SIZE) ||
        (s_smartdmaReady == 0U) || (s_smartdmaCommand != kSmartDmaCommandIdle) ||
        (s_smartdmaActiveCommand != kSmartDmaCommandIdle))
    {
        return false;
    }

    s_smartdmaParameters.rxBuffer = rxBuffer;
    s_smartdmaParameters.rxCount = rxCount;
    s_smartdmaDebug[kDebugRxComplete] = 0U;
    s_smartdmaActiveCommand = kSmartDmaCommandRx;
    __DMB();
    s_smartdmaCommand = kSmartDmaCommandRx;
    __DSB();
    return true;
}

bool APP_SmartDMAAbort(void)
{
    if (s_smartdmaActiveCommand == kSmartDmaCommandIdle)
    {
        return false;
    }

    s_smartdmaDebug[kDebugAbortComplete] = 0U;
    __DMB();
    s_smartdmaCommand = kSmartDmaCommandAbort;
    __DSB();
    return true;
}

bool APP_SmartDMAIsBusy(void)
{
    __DMB();
    return s_smartdmaActiveCommand != kSmartDmaCommandIdle;
}

void SMARTDMA_IRQHandler(void)
{
    uint32_t interruptReason = SMARTDMA0->EZH2ARM;

    if ((interruptReason >= kSmartDmaCommandTx) && (interruptReason <= kSmartDmaCommandAbort))
    {
        s_smartdmaActiveCommand = kSmartDmaCommandIdle;
    }

    SDK_ISR_EXIT_BARRIER;
}
