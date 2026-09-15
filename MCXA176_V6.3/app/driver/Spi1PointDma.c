#include "Spi1PointDma.h"

#include "fsl_ctimer.h"
#include "fsl_edma.h"
#include "fsl_lpspi.h"
#include "peripherals.h"

static volatile bool s_spi1PointDmaRunning;
static volatile spi1_output_state_t s_spi1OutputState;
static volatile uint8_t s_activeTcdIndex;
static volatile bool s_replacePending;
static const uint16_t *s_replacementPoints;
static uint32_t s_replacementPointCount;
static edma_transfer_config_t s_pointTransfers[SPI1_POINT_DMA_BUFFER_COUNT];
EDMA_ALLOCATE_TCD(s_spi1PointDmaTcd, SPI1_POINT_DMA_BUFFER_COUNT);
static volatile spi1_point_dma_diagnostics_t s_diagnostics;

#define SPI1_POINT_DMA_STOP_WAIT_LOOPS (100000U)
#define SPI1_POINT_DMA_SAFETY_MARGIN_NS (1000ULL)

static bool Spi1PointDmaParametersAreValid(const uint16_t *points, uint32_t pointCount)
{
    return (points != NULL) && (pointCount != 0U) && (pointCount <= SPI1_POINT_DMA_MAX_POINTS);
}

static bool Spi1OutputTrySetState(spi1_output_state_t expected, spi1_output_state_t desired)
{
    const uint32_t irqMask = DisableGlobalIRQ();
    const bool acquired = s_spi1OutputState == expected;

    if (acquired)
    {
        s_spi1OutputState = desired;
    }
    EnableGlobalIRQ(irqMask);
    return acquired;
}

static void Spi1PointDmaSetFault(status_t error, bool dmaError, bool txFifoError)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    if (dmaError)
    {
        s_diagnostics.dmaErrorCount++;
    }
    if (txFifoError)
    {
        s_diagnostics.txFifoErrorCount++;
    }
    s_diagnostics.lastError = error;
    s_spi1OutputState = kSpi1OutputFault;
    EnableGlobalIRQ(irqMask);
}

static bool Spi1PointDmaPeriodIsSafe(void)
{
    uint64_t frameNs;
    const uint64_t pointPeriodNs =
        ((uint64_t)(CTIMER0_Match_0_config.matchValue + 1UL) *
         (uint64_t)(CTIMER0_config.prescale + 1UL) * 1000000000ULL) /
        (uint64_t)CTIMER0_TICK_FREQ;
    if ((LPSPI1_config.bitsPerFrame != 16U) || (LPSPI1_config.baudRate == 0U))
    {
        return false;
    }
    frameNs = ((uint64_t)LPSPI1_config.bitsPerFrame * 1000000000ULL) /
              (uint64_t)LPSPI1_config.baudRate +
              (uint64_t)LPSPI1_config.pcsToSckDelayInNanoSec +
              (uint64_t)LPSPI1_config.lastSckToPcsDelayInNanoSec;
    return pointPeriodNs > (frameNs + SPI1_POINT_DMA_SAFETY_MARGIN_NS);
}

static void Spi1PointDmaPrepareConfig(edma_transfer_config_t *transfer,
                                       const uint16_t *points,
                                       uint32_t pointCount)
{
    EDMA_PrepareTransferConfig(transfer, (void *)points, sizeof(points[0]), sizeof(points[0]),
                               (void *)LPSPI_GetTxRegisterAddress(LPSPI1), sizeof(uint16_t), 0,
                               sizeof(uint16_t), pointCount * sizeof(points[0]));
    transfer->enabledInterruptMask = (uint32_t)kEDMA_MajorInterruptEnable;
}

static void Spi1PointDmaPrepareTcd(uint8_t index, const uint16_t *points, uint32_t pointCount)
{
    const uint8_t nextIndex = index ^ 1U;

    EDMA_PrepareTransferTCD(&DMA0_CH4_Handle, &s_spi1PointDmaTcd[index], (void *)points,
                            sizeof(points[0]), sizeof(points[0]),
                            (void *)LPSPI_GetTxRegisterAddress(LPSPI1), sizeof(uint16_t), 0,
                            sizeof(uint16_t), pointCount * sizeof(points[0]),
                            &s_spi1PointDmaTcd[nextIndex]);
}

