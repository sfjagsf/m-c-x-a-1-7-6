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

/* Install the firmware in SRAMX0 and start the SmartDMA command processor. */
bool APP_SmartDMAInit(const app_smartdma_config_t *config);

/* Start one asynchronous transfer. The buffer must remain valid until IsBusy returns false. */
bool APP_SmartDMAStartTx(uint8_t *txBuffer, uint32_t txCount);
bool APP_SmartDMAStartRx(uint8_t *rxBuffer, uint32_t rxCount);

/* Request cancellation of the active TX or RX transfer. */
bool APP_SmartDMAAbort(void);

/* Return true until the SmartDMA completion interrupt has been handled. */
bool APP_SmartDMAIsBusy(void);

#endif /* APP_SMARTDMA_H_ */
