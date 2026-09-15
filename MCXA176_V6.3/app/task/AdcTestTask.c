#include "AdcTestTask.h"

#include <stdio.h>

#include "UartDriver.h"
#include "cmsis_os2.h"

#define ADC_TEST_TASK_PERIOD_MS (10U)
#define ADC_TEST_UART0_REPORT_PERIOD_MS (500U)
#define ADC_TEST_UART0_REPORT_TICKS (ADC_TEST_UART0_REPORT_PERIOD_MS / ADC_TEST_TASK_PERIOD_MS)
/* Change this to the measured VDDA/VREFH voltage before precision checks. */
#define ADC_TEST_REFERENCE_VOLTS (ADC_DMA_DEFAULT_REFERENCE_VOLTS)

volatile bool g_adcTestLastReadOk;
volatile uint16_t g_adcTestRaw[ADC_DMA_CHANNEL_COUNT];
volatile float g_adcTestVolts[ADC_DMA_CHANNEL_COUNT];
volatile adc_dma_diagnostics_t g_adcTestDiagnostics;
volatile uint32_t g_adcTestSequence;
volatile uint32_t g_adcTestUart0SentCount;
volatile uint32_t g_adcTestUart0SkippedCount;
volatile uint32_t g_adcTestUart1SentCount;
volatile uint32_t g_adcTestUart1SkippedCount;

/* Keep UART formatting buffers out of the RTOS task stack. */
static char s_adcTestUart0Frame[160];
static char s_adcTestUart1Frame[256];

void AdcTestTask(void *argument)
{
    uint16_t raw[ADC_DMA_CHANNEL_COUNT];
    adc_dma_diagnostics_t diagnostics;
    uint32_t index;
    uint32_t reportTicks = 0U;

    (void)argument;
    for (;;)
    {
        const bool readOk = AdcDmaReadRaw(raw);

        if (readOk)
        {
            for (index = 0U; index < ADC_DMA_CHANNEL_COUNT; index++)
            {
                g_adcTestRaw[index] = raw[index];
                g_adcTestVolts[index] = ((float)raw[index] * ADC_TEST_REFERENCE_VOLTS) / 4096.0F;
            }
            g_adcTestSequence++;
        }
        g_adcTestLastReadOk = readOk;
        AdcDmaGetDiagnostics(&diagnostics);
        g_adcTestDiagnostics = diagnostics;

        reportTicks++;
        if (reportTicks >= ADC_TEST_UART0_REPORT_TICKS)
        {
            const uint32_t adc13MilliVolts =
                ((uint32_t)g_adcTestRaw[4] * (uint32_t)(ADC_TEST_REFERENCE_VOLTS * 1000.0F)) / 4096U;
            int uart0Length;
            int uart1Length;

            uart0Length = snprintf(s_adcTestUart0Frame, sizeof(s_adcTestUart0Frame),
                                   "ADC1_3 raw=%u mv=%lu blocks=%lu/%lu err=%lu/%lu/%lu\r\n",
                                   (unsigned int)g_adcTestRaw[4], (unsigned long)adc13MilliVolts,
                                   (unsigned long)diagnostics.adc0BlockCount,
                                   (unsigned long)diagnostics.adc1BlockCount,
                                   (unsigned long)diagnostics.dmaErrorCount,
                                   (unsigned long)diagnostics.fifoOverflowCount,
                                   (unsigned long)diagnostics.resultTagErrorCount);
            uart1Length = snprintf(
                s_adcTestUart1Frame, sizeof(s_adcTestUart1Frame),
                "ADC ok=%u e=%u blk=%lu/%lu d/f/t=%lu/%lu/%lu es=%08lx/%08lx/%08lx tag=a%u i%u %u/%u w=%08lx raw:[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u] "
                "mv:[%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu]\r\n",
                readOk ? 1U : 0U, (unsigned int)diagnostics.lastError,
                (unsigned long)diagnostics.adc0BlockCount,
                (unsigned long)diagnostics.adc1BlockCount,
                (unsigned long)diagnostics.dmaErrorCount,
                (unsigned long)diagnostics.fifoOverflowCount,
                (unsigned long)diagnostics.resultTagErrorCount,
                (unsigned long)diagnostics.adc0DmaChannelErrorFlags,
                (unsigned long)diagnostics.adc1DmaChannelErrorFlags,
                (unsigned long)diagnostics.dmaGlobalErrorFlags,
                (unsigned int)diagnostics.resultTagAdcIndex,
                (unsigned int)diagnostics.resultTagSampleIndex,
                (unsigned int)diagnostics.resultTagExpectedCommand,
                (unsigned int)diagnostics.resultTagActualCommand,
                (unsigned long)diagnostics.resultTagLastWord,
                (unsigned int)g_adcTestRaw[0], (unsigned int)g_adcTestRaw[1], (unsigned int)g_adcTestRaw[2],
                (unsigned int)g_adcTestRaw[3], (unsigned int)g_adcTestRaw[4], (unsigned int)g_adcTestRaw[5],
                (unsigned int)g_adcTestRaw[6], (unsigned int)g_adcTestRaw[7], (unsigned int)g_adcTestRaw[8],
                (unsigned int)g_adcTestRaw[9], (unsigned int)g_adcTestRaw[10], (unsigned int)g_adcTestRaw[11],
                (unsigned int)g_adcTestRaw[12], (unsigned int)g_adcTestRaw[13],
                (unsigned long)(g_adcTestVolts[0] * 1000.0F),
                (unsigned long)(g_adcTestVolts[1] * 1000.0F),
                (unsigned long)(g_adcTestVolts[2] * 1000.0F),
                (unsigned long)(g_adcTestVolts[3] * 1000.0F),
                (unsigned long)(g_adcTestVolts[4] * 1000.0F),
                (unsigned long)(g_adcTestVolts[5] * 1000.0F),
                (unsigned long)(g_adcTestVolts[6] * 1000.0F),
                (unsigned long)(g_adcTestVolts[7] * 1000.0F),
                (unsigned long)(g_adcTestVolts[8] * 1000.0F),
                (unsigned long)(g_adcTestVolts[9] * 1000.0F),
                (unsigned long)(g_adcTestVolts[10] * 1000.0F),
                (unsigned long)(g_adcTestVolts[11] * 1000.0F),
                (unsigned long)(g_adcTestVolts[12] * 1000.0F),
                (unsigned long)(g_adcTestVolts[13] * 1000.0F));

            reportTicks = 0U;
            if ((uart0Length > 0) && ((size_t)uart0Length < sizeof(s_adcTestUart0Frame)) &&
                (Uart0_Send((const uint8_t *)s_adcTestUart0Frame, (size_t)uart0Length) == kStatus_Success))
            {
                g_adcTestUart0SentCount++;
            }
            else
            {
                g_adcTestUart0SkippedCount++;
            }
            if ((uart1Length > 0) && ((size_t)uart1Length < sizeof(s_adcTestUart1Frame)) &&
                (Uart1_Send((const uint8_t *)s_adcTestUart1Frame, (size_t)uart1Length) == kStatus_Success))
            {
                g_adcTestUart1SentCount++;
            }
            else
            {
                g_adcTestUart1SkippedCount++;
            }
        }
        (void)osDelay(ADC_TEST_TASK_PERIOD_MS);
    }
}
