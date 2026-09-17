/* LPUART0 binding for the application-provided SmartDMA firmware. */
#ifndef APP_SMARTDMA_LPUART0_H_
#define APP_SMARTDMA_LPUART0_H_

#include <stdbool.h>

/* TX and RX each own one buffer.  The firmware permits 1 through 512 bytes. */
#define APP_SMARTDMA_LPUART0_BUFFER_SIZE (256U)

#ifdef __cplusplus
extern "C" {
#endif

/* Call after BOARD_InitBootPeripherals().  This only starts SmartDMA firmware. */
bool APP_SmartDMALPUART0_Init(void);
bool APP_SmartDMALPUART0_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SMARTDMA_LPUART0_H_ */
