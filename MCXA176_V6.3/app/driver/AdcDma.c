#include "AdcDma.h"

#include "fsl_clock.h"
#include "fsl_ctimer.h"
#include "fsl_edma.h"
#include "fsl_lpadc.h"
#include "peripherals.h"

#define ADC_DMA_INPUTMUX_CTIMER3_MAT1 (0x2AU)
#define ADC_DMA_TIMER_PWM_TICKS       ((CTIMER3_Match_0_config.matchValue + 1UL) / 2UL)
/* Standard single-ended LPADC conversion is 12-bit left-aligned in RESFIFO.D[14:3]. */
#define ADC_DMA_STANDARD_RESULT_SHIFT (3U)
#define ADC_DMA_FIRST_COMMAND_ID      (1U)

/*
 * RESFIFO words remain 32-bit so VALID and CMDSRC are always checked.
 * Put DMA destinations in non-cacheable, cache-line-aligned memory. This is
 * harmless on cacheless variants and prevents stale data on cacheable ones.
 */
AT_NONCACHEABLE_SECTION_ALIGN(static uint32_t s_adc0Blocks[ADC_DMA_BUFFER_COUNT][ADC_DMA_BLOCK_WORD_COUNT], 32U);
AT_NONCACHEABLE_SECTION_ALIGN(static uint32_t s_adc1Blocks[ADC_DMA_BUFFER_COUNT][ADC_DMA_BLOCK_WORD_COUNT], 32U);
static uint16_t s_latestRaw[ADC_DMA_CHANNEL_COUNT];
static edma_transfer_config_t s_adc0Transfers[ADC_DMA_BUFFER_COUNT];
static edma_transfer_config_t s_adc1Transfers[ADC_DMA_BUFFER_COUNT];
EDMA_ALLOCATE_TCD(s_adc0Tcd, ADC_DMA_BUFFER_COUNT);
EDMA_ALLOCATE_TCD(s_adc1Tcd, ADC_DMA_BUFFER_COUNT);

static volatile uint32_t s_adc0CompletedBlocks;
static volatile uint32_t s_adc1CompletedBlocks;
static volatile bool s_initialized;
static volatile bool s_running;
static uint32_t s_lastConsumedPair;
static bool s_latestValid;
static adc_dma_diagnostics_t s_diagnostics;

static void AdcDmaDrainFifo(ADC_Type *base)
{
    lpadc_conv_result_t ignored;

    while (LPADC_GetConvResult(base, &ignored))
    {
        /* Drain pre-start or fault residues before arming a new DMA ring. */
    }
}

static void AdcDmaConfigureInputMux(void)
{
    CLOCK_EnableClock(kCLOCK_InputMux);
    INPUTMUX0->ADC0_TRIG[ADC0_ADC0_TRIG0] =
        INPUTMUX_ADC0_TRIGM_ADC0_TRIG_TRIGIN(ADC_DMA_INPUTMUX_CTIMER3_MAT1);
    INPUTMUX0->ADC1_TRIG[ADC1_ADC1_TRIG0] =
        INPUTMUX_ADC1_TRIGM_ADC1_TRIG_TRIGIN(ADC_DMA_INPUTMUX_CTIMER3_MAT1);
}

static bool AdcDmaConfigureTimer(void)
{
    CTIMER_StopTimer(CTIMER3_PERIPHERAL);
    CTIMER_Reset(CTIMER3_PERIPHERAL);

    /* M0 defines the 1 ms period. M1 is internal-only; it is not pin-muxed. */
    CTIMER_SetupMatch(CTIMER3_PERIPHERAL, CTIMER3_MATCH_0_CHANNEL, &CTIMER3_Match_0_config);
    if (CTIMER_SetupPwmPeriod(CTIMER3_PERIPHERAL, CTIMER3_MATCH_0_CHANNEL,
                              kCTIMER_Match_1, CTIMER3_Match_0_config.matchValue,
                              ADC_DMA_TIMER_PWM_TICKS, false) != kStatus_Success)
    {
        s_diagnostics.lastError = kAdcDmaErrorTimer;
        return false;
    }

    return true;
}

