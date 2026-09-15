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

/* Two eDMA TCDs form a ping-pong ring, allowing a whole table to change on a table boundary. */
#define SPI1_POINT_DMA_BUFFER_COUNT (2U)

typedef enum
{
    kSpi1OutputIdle = 0U,
    kSpi1OutputPointDmaRunning,
    kSpi1OutputDirectFrameBusy,
    kSpi1OutputFault
} spi1_output_state_t;

typedef struct
{
    uint32_t startCount;
    uint32_t stopCount;
    uint32_t dmaErrorCount;
    uint32_t txFifoErrorCount;
    status_t lastError;
} spi1_point_dma_diagnostics_t;

/*
 * Starts circular, CTIMER0-paced output of a 16-bit point table.
 *
 * BOARD_InitBootPeripherals() must have initialized LPSPI1, DMA0_CH4 and
 * CTIMER0 first. The table must remain valid and unchanged until Stop().
 */
status_t Spi1PointDmaStart(const uint16_t *points, uint32_t pointCount);

/*
 * Queues a replacement table without stopping CTIMER0.  It must be called
 * before the current table's final DMA request; kStatus_Busy means retry on
 * the next table cycle.  The replacement becomes permanent at a table edge.
 */
status_t Spi1PointDmaQueueNext(const uint16_t *points, uint32_t pointCount);

/* Stops CTIMER0 before aborting the DMA channel, then resets the timer phase. */
void Spi1PointDmaStop(void);

/* True only after a point table has been submitted and CTIMER0 was started. */
bool Spi1PointDmaIsRunning(void);

/* Checks DMA/LPSPI error flags and enters Fault if recovery was needed. */
bool Spi1PointDmaPoll(void);
spi1_output_state_t Spi1OutputGetState(void);
void Spi1PointDmaGetDiagnostics(spi1_point_dma_diagnostics_t *diagnostics);

/* Used by Spi1DigitalSendFrame(); only one SPI1 output owner is allowed. */
status_t Spi1OutputAcquireDirectFrame(void);
void Spi1OutputReleaseDirectFrame(status_t result);

/* Clears LPSPI1 and DMA0 CH4 after a fault, returning the output to Idle. */
void Spi1OutputRecover(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INCLUDE_SPI1_POINT_DMA_H_ */
