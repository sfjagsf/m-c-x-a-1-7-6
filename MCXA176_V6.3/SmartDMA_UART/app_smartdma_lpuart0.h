/* LPUART0 binding for the application-provided SmartDMA firmware. */
#ifndef APP_SMARTDMA_LPUART0_H_
#define APP_SMARTDMA_LPUART0_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

/* TX and RX each own one buffer.  The firmware permits 1 through 512 bytes. */
#define APP_SMARTDMA_LPUART0_BUFFER_SIZE (256U)

#ifdef __cplusplus
extern "C" {
#endif

/* Call after BOARD_InitBootPeripherals().  This only starts SmartDMA firmware. */
bool APP_SmartDMALPUART0_Init(void);
bool APP_SmartDMALPUART0_IsInitialized(void);
void APP_SmartDMALPUART0_TransportInit(void);
status_t APP_SmartDMALPUART0_StartReceive(void);
status_t APP_SmartDMALPUART0_Send(const uint8_t *data, size_t size, bool reply);
void APP_SmartDMALPUART0_Abort(void);
bool APP_SmartDMALPUART0_IsBusy(void);
bool APP_SmartDMALPUART0_IsFrameAvailable(void);
const uint8_t *APP_SmartDMALPUART0_GetFrame(size_t *length);
status_t APP_SmartDMALPUART0_ReleaseFrame(void);
uint32_t APP_SmartDMALPUART0_GetAndClearErrors(void);
void APP_SmartDMALPUART0_HandleLpuartIrq(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SMARTDMA_LPUART0_H_ */
