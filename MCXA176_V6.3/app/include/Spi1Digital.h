/* SPI1 differential digital-output interface. */
#ifndef APP_INCLUDE_SPI1_DIGITAL_H_
#define APP_INCLUDE_SPI1_DIGITAL_H_

#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sends one external digital frame.
 *
 * During this call, LPSPI1_PCS0 drives SYNC active, LPSPI1_SCK drives CLK,
 * and LPSPI1_SDO drives both differential DATA outputs through AM26C31.
 * SYNC returns inactive after the final byte.
 */
status_t Spi1DigitalSendFrame(const uint8_t *data, size_t dataSize);

#ifdef __cplusplus
}
#endif

#endif /* APP_INCLUDE_SPI1_DIGITAL_H_ */
