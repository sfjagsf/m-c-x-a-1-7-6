/* Common UART transport driver public interface. */
#ifndef UART_DRIVER_H_
#define UART_DRIVER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

/* Four logical ports are reserved; every port owns one RX and one TX buffer. */
#define UART_PORT_COUNT  (4U)
#define UART_BUFFER_SIZE (256U)

/* Compatibility names used by the existing UART0 application. */
#define UART0_RX_BUFFER_SIZE UART_BUFFER_SIZE
#define UART0_TX_BUFFER_SIZE UART_BUFFER_SIZE

typedef enum
{
    kUartPort0 = 0U,
    kUartPort1,
    kUartPort2,
    kUartPort3,
} uart_port_id_t;

typedef struct
{
    uint32_t uartErrors;       /* Accumulated kLPUART_* line-error flags. */
    uint32_t uartErrorCount;   /* Number of UART line-error IRQs. */
    uint32_t dmaChannelErrors; /* Last DMA channel CH_ES value. */
    uint32_t dmaGlobalErrors;  /* Last DMA module MP_ES value. */
    uint32_t dmaErrorCount;
    uint32_t discardedFrameCount; /* Frames deliberately discarded after RX errors. */
    uint32_t lastRxRemaining;
    uint32_t lastRxLength;
    status_t lastDriverStatus;
} uart_diagnostics_t;

/* Old name remains valid so existing code does not need to change. */
typedef uart_diagnostics_t uart0_diagnostics_t;

/* Generic core API. Port-specific functions below are the normal application API. */
void UartPort_Init(uart_port_id_t port);
status_t UartPort_StartReceive(uart_port_id_t port);
status_t UartPort_Send(uart_port_id_t port, const uint8_t *data, size_t size);
status_t UartPort_Reply(uart_port_id_t port, const uint8_t *data, size_t size);
void UartPort_Abort(uart_port_id_t port);
bool UartPort_IsBusy(uart_port_id_t port);
bool UartPort_IsFrameAvailable(uart_port_id_t port);
/* Returned RX data is valid until ReleaseFrame/Reply starts the next receive. */
const uint8_t *UartPort_GetFrame(uart_port_id_t port, size_t *length);
/* RX buffers are not cleared; length defines valid bytes and the next DMA overwrites them. */
status_t UartPort_ReleaseFrame(uart_port_id_t port);
uint32_t UartPort_GetAndClearErrors(uart_port_id_t port);
void UartPort_GetDiagnostics(uart_port_id_t port, uart_diagnostics_t *diagnostics);
void UartPort_ClearDiagnostics(uart_port_id_t port);

/* Application code uses UartN_* and never supplies a port number. */
#define UART_DECLARE_PORT_API(n)                                                            \
    void Uart##n##_Init(void);                                                              \
    status_t Uart##n##_StartReceive(void);                                                  \
    status_t Uart##n##_Send(const uint8_t *data, size_t size);                              \
    status_t Uart##n##_Reply(const uint8_t *data, size_t size);                             \
    void Uart##n##_Abort(void);                                                             \
    bool Uart##n##_IsBusy(void);                                                            \
    bool Uart##n##_IsFrameAvailable(void);                                                  \
    const uint8_t *Uart##n##_GetFrame(size_t *length);                                      \
    status_t Uart##n##_ReleaseFrame(void);                                                  \
    uint32_t Uart##n##_GetAndClearErrors(void);                                             \
    void Uart##n##_GetDiagnostics(uart_diagnostics_t *diagnostics);                         \
    void Uart##n##_ClearDiagnostics(void)

UART_DECLARE_PORT_API(0);
UART_DECLARE_PORT_API(1);
UART_DECLARE_PORT_API(2);
UART_DECLARE_PORT_API(3);

#undef UART_DECLARE_PORT_API

#endif /* UART_DRIVER_H_ */
