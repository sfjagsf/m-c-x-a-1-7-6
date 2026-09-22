/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef APP_SMARTDMA_H_
#define APP_SMARTDMA_H_

#include <stdbool.h>
#include <stdint.h>

#define APP_SMARTDMA_MAX_TRANSFER_SIZE 512U

/*
 * Optional logic-analyzer outputs.
 * Remove a definition when the corresponding MCU pin is needed elsewhere.
 * The prebuilt firmware remains unchanged.
 */
//#define APP_SMARTDMA_ENABLE_CMD_SCAN_PIN    /* P0_4 / SmartDMA_PIO0. */
//#define APP_SMARTDMA_ENABLE_TX_SCAN_PIN     /* P0_5 / SmartDMA_PIO1. */
//#define APP_SMARTDMA_ENABLE_RX_SCAN_PIN     /* P3_2 / SmartDMA_PIO2. */
//#define APP_SMARTDMA_ENABLE_BYTE_TOGGLE_PIN /* P3_3 / SmartDMA_PIO3. */

/* Resources shared by the application and the prebuilt SmartDMA firmware. */
typedef struct
{
    uint8_t *txBuffer;                 /* Initial TX buffer. */
    uint32_t txBufferSize;             /* TX buffer capacity, 1 to 512 bytes. */
    volatile uint32_t *txDataRegister; /* LPUART DATA register used for TX. */
    uint8_t *rxBuffer;                 /* Initial RX buffer. */
    uint32_t rxBufferSize;             /* RX buffer capacity, 1 to 512 bytes. */
    volatile uint32_t *rxDataRegister; /* LPUART DATA register used for RX. */
} app_smartdma_config_t;

typedef enum
{
    kAppSmartDMAEventTxComplete = 1U,
    kAppSmartDMAEventRxComplete = 2U,
    kAppSmartDMAEventAbortComplete = 3U,
} app_smartdma_event_t;

typedef void (*app_smartdma_callback_t)(app_smartdma_event_t event, void *userData);

/* Read-only snapshot used by the UART1 diagnostic logger. */
typedef struct
{
    uint32_t ready;
    uint32_t command;
    uint32_t activeCommand;
    uint32_t txRemaining;
    uint32_t rxRemaining;
    uint32_t lastCommand;
    uint32_t txLastStatus;
    uint32_t txLastData;
    uint32_t txComplete;
    uint32_t rxLastStatus;
    uint32_t rxLastData;
    uint32_t rxComplete;
    uint32_t abortComplete;
    uint32_t smartdmaCtrl;
    uint32_t smartdmaPc;
} app_smartdma_debug_snapshot_t;

/* Install the firmware in SRAMX0 and start the SmartDMA command processor. */
bool APP_SmartDMAInit(const app_smartdma_config_t *config);

/* Start one asynchronous transfer. The buffer must remain valid until IsBusy returns false. */
bool APP_SmartDMAStartTx(uint8_t *txBuffer, uint32_t txCount);
bool APP_SmartDMAStartRx(uint8_t *rxBuffer, uint32_t rxCount);

/* Request cancellation of the active TX or RX transfer. */
bool APP_SmartDMAAbort(void);

/* Return true until the SmartDMA completion interrupt has been handled. */
bool APP_SmartDMAIsBusy(void);
uint32_t APP_SmartDMAGetRxRemaining(void);
void APP_SmartDMASetCallback(app_smartdma_callback_t callback, void *userData);
void APP_SmartDMAGetDebugSnapshot(app_smartdma_debug_snapshot_t *snapshot);

#endif /* APP_SMARTDMA_H_ */
