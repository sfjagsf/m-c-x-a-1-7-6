/* Temporary on-target ADC validation task. */
#ifndef APP_ADC_TEST_TASK_H_
#define APP_ADC_TEST_TASK_H_

#include <stdbool.h>
#include <stdint.h>

#include "AdcDma.h"

extern volatile bool g_adcTestLastReadOk;
extern volatile uint16_t g_adcTestRaw[ADC_DMA_CHANNEL_COUNT];
extern volatile float g_adcTestVolts[ADC_DMA_CHANNEL_COUNT];
extern volatile adc_dma_diagnostics_t g_adcTestDiagnostics;
extern volatile uint32_t g_adcTestSequence;
extern volatile uint32_t g_adcTestUart0SentCount;
extern volatile uint32_t g_adcTestUart0SkippedCount;
extern volatile uint32_t g_adcTestUart1SentCount;
extern volatile uint32_t g_adcTestUart1SkippedCount;

void AdcTestTask(void *argument);
bool AdcTestTask_Create(void);

#endif /* APP_ADC_TEST_TASK_H_ */