static void AdcDmaPrepareTransfer(edma_transfer_config_t *transfer, ADC_Type *adc, uint32_t *destination)
{
    EDMA_PrepareTransferConfig(transfer, (void *)&adc->RESFIFO, sizeof(uint32_t), 0,
                               (void *)destination, sizeof(uint32_t), sizeof(uint32_t),
                               sizeof(uint32_t), ADC_DMA_BLOCK_WORD_COUNT * sizeof(uint32_t));
    transfer->enabledInterruptMask = (uint32_t)kEDMA_MajorInterruptEnable;
}

static void AdcDmaCallback(edma_handle_t *handle, void *userData, bool transferDone, uint32_t tcds)
{
    volatile uint32_t *completedBlocks = (volatile uint32_t *)userData;

    (void)handle;
    (void)transferDone;
    (void)tcds;

    /*
     * A loop transfer may load its next TCD before this ISR observes CH_CSR.DONE.
     * Thus transferDone == false is normal for a circular SG ring, not a fault.
     * This IRQ is enabled only for major-loop completion, so it marks one finished
     * 70-word ADC block.  AdcDmaPoll() independently checks CH1/CH2 CH_ES.
     */
    (*completedBlocks)++;
}

static void AdcDmaAbortHardware(void)
{
    /* Stop new trigger edges before disabling the FIFO-request DMA channels. */
    CTIMER_StopTimer(CTIMER3_PERIPHERAL);
    EDMA_DisableChannelRequest(DMA0_DMA_BASEADDR, DMA0_CH1_DMA_CHANNEL);
    EDMA_DisableChannelRequest(DMA0_DMA_BASEADDR, DMA0_CH2_DMA_CHANNEL);
    EDMA_AbortTransfer(&DMA0_CH1_Handle);
    EDMA_AbortTransfer(&DMA0_CH2_Handle);
    AdcDmaDrainFifo(ADC0);
    AdcDmaDrainFifo(ADC1);
    CTIMER_Reset(CTIMER3_PERIPHERAL);
}

static void AdcDmaFault(adc_dma_error_t error)
{
    AdcDmaAbortHardware();
    s_running = false;
    s_diagnostics.lastError = error;
}

static bool AdcDmaAccumulateBlock(const uint32_t *results, uint32_t *sums, uint16_t *minimums, uint16_t *maximums)
{
    uint8_t sampleCount[ADC_DMA_ADC0_CHANNEL_COUNT] = {0U};
    uint32_t sampleIndex;

    for (sampleIndex = 0U; sampleIndex < ADC_DMA_BLOCK_WORD_COUNT; sampleIndex++)
    {
        const uint32_t result = results[sampleIndex];
        const uint8_t command = (uint8_t)((result & ADC_RESFIFO_CMDSRC_MASK) >> ADC_RESFIFO_CMDSRC_SHIFT);
        const uint32_t channel = (uint32_t)command - ADC_DMA_FIRST_COMMAND_ID;

        /* Both generated command chains use contiguous CMD1..CMD7. */
        if (((result & ADC_RESFIFO_VALID_MASK) == 0U) || (command < ADC_DMA_FIRST_COMMAND_ID) ||
            (channel >= ADC_DMA_ADC0_CHANNEL_COUNT) || (sampleCount[channel] >= ADC_DMA_SAMPLES_PER_CHANNEL))
        {
            s_diagnostics.resultTagErrorCount++;
            s_diagnostics.lastError = kAdcDmaErrorResultTag;
            return false;
        }

        const uint16_t value = (uint16_t)((result & ADC_RESFIFO_D_MASK) >> ADC_DMA_STANDARD_RESULT_SHIFT);

        sums[channel] += value;
        if (value < minimums[channel])
        {
            minimums[channel] = value;
        }
        if (value > maximums[channel])
        {
            maximums[channel] = value;
        }
        sampleCount[channel]++;
    }

    for (sampleIndex = 0U; sampleIndex < ADC_DMA_ADC0_CHANNEL_COUNT; sampleIndex++)
    {
        if (sampleCount[sampleIndex] != ADC_DMA_SAMPLES_PER_CHANNEL)
        {
            s_diagnostics.resultTagErrorCount++;
            s_diagnostics.lastError = kAdcDmaErrorResultTag;
            return false;
        }
    }

    return true;
}

