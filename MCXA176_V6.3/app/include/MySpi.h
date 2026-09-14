/*
 * SPI0 transaction adapter for the W25Qxx NOR flash.
 *
 * This is the MCXA/LPSPI equivalent of the reference project's my_spi layer:
 * it keeps PCS0 asserted while both data segments are clocked.  Calls are
 * serialized; a concurrent call returns kStatus_Busy.
 */
#ifndef APP_INCLUDE_MY_SPI_H_
#define APP_INCLUDE_MY_SPI_H_

#include <stddef.h>
#include <stdint.h>

#include "SpiTransport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One W25Qxx page-program transaction: command + 24-bit address + 256 bytes. */
#define MY_SPI0_MAX_TRANSACTION_BYTES (261U)

status_t MY_SPI0_Transmit(const uint8_t *firstData,
                          size_t firstSize,
                          const uint8_t *secondData,
                          size_t secondSize);

status_t MY_SPI0_TransmitReceive(const uint8_t *txData,
                                 size_t txSize,
                                 uint8_t *rxData,
                                 size_t rxSize);

#ifdef __cplusplus
}
#endif

#endif /* APP_INCLUDE_MY_SPI_H_ */
