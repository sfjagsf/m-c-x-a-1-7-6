/*
 * Synchronous ADC0/ADC1 continuous sampler.
 *
 * CTIMER3_MAT1 is routed internally to both ADC Trigger0 inputs. Each timer
 * edge starts two seven-command scans; DMA CH1 and CH2 use two-block rings.
 */
#ifndef APP_ADC_DMA_H_
#define APP_ADC_DMA_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_DMA_ADC0_CHANNEL_COUNT        (7U)
#define ADC_DMA_ADC1_CHANNEL_COUNT        (7U)
#define ADC_DMA_CHANNEL_COUNT             (14U)
#define ADC_DMA_SAMPLES_PER_CHANNEL       (10U)
#define ADC_DMA_BUFFER_COUNT              (2U)
#define ADC_DMA_BLOCK_WORD_COUNT          (ADC_DMA_ADC0_CHANNEL_COUNT * ADC_DMA_SAMPLES_PER_CHANNEL)
#define ADC_DMA_SAMPLE_RATE_HZ            (1000U)
#define ADC_DMA_LEGACY_CHANNEL_COUNT      (12U)
#define ADC_DMA_DEFAULT_REFERENCE_VOLTS   (3.275F)

typedef enum
{
    kAdcDmaErrorNone = 0,
    kAdcDmaErrorNotInitialized,
    kAdcDmaErrorNotRunning,
    kAdcDmaErrorNoData,
    kAdcDmaErrorDmaSubmit,
    kAdcDmaErrorDma,
    kAdcDmaErrorFifoOverflow,
    kAdcDmaErrorResultTag,
    kAdcDmaErrorTimer,
    kAdcDmaErrorBufferOverrun,
} adc_dma_error_t;

typedef struct
{
    uint32_t startCount;
    uint32_t stopCount;
    uint32_t acquisitionCount;
    uint32_t adc0BlockCount;
    uint32_t adc1BlockCount;
    uint32_t completedBlockCount;
    uint32_t droppedBlockCount;
    uint32_t dmaErrorCount;
    uint32_t adc0DmaChannelErrorFlags;
    uint32_t adc1DmaChannelErrorFlags;
    uint32_t dmaGlobalErrorFlags;
    uint32_t fifoOverflowCount;
    uint32_t resultTagErrorCount;
    uint32_t resultTagLastWord;
    uint8_t resultTagExpectedCommand;
    uint8_t resultTagActualCommand;
    uint8_t resultTagAdcIndex;
    uint8_t resultTagSampleIndex;
    adc_dma_error_t lastError;
} adc_dma_diagnostics_t;

/* Call once after BOARD_InitBootPeripherals(). Does not start sampling. */
void AdcDma_Init(void);

/* Arms both DMA rings, then starts CTIMER3's internal 1 kHz MAT1 waveform. */
bool AdcDmaStartContinuous(void);

/* Stops the timer first, then disables and aborts both DMA channels. */
void AdcDmaStopContinuous(void);

/* Checks DMA and ADC FIFO status. False means sampling stopped on a fault. */
bool AdcDmaPoll(void);

/*
 * Returns the newest complete ten-scan average without stopping acquisition.
 * Output order: AD1, AD2, AD3, AD4, ADC1_3, AD6, AD8, AD7, AD9..AD14.
 * Returns false until the first 10 ms block has completed or after a fault.
 */
bool AdcDmaReadRaw(uint16_t values[ADC_DMA_CHANNEL_COUNT]);

bool AdcDmaReadVolts(float values[ADC_DMA_CHANNEL_COUNT], float referenceVolts);

/* STM32 logical order: AD1..AD7, AD8, AD9..AD12. */
bool AdcDmaReadLegacy12(float values[ADC_DMA_LEGACY_CHANNEL_COUNT], float referenceVolts);

/* Kept for existing callers: true while the continuous acquisition engine runs. */
bool AdcDmaIsBusy(void);
bool AdcDmaIsRunning(void);
void AdcDmaGetDiagnostics(adc_dma_diagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif /* APP_ADC_DMA_H_ */