static bool AdcDmaUpdateLatest(void)
{
    uint32_t adc0Completed;
    uint32_t adc1Completed;
    uint32_t pairCompleted;
    uint32_t sums[ADC_DMA_CHANNEL_COUNT] = {0U};
    uint16_t minimums[ADC_DMA_CHANNEL_COUNT];
    uint16_t maximums[ADC_DMA_CHANNEL_COUNT] = {0U};
    uint32_t channel;
    uint8_t blockIndex;
    uint32_t irqMask;

    irqMask = DisableGlobalIRQ();
    adc0Completed = s_adc0CompletedBlocks;
    adc1Completed = s_adc1CompletedBlocks;
    EnableGlobalIRQ(irqMask);
    pairCompleted = (adc0Completed < adc1Completed) ? adc0Completed : adc1Completed;

    if (pairCompleted == 0U)
    {
        s_diagnostics.lastError = kAdcDmaErrorNoData;
        return s_latestValid;
    }
    if (pairCompleted == s_lastConsumedPair)
    {
        return s_latestValid;
    }
    if ((pairCompleted - s_lastConsumedPair) > ADC_DMA_BUFFER_COUNT)
    {
        s_diagnostics.droppedBlockCount += (pairCompleted - s_lastConsumedPair) - ADC_DMA_BUFFER_COUNT;
        s_diagnostics.lastError = kAdcDmaErrorBufferOverrun;
    }

    for (channel = 0U; channel < ADC_DMA_CHANNEL_COUNT; channel++)
    {
        minimums[channel] = UINT16_MAX;
    }

    blockIndex = (uint8_t)((pairCompleted - 1U) & 1U);
    if ((!AdcDmaAccumulateBlock(s_adc0Blocks[blockIndex], &sums[0U], &minimums[0U], &maximums[0U])) ||
        (!AdcDmaAccumulateBlock(s_adc1Blocks[blockIndex], &sums[ADC_DMA_ADC0_CHANNEL_COUNT],
                                &minimums[ADC_DMA_ADC0_CHANNEL_COUNT], &maximums[ADC_DMA_ADC0_CHANNEL_COUNT])))
    {
        AdcDmaFault(kAdcDmaErrorResultTag);
        return false;
    }

    for (channel = 0U; channel < ADC_DMA_CHANNEL_COUNT; channel++)
    {
        s_latestRaw[channel] =
            (uint16_t)((sums[channel] - minimums[channel] - maximums[channel]) / ADC_DMA_FILTERED_SAMPLE_COUNT);
    }

    s_lastConsumedPair = pairCompleted;
    s_latestValid = true;
    s_diagnostics.acquisitionCount++;
    if (s_diagnostics.lastError != kAdcDmaErrorBufferOverrun)
    {
        s_diagnostics.lastError = kAdcDmaErrorNone;
    }
    return true;
}

void AdcDma_Init(void)
{
    AdcDmaConfigureInputMux();
    EDMA_SetCallback(&DMA0_CH1_Handle, AdcDmaCallback, (void *)&s_adc0CompletedBlocks);
    EDMA_SetCallback(&DMA0_CH2_Handle, AdcDmaCallback, (void *)&s_adc1CompletedBlocks);
    EnableIRQ(DMA0_DMA_CH_INT_DONE_1_IRQN);
    EnableIRQ(DMA0_DMA_CH_INT_DONE_2_IRQN);
    s_diagnostics.lastError = kAdcDmaErrorNone;
    s_initialized = true;
}

