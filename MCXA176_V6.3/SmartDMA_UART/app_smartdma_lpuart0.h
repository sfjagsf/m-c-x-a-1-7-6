/* LPUART0 binding for the application-provided SmartDMA firmware. */
#ifndef APP_SMARTDMA_LPUART0_H_
#define APP_SMARTDMA_LPUART0_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"
#include "app_smartdma.h"

/* TX and RX each own one buffer.  The firmware permits 1 through 512 bytes. */
#define APP_SMARTDMA_LPUART0_BUFFER_SIZE (256U)
#define APP_SMARTDMA_LPUART0_DEBUG_BYTES (16U)

typedef enum
{
    kAppUart0EventRx = 1U,
    kAppUart0EventTx = 2U,
    kAppUart0EventError = 3U,
    kAppUart0EventRecovery = 4U,
    kAppUart0EventTxWireComplete = 5U,
    kAppUart0EventReplyFailure = 6U,
    kAppUart0EventTxStartFailure = 7U,
} app_uart0_event_kind_t;

typedef struct
{
    uint32_t id;
    uint32_t kind;
    uint32_t rxFrameCount;
    uint32_t txRequestCount;
    uint32_t recoveryCount;
    uint32_t rxState;
    uint32_t txState;
    uint32_t length;
    uint32_t flags;
    uint32_t status;
    uint32_t rxRemaining;
    uint32_t direction;
    uint8_t bytes[APP_SMARTDMA_LPUART0_DEBUG_BYTES];
} app_uart0_event_t;

typedef struct
{
    uint32_t initialized;
    uint32_t rxState;
    uint32_t txState;
    uint32_t rxLength;
    uint32_t pendingTxLength;
    uint32_t lastRxLength;
    uint32_t lastTxLength;
    uint8_t lastRxBytes[APP_SMARTDMA_LPUART0_DEBUG_BYTES];
    uint8_t lastTxBytes[APP_SMARTDMA_LPUART0_DEBUG_BYTES];
    uint32_t errors;
    uint32_t rxStartCount;
    uint32_t rxFrameCount;
    uint32_t txRequestCount;
    uint32_t txStartCount;
    uint32_t txCompleteCount;
    uint32_t txWireCompleteCount;
    uint32_t abortCompleteCount;
    uint32_t breakCount;
    uint32_t rxRecoveryCount;
    uint32_t recoveryPending;
    uint32_t eventDropCount;
    uint32_t replyFailureCount;
    uint32_t lpuartStat;
    uint32_t lpuartCtrl;
    uint32_t lpuartBaud;
    uint32_t lpuartFifo;
    uint32_t lpuartWater;
    app_smartdma_debug_snapshot_t smartdma;
} app_smartdma_lpuart0_debug_snapshot_t;

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
void APP_SmartDMALPUART0_GetDebugSnapshot(app_smartdma_lpuart0_debug_snapshot_t *snapshot);
bool APP_SmartDMALPUART0_PopEvent(app_uart0_event_t *event);
void APP_SmartDMALPUART0_RecordReplyFailure(status_t status);
void APP_SmartDMALPUART0_Service(void);
void APP_SmartDMALPUART0_HandleLpuartIrq(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SMARTDMA_LPUART0_H_ */
