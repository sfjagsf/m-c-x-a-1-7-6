#include "Spi1PointDma.h"

#include "fsl_ctimer.h"
#include "fsl_edma.h"
#include "fsl_lpspi.h"
#include "peripherals.h"

static volatile bool s_spi1PointDmaRunning;

status_t Spi1PointDmaStart(const uint16_t *points, uint32_t pointCount)
{
    edma_transfer_config_t transfer;
    status_t status;
    uint32_t totalBytes;

    if ((points == NULL) || (pointCount == 0U) || (pointCount > SPI1_POINT_DMA_MAX_POINTS))
    {
        return kStatus_InvalidArgument;
    }

    totalBytes = pointCount * sizeof(points[0]);

    /* No CTIMER request may arrive while CH4's TCD is being replaced. */
    CTIMER_StopTimer(CTIMER0_PERIPHERAL);
    EDMA_AbortTransfer(&DMA0_CH4_Handle);
    EDMA_ClearChannelStatusFlags(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL,
                                 (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                     (uint32_t)kEDMA_InterruptFlag);
    CTIMER_Reset(CTIMER0_PERIPHERAL);
    s_spi1PointDmaRunning = false;

    /* One CTIMER0_M0 request transfers exactly one 16-bit point to LPSPI1. */
    EDMA_PrepareTransferConfig(&transfer, (void *)points, sizeof(points[0]), sizeof(points[0]),
                               (void *)LPSPI_GetTxRegisterAddress(LPSPI1), sizeof(uint16_t), 0,
                               sizeof(uint16_t), totalBytes);

    /* At the end of the table, restore the source address to points[0]. */
    transfer.srcMajorLoopOffset = -(int32_t)totalBytes;
    transfer.dstMajorLoopOffset = 0;
    transfer.enabledInterruptMask = 0U;

    status = EDMA_SubmitTransfer(&DMA0_CH4_Handle, &transfer);
    if (status != kStatus_Success)
    {
        return status;
    }

    /* Keep CTIMER0 requests enabled across each completed major loop. */
    EDMA_EnableAutoStopRequest(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL, false);
    EDMA_EnableChannelRequest(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL);
    EDMA_StartTransfer(&DMA0_CH4_Handle);

    /* The first word is emitted on the first Match0, not during setup. */
    CTIMER_StartTimer(CTIMER0_PERIPHERAL);
    s_spi1PointDmaRunning = true;

    return kStatus_Success;
}

void Spi1PointDmaStop(void)
{
    CTIMER_StopTimer(CTIMER0_PERIPHERAL);
    EDMA_AbortTransfer(&DMA0_CH4_Handle);
    CTIMER_Reset(CTIMER0_PERIPHERAL);
    s_spi1PointDmaRunning = false;
}

bool Spi1PointDmaIsRunning(void)
{
    return s_spi1PointDmaRunning;
}