bool AdcDmaStartContinuous(void)
{
    status_t status;
    uint32_t index;

    if (!s_initialized)
    {
        s_diagnostics.lastError = kAdcDmaErrorNotInitialized;
        return false;
    }
    if (s_running)
    {
        return true;
    }
    if (!AdcDmaConfigureTimer())
    {
        return false;
    }

    AdcDmaAbortHardware();
    EDMA_ClearChannelStatusFlags(DMA0_DMA_BASEADDR, DMA0_CH1_DMA_CHANNEL,
                                 (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                     (uint32_t)kEDMA_InterruptFlag);
    EDMA_ClearChannelStatusFlags(DMA0_DMA_BASEADDR, DMA0_CH2_DMA_CHANNEL,
                                 (uint32_t)kEDMA_DoneFlag | (uint32_t)kEDMA_ErrorFlag |
                                     (uint32_t)kEDMA_InterruptFlag);
    LPADC_ClearStatusFlags(ADC0, kLPADC_ResultFIFO0OverflowFlag);
    LPADC_ClearStatusFlags(ADC1, kLPADC_ResultFIFO0OverflowFlag);

    for (index = 0U; index < ADC_DMA_BUFFER_COUNT; index++)
    {
        AdcDmaPrepareTransfer(&s_adc0Transfers[index], ADC0, s_adc0Blocks[index]);
        AdcDmaPrepareTransfer(&s_adc1Transfers[index], ADC1, s_adc1Blocks[index]);
    }
    s_adc0CompletedBlocks = 0U;
    s_adc1CompletedBlocks = 0U;
    s_lastConsumedPair = 0U;
    s_latestValid = false;

    EDMA_InstallTCDMemory(&DMA0_CH1_Handle, s_adc0Tcd, ADC_DMA_BUFFER_COUNT);
    EDMA_InstallTCDMemory(&DMA0_CH2_Handle, s_adc1Tcd, ADC_DMA_BUFFER_COUNT);
    status = EDMA_SubmitLoopTransfer(&DMA0_CH1_Handle, s_adc0Transfers, ADC_DMA_BUFFER_COUNT);
    if (status == kStatus_Success)
    {
        status = EDMA_SubmitLoopTransfer(&DMA0_CH2_Handle, s_adc1Transfers, ADC_DMA_BUFFER_COUNT);
    }
    if (status != kStatus_Success)
    {
        s_diagnostics.dmaErrorCount++;
        s_diagnostics.lastError = kAdcDmaErrorDmaSubmit;
        AdcDmaAbortHardware();
        return false;
    }

    /* Do not let the generated one-shot setting stop a double-buffer ring. */
    EDMA_EnableAutoStopRequest(DMA0_DMA_BASEADDR, DMA0_CH1_DMA_CHANNEL, false);
    EDMA_EnableAutoStopRequest(DMA0_DMA_BASEADDR, DMA0_CH2_DMA_CHANNEL, false);
    EDMA_EnableChannelRequest(DMA0_DMA_BASEADDR, DMA0_CH1_DMA_CHANNEL);
    EDMA_EnableChannelRequest(DMA0_DMA_BASEADDR, DMA0_CH2_DMA_CHANNEL);
    EDMA_EnableChannelInterrupts(DMA0_DMA_BASEADDR, DMA0_CH1_DMA_CHANNEL, (uint32_t)kEDMA_ErrorInterruptEnable);
    EDMA_EnableChannelInterrupts(DMA0_DMA_BASEADDR, DMA0_CH2_DMA_CHANNEL, (uint32_t)kEDMA_ErrorInterruptEnable);
    EDMA_StartTransfer(&DMA0_CH1_Handle);
    EDMA_StartTransfer(&DMA0_CH2_Handle);

    CTIMER_Reset(CTIMER3_PERIPHERAL);
    CTIMER_StartTimer(CTIMER3_PERIPHERAL);
    s_running = true;
    s_diagnostics.startCount++;
    s_diagnostics.lastError = kAdcDmaErrorNone;
    return true;
}

void AdcDmaStopContinuous(void)
{
    if (s_running)
    {
        AdcDmaAbortHardware();
        s_running = false;
        s_diagnostics.stopCount++;
    }
}

bool AdcDmaPoll(void)
{
    const uint32_t adc0DmaFlags = EDMA_GetChannelStatusFlags(DMA0_DMA_BASEADDR, DMA0_CH1_DMA_CHANNEL);
    const uint32_t adc1DmaFlags = EDMA_GetChannelStatusFlags(DMA0_DMA_BASEADDR, DMA0_CH2_DMA_CHANNEL);

    if (!s_running)
    {
        /* Preserve a more specific start/fault cause for field diagnostics. */
        if (s_diagnostics.lastError == kAdcDmaErrorNone)
        {
            s_diagnostics.lastError = kAdcDmaErrorNotRunning;
        }
        return false;
    }
    if (((adc0DmaFlags & (uint32_t)kEDMA_ErrorFlag) != 0U) ||
        ((adc1DmaFlags & (uint32_t)kEDMA_ErrorFlag) != 0U))
    {
        s_diagnostics.adc0DmaChannelErrorFlags = DMA0->CH[DMA0_CH1_DMA_CHANNEL].CH_ES;
        s_diagnostics.adc1DmaChannelErrorFlags = DMA0->CH[DMA0_CH2_DMA_CHANNEL].CH_ES;
        s_diagnostics.dmaGlobalErrorFlags = EDMA_GetErrorStatusFlags(DMA0_DMA_BASEADDR);
        s_diagnostics.dmaErrorCount++;
        AdcDmaFault(kAdcDmaErrorDma);
        return false;
    }
    if (((LPADC_GetStatusFlags(ADC0) & kLPADC_ResultFIFO0OverflowFlag) != 0U) ||
        ((LPADC_GetStatusFlags(ADC1) & kLPADC_ResultFIFO0OverflowFlag) != 0U))
    {
        s_diagnostics.fifoOverflowCount++;
        AdcDmaFault(kAdcDmaErrorFifoOverflow);
        return false;
    }
    return true;
}

bool AdcDmaReadRaw(uint16_t values[ADC_DMA_CHANNEL_COUNT])
{
    uint32_t index;

    if ((values == NULL) || (!AdcDmaPoll()) || (!AdcDmaUpdateLatest()))
    {
        return false;
    }
    for (index = 0U; index < ADC_DMA_CHANNEL_COUNT; index++)
    {
        values[index] = s_latestRaw[index];
    }
    return true;
}

bool AdcDmaReadVolts(float values[ADC_DMA_CHANNEL_COUNT], float referenceVolts)
{
    uint16_t raw[ADC_DMA_CHANNEL_COUNT];
    uint32_t index;

    if ((values == NULL) || (referenceVolts <= 0.0F) || (!AdcDmaReadRaw(raw)))
    {
        return false;
    }
    for (index = 0U; index < ADC_DMA_CHANNEL_COUNT; index++)
    {
        values[index] = ((float)raw[index] * referenceVolts) / 4096.0F;
    }
    return true;
}

bool AdcDmaReadLegacy12(float values[ADC_DMA_LEGACY_CHANNEL_COUNT], float referenceVolts)
{
    static const uint8_t legacyMap[ADC_DMA_LEGACY_CHANNEL_COUNT] =
        {0U, 1U, 2U, 3U, 4U, 5U, 7U, 6U, 8U, 9U, 10U, 11U};
    float allValues[ADC_DMA_CHANNEL_COUNT];
    uint32_t index;

    if ((values == NULL) || (!AdcDmaReadVolts(allValues, referenceVolts)))
    {
        return false;
    }
    for (index = 0U; index < ADC_DMA_LEGACY_CHANNEL_COUNT; index++)
    {
        values[index] = allValues[legacyMap[index]];
    }
    return true;
}

bool AdcDmaIsBusy(void)
{
    return s_running;
}

bool AdcDmaIsRunning(void)
{
    return s_running;
}

void AdcDmaGetDiagnostics(adc_dma_diagnostics_t *diagnostics)
{
    uint32_t irqMask;

    if (diagnostics != NULL)
    {
        irqMask = DisableGlobalIRQ();
        *diagnostics = s_diagnostics;
        diagnostics->adc0BlockCount = s_adc0CompletedBlocks;
        diagnostics->adc1BlockCount = s_adc1CompletedBlocks;
        diagnostics->completedBlockCount =
            (s_adc0CompletedBlocks < s_adc1CompletedBlocks) ? s_adc0CompletedBlocks : s_adc1CompletedBlocks;
        EnableGlobalIRQ(irqMask);
    }
}

void DMA_CH1_IRQHandler(void)
{
    EDMA_HandleIRQ(&DMA0_CH1_Handle);
}

void DMA_CH2_IRQHandler(void)
{
    EDMA_HandleIRQ(&DMA0_CH2_Handle);
}
