/* LPUART0 resources used by the SmartDMA LPUART firmware. */
#include "app_smartdma_lpuart0.h"

#include <stdint.h>

#include "app_smartdma.h"
#include "fsl_device_registers.h"

_Static_assert(APP_SMARTDMA_LPUART0_BUFFER_SIZE > 0U, "SmartDMA buffer must not be empty");
_Static_assert(APP_SMARTDMA_LPUART0_BUFFER_SIZE <= APP_SMARTDMA_MAX_TRANSFER_SIZE,
               "SmartDMA buffer exceeds firmware limit");

static uint8_t s_lpuart0SmartDmaTx[APP_SMARTDMA_LPUART0_BUFFER_SIZE];
static uint8_t s_lpuart0SmartDmaRx[APP_SMARTDMA_LPUART0_BUFFER_SIZE];
static bool s_lpuart0SmartDmaInitialized;

bool APP_SmartDMALPUART0_Init(void)
{
    const app_smartdma_config_t config = {
        .txBuffer = s_lpuart0SmartDmaTx,
        .txBufferSize = sizeof(s_lpuart0SmartDmaTx),
        .txDataRegister = &LPUART0->DATA,
        .rxBuffer = s_lpuart0SmartDmaRx,
        .rxBufferSize = sizeof(s_lpuart0SmartDmaRx),
        .rxDataRegister = &LPUART0->DATA,
    };

    s_lpuart0SmartDmaInitialized = APP_SmartDMAInit(&config);
    return s_lpuart0SmartDmaInitialized;
}

bool APP_SmartDMALPUART0_IsInitialized(void)
{
    return s_lpuart0SmartDmaInitialized;
}
