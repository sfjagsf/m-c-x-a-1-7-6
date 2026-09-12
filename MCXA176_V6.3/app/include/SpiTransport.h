/* Common blocking LPSPI transport for board SPI buses. */
#ifndef APP_INCLUDE_SPI_TRANSPORT_H_
#define APP_INCLUDE_SPI_TRANSPORT_H_

#include <stddef.h>
#include <stdint.h>

#include "fsl_lpspi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    LPSPI_Type *base;
    uint32_t pcsConfigFlag;
} SpiTransport_t;

extern const SpiTransport_t g_spi0Transport;
extern const SpiTransport_t g_spi1Transport;

/* A raw full-duplex transaction.  A NULL rxData discards received bytes. */
status_t SpiTransportTransfer(const SpiTransport_t *bus,
                              const uint8_t *txData,
                              uint8_t *rxData,
                              size_t dataSize);

/* A transmit-only transaction.  PCS is asserted for all data bytes. */
status_t SpiTransportTransmit(const SpiTransport_t *bus,
                              const uint8_t *data,
                              size_t dataSize);

#ifdef __cplusplus
}
#endif

#endif /* APP_INCLUDE_SPI_TRANSPORT_H_ */
