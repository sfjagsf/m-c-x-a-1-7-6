#include "GpioDmaFilter.h"

#include "fsl_ctimer.h"
#include "peripherals.h"

/* This symbol is referenced by the Config Tools generated DMA configuration. */
volatile uint32_t g_gpio3SampleBuf[GPIO_DMA_FILTER_SAMPLE_COUNT];

static volatile uint32_t s_gpio3FilteredPdir;
static volatile uint32_t s_dmaErrorCount;
static volatile uint32_t s_completedWindowCount;
static volatile bool s_running;

static bool GpioDmaFilterArm(void)
{
    status_t status;

    status = EDMA_SubmitTransfer(&DMA0_CH7_Handle, &DMA0_CH7_TRANSFER0_CONFIG);
    if (status != kStatus_Success)
    {
        s_dmaErrorCount++;
        return false;
    }

    EDMA_StartTransfer(&DMA0_CH7_Handle);
    return true;
}

static void GpioDmaFilterUpdate(void)
{
    uint32_t filtered;
    uint32_t pinMask;

    /* Bits outside the five inputs always retain the live GPIO3 state. */
    filtered = GPIO3->PDIR & ~GPIO_DMA_FILTER_MASK;

    for (pinMask = GPIO_DMA_FILTER_IN5_MASK; pinMask <= GPIO_DMA_FILTER_IN7_MASK; pinMask <<= 1U)
    {
        uint32_t highCount = 0U;
        uint32_t index;

        for (index = 0U; index < GPIO_DMA_FILTER_SAMPLE_COUNT; index++)
        {
            if ((g_gpio3SampleBuf[index] & pinMask) != 0U)
            {
                highCount++;
            }
        }

        /* Eight samples: five or more high samples means a filtered high. */
        if (highCount >= ((GPIO_DMA_FILTER_SAMPLE_COUNT / 2U) + 1U))
        {
            filtered |= pinMask;
        }
    }

    s_gpio3FilteredPdir = filtered;
}

void GpioDmaFilterDmaCallback(edma_handle_t *handle, void *userData, bool transferDone, uint32_t tcds)
{
    (void)handle;
    (void)userData;
    (void)tcds;

    if (!transferDone)
    {
        s_dmaErrorCount++;
        CTIMER_StopTimer(CTIMER4);
        s_running = false;
        return;
    }

    GpioDmaFilterUpdate();
    s_completedWindowCount++;
    if (!GpioDmaFilterArm())
    {
        /* Do not keep issuing timer DMA requests after a failed rearm. */
        CTIMER_StopTimer(CTIMER4);
        s_running = false;
    }
}

bool GpioDmaFilterStart(void)
{
    uint32_t index;

    CTIMER_StopTimer(CTIMER4);
    EDMA_AbortTransfer(&DMA0_CH7_Handle);
    CTIMER_Reset(CTIMER4);

    for (index = 0U; index < GPIO_DMA_FILTER_SAMPLE_COUNT; index++)
    {
        g_gpio3SampleBuf[index] = 0U;
    }

    /* Before the first 800-us DMA window completes, publish the live level. */
    s_gpio3FilteredPdir = GPIO3->PDIR;
    s_dmaErrorCount     = 0U;
    s_completedWindowCount = 0U;
    s_running = false;

    /* Config Tools assigned CH7's priority but did not enable this IRQ. */
    EnableIRQ(DMA0_DMA_CH_INT_DONE_7_IRQN);

    if (!GpioDmaFilterArm())
    {
        return false;
    }

    CTIMER_StartTimer(CTIMER4);
    s_running = true;
    return true;
}

uint32_t GpioDmaFilterGetPortState(void)
{
    return s_gpio3FilteredPdir;
}

bool GpioDmaFilterReadPin(uint32_t pinMask)
{
    if ((pinMask == 0U) || ((pinMask & ~GPIO_DMA_FILTER_MASK) != 0U))
    {
        return false;
    }

    return (GpioDmaFilterGetPortState() & pinMask) != 0U;
}

uint32_t GpioDmaFilterGetErrorCount(void)
{
    return s_dmaErrorCount;
}

uint32_t GpioDmaFilterGetCompletedWindowCount(void)
{
    return s_completedWindowCount;
}

bool GpioDmaFilterIsHealthy(void)
{
    return s_running && (s_dmaErrorCount == 0U);
}
