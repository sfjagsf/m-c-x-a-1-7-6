/*
 * Timer-paced SPI1 point-table output.
 *
 * CTIMER0 Match0 is the DMA request source. Every timer match transfers one
 * uint16_t from the caller's table into LPSPI1 TDR through DMA0 channel 4.
 */
#ifndef APP_INCLUDE_SPI1_POINT_DMA_H_
#define APP_INCLUDE_SPI1_POINT_DMA_H_

#include <stdbool.h>
#include <stdint.h>

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* eDMA major-loop count is limited to 15 bits when channel linking is off. */
#define SPI1_POINT_DMA_MAX_POINTS (32767U)

/*
 * Starts circular, CTIMER0-paced output of a 16-bit point table.
 *
 * BOARD_InitBootPeripherals() must have initialized LPSPI1, DMA0_CH4 and
 * CTIMER0 first. The table must remain valid and unchanged until Stop().
 */
status_t Spi1PointDmaStart(const uint16_t *points, uint32_t pointCount);

/* Stops CTIMER0 before aborting the DMA channel, then resets the timer phase. */
void Spi1PointDmaStop(void);

/* True only after a point table has been submitted and CTIMER0 was started. */
bool Spi1PointDmaIsRunning(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INCLUDE_SPI1_POINT_DMA_H_ */