static void Spi1PointDmaDmaCallback(edma_handle_t *handle, void *userData, bool transferDone, uint32_t tcds)
{
    uint8_t reusableTcd;

    (void)handle;
    (void)userData;
    (void)tcds;

    if (!transferDone)
    {
        CTIMER_StopTimer(CTIMER0_PERIPHERAL);
        EDMA_DisableChannelRequest(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL);
        s_spi1PointDmaRunning = false;
        Spi1PointDmaSetFault(kStatus_Fail, true, false);
        return;
    }

    /* The descriptor that just completed is now safe to prepare for a later cycle. */
    s_activeTcdIndex ^= 1U;
    if (s_replacePending)
    {
        reusableTcd = s_activeTcdIndex ^ 1U;
        Spi1PointDmaPrepareTcd(reusableTcd, s_replacementPoints, s_replacementPointCount);
        s_replacePending = false;
    }
}

void DMA_CH4_IRQHandler(void)
{
    EDMA_HandleIRQ(&DMA0_CH4_Handle);
}

status_t Spi1PointDmaStart(const uint16_t *points, uint32_t pointCount)
{
    status_t status;

    if (!Spi1PointDmaParametersAreValid(points, pointCount) || !Spi1PointDmaPeriodIsSafe())
    {
        return kStatus_InvalidArgument;
    }
    if (!Spi1OutputTrySetState(kSpi1OutputIdle, kSpi1OutputPointDmaRunning))
    {
        return kStatus_Busy;
    }

    /* No CTIMER request may arrive while CH4's TCD is being replaced. */
    CTIMER_StopTimer(CTIMER0_PERIPHERAL);
    EDMA_AbortTransfer(&DMA0_CH4_Handle);
    EDMA_ClearChannelStatusFlags(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL,
                                 (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                     (uint32_t)kEDMA_InterruptFlag);
    CTIMER_Reset(CTIMER0_PERIPHERAL);
    s_spi1PointDmaRunning = false;
    s_replacePending = false;

    /*
     * DMA writes TDR directly and therefore bypasses LPSPI_MasterTransfer().
     * Keep PCS0 (P2_17 / SPI1_CS) under LPSPI hardware control: CONT=0 makes
     * each 16-bit point one complete, active-low PCS pulse.  The board has no
     * LPSPI1 receive path, so mask RX to prevent its FIFO from filling.
     */
    LPSPI1->TCR = (LPSPI_GetTcr(LPSPI1) &
                    ~(LPSPI_TCR_CONT_MASK | LPSPI_TCR_CONTC_MASK | LPSPI_TCR_RXMSK_MASK |
                      LPSPI_TCR_TXMSK_MASK | LPSPI_TCR_PCS_MASK | LPSPI_TCR_FRAMESZ_MASK)) |
                   LPSPI_TCR_RXMSK(1U) | LPSPI_TCR_PCS((uint32_t)kLPSPI_Pcs0) |
                   LPSPI_TCR_FRAMESZ(15U);

    Spi1PointDmaPrepareConfig(&s_pointTransfers[0], points, pointCount);
    Spi1PointDmaPrepareConfig(&s_pointTransfers[1], points, pointCount);
    EDMA_InstallTCDMemory(&DMA0_CH4_Handle, s_spi1PointDmaTcd, SPI1_POINT_DMA_BUFFER_COUNT);
    EDMA_SetCallback(&DMA0_CH4_Handle, Spi1PointDmaDmaCallback, NULL);
    s_activeTcdIndex = 0U;

    /* Two linked descriptors make an endless A->A ring and leave one safe to replace. */
    status = EDMA_SubmitLoopTransfer(&DMA0_CH4_Handle, s_pointTransfers, SPI1_POINT_DMA_BUFFER_COUNT);
    if (status != kStatus_Success)
    {
        s_spi1OutputState = kSpi1OutputIdle;
        return status;
    }

    /* Keep CTIMER0 requests enabled across each completed major loop. */
    EDMA_EnableAutoStopRequest(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL, false);
    EDMA_EnableChannelRequest(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL);
    EDMA_EnableChannelInterrupts(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL,
                                 (uint32_t)kEDMA_ErrorInterruptEnable);
    EnableIRQ(DMA0_DMA_CH_INT_DONE_4_IRQN);
    EDMA_StartTransfer(&DMA0_CH4_Handle);

    /* The first word is emitted on the first Match0, not during setup. */
    CTIMER_StartTimer(CTIMER0_PERIPHERAL);
    s_spi1PointDmaRunning = true;
    s_diagnostics.startCount++;

    return kStatus_Success;
}

status_t Spi1PointDmaQueueNext(const uint16_t *points, uint32_t pointCount)
{
    uint32_t irqMask;
    uint8_t nextTcd;

    if (!Spi1PointDmaParametersAreValid(points, pointCount))
    {
        return kStatus_InvalidArgument;
    }

    irqMask = DisableGlobalIRQ();
    if (!s_spi1PointDmaRunning || s_replacePending ||
        (EDMA_GetRemainingMajorLoopCount(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL) <= 1U))
    {
        EnableGlobalIRQ(irqMask);
        return kStatus_Busy;
    }

    nextTcd = s_activeTcdIndex ^ 1U;
    s_replacementPoints = points;
    s_replacementPointCount = pointCount;
    Spi1PointDmaPrepareTcd(nextTcd, points, pointCount);
    s_replacePending = true;
    EnableGlobalIRQ(irqMask);

    return kStatus_Success;
}

void Spi1PointDmaStop(void)
{
    uint32_t waitLoops = SPI1_POINT_DMA_STOP_WAIT_LOOPS;

    CTIMER_StopTimer(CTIMER0_PERIPHERAL);
    EDMA_DisableChannelRequest(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL);
    EDMA_AbortTransfer(&DMA0_CH4_Handle);
    while (((LPSPI_GetStatusFlags(LPSPI1) & (uint32_t)kLPSPI_ModuleBusyFlag) != 0U) && (--waitLoops != 0U))
    {
    }
    LPSPI_FlushFifo(LPSPI1, true, true);
    LPSPI_ClearStatusFlags(LPSPI1, (uint32_t)kLPSPI_AllStatusFlag);
    CTIMER_Reset(CTIMER0_PERIPHERAL);
    s_spi1PointDmaRunning = false;
    s_replacePending = false;
    s_diagnostics.stopCount++;
    if (s_spi1OutputState != kSpi1OutputFault)
    {
        s_spi1OutputState = kSpi1OutputIdle;
    }
}

bool Spi1PointDmaIsRunning(void)
{
    return s_spi1PointDmaRunning;
}

bool Spi1PointDmaPoll(void)
{
    const uint32_t dmaFlags = EDMA_GetChannelStatusFlags(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL);
    const uint32_t lpspiFlags = LPSPI_GetStatusFlags(LPSPI1);

    if ((dmaFlags & (uint32_t)kEDMA_ErrorFlag) != 0U)
    {
        Spi1PointDmaStop();
        Spi1PointDmaSetFault(kStatus_Fail, true, false);
        return false;
    }
    if ((lpspiFlags & (uint32_t)kLPSPI_TransmitErrorFlag) != 0U)
    {
        Spi1PointDmaStop();
        Spi1PointDmaSetFault(kStatus_Fail, false, true);
        return false;
    }
    return s_spi1OutputState != kSpi1OutputFault;
}

spi1_output_state_t Spi1OutputGetState(void)
{
    return s_spi1OutputState;
}

void Spi1PointDmaGetDiagnostics(spi1_point_dma_diagnostics_t *diagnostics)
{
    uint32_t irqMask;

    if (diagnostics == NULL)
    {
        return;
    }
    irqMask = DisableGlobalIRQ();
    *diagnostics = s_diagnostics;
    EnableGlobalIRQ(irqMask);
}

status_t Spi1OutputAcquireDirectFrame(void)
{
    return Spi1OutputTrySetState(kSpi1OutputIdle, kSpi1OutputDirectFrameBusy) ? kStatus_Success : kStatus_Busy;
}

void Spi1OutputReleaseDirectFrame(status_t result)
{
    if (result == kStatus_Success)
    {
        (void)Spi1OutputTrySetState(kSpi1OutputDirectFrameBusy, kSpi1OutputIdle);
    }
    else
    {
        Spi1PointDmaSetFault(result, false, false);
    }
}

void Spi1OutputRecover(void)
{
    Spi1PointDmaStop();
    EDMA_ClearChannelStatusFlags(DMA0_DMA_BASEADDR, DMA0_CH4_DMA_CHANNEL,
                                 (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                     (uint32_t)kEDMA_InterruptFlag);
    s_diagnostics.lastError = kStatus_Success;
    s_spi1OutputState = kSpi1OutputIdle;
}
