#include "UartEchoTask.h"

#include <stdio.h>
#include <string.h>

#include "AdcDma.h"
#include "GpioDmaFilter.h"
#include "GpioInputTask.h"
#include "MySpi.h"
#include "UartDriver.h"
#include "W25qxx.h"
#include "cmsis_os2.h"

#define UART_ECHO_TASK_PERIOD_MS (1U)

/*
 * UART1 temporary board-bring-up tests.  The three function calls in
 * Uart1EchoTask() below may be individually commented out after validation.
 */
#define UART1_BOARD_TEST_ENABLE             (1U)
#define UART1_BOARD_TEST_PERIOD_MS          (1000U)
#define UART1_BOARD_TEST_ADC_PHASE_MS       (333U)
#define UART1_BOARD_TEST_FLASH_PHASE_MS     (666U)
#define UART1_FLASH_TEST_ADDRESS             (0x00000000UL)

/*
 * WARNING: the Flash test erases the complete 4 KiB sector containing this
 * address, programs this payload, then reads and compares it.  Change the
 * address to a reserved unused sector before enabling it on a product unit.
 */
static const uint8_t s_uart1FlashTestWriteData[] = {
    0xA5U, 0x5AU, 'M', 'C', 'X', 'A', '1', '7', '6', '-', 'S', 'P', 'I', '-', 'O', 'K'};

static bool Uart1TestPeriodElapsed(uint32_t *ticks)
{
    (*ticks)++;
    if (*ticks < UART1_BOARD_TEST_PERIOD_MS)
    {
        return false;
    }

    *ticks = 0U;
    return true;
}

static bool Uart1TestSend(const char *message, size_t length)
{
    if ((message != NULL) && (length != 0U) && (!Uart1_IsBusy()))
    {
        return Uart1_Send((const uint8_t *)message, length) == kStatus_Success;
    }

    return false;
}

static void Uart1TestReportGpio(void)
{
    static uint32_t ticks;
    char message[160];
    int length;

    if (!Uart1TestPeriodElapsed(&ticks))
    {
        return;
    }

    length = snprintf(message, sizeof(message),
                      "GPIO IN1=%u IN5=%u IN6=%u IN7=%u IN10=%u pdir=%08lX seq=%lu dma=%s err=%lu\\r\\n",
                      GpioInputGetStableState(kGpioInputIn1) ? 1U : 0U,
                      GpioInputGetStableState(kGpioInputIn5) ? 1U : 0U,
                      GpioInputGetStableState(kGpioInputIn6) ? 1U : 0U,
                      GpioInputGetStableState(kGpioInputIn7) ? 1U : 0U,
                      GpioInputGetStableState(kGpioInputIn10) ? 1U : 0U,
                      (unsigned long)GpioDmaFilterGetPortState(),
                      (unsigned long)GpioInputGetChangeSequence(),
                      GpioDmaFilterIsHealthy() ? "ok" : "fault",
                      (unsigned long)GpioDmaFilterGetErrorCount());
    if ((length > 0) && ((size_t)length < sizeof(message)))
    {
        Uart1TestSend(message, (size_t)length);
    }
}

static void Uart1TestReportAdc(void)
{
    static uint32_t ticks = UART1_BOARD_TEST_ADC_PHASE_MS;
    uint16_t raw[ADC_DMA_CHANNEL_COUNT];
    char message[UART_BUFFER_SIZE];
    size_t used = 0U;
    uint32_t channel;

    if (!Uart1TestPeriodElapsed(&ticks))
    {
        return;
    }

    if (!AdcDmaReadRaw(raw))
    {
        const int length = snprintf(message, sizeof(message), "ADC no-data/fault\\r\\n");
        if ((length > 0) && ((size_t)length < sizeof(message)))
        {
            Uart1TestSend(message, (size_t)length);
        }
        return;
    }

    used = (size_t)snprintf(message, sizeof(message), "ADC raw:[");
    for (channel = 0U; (channel < ADC_DMA_CHANNEL_COUNT) && (used < sizeof(message)); channel++)
    {
        const int length = snprintf(&message[used], sizeof(message) - used, "%u%s", raw[channel],
                                    (channel + 1U == ADC_DMA_CHANNEL_COUNT) ? "] mv:[" : ",");
        if ((length < 0) || ((size_t)length >= (sizeof(message) - used)))
        {
            return;
        }
        used += (size_t)length;
    }
    for (channel = 0U; (channel < ADC_DMA_CHANNEL_COUNT) && (used < sizeof(message)); channel++)
    {
        const uint32_t millivolts = ((uint32_t)raw[channel] * 3275UL) / 4095UL;
        const int length = snprintf(&message[used], sizeof(message) - used, "%lu%s", (unsigned long)millivolts,
                                    (channel + 1U == ADC_DMA_CHANNEL_COUNT) ? "]\\r\\n" : ",");
        if ((length < 0) || ((size_t)length >= (sizeof(message) - used)))
        {
            return;
        }
        used += (size_t)length;
    }
    Uart1TestSend(message, used);
}

static void Uart1TestFlash(void)
{
    static uint32_t ticks = UART1_BOARD_TEST_FLASH_PHASE_MS;
    static bool completed;
    static bool reported;
    static bool passed;
    static uint8_t jedecId[3];
    static uint8_t statusRegister1;
    static status_t jedecTransferStatus;
    static status_t statusTransferStatus;
    static bool initPassed;
    char message[160];
    int length;

    if (!completed)
    {
        static const uint8_t jedecCommand = 0x9FU;
        static const uint8_t statusCommand = 0x05U;
        uint8_t readData[sizeof(s_uart1FlashTestWriteData)] = {0U};

        if (!Uart1TestPeriodElapsed(&ticks))
        {
            return;
        }

        /*
         * Read raw commands before W25qxxInit.  This distinguishes a failed
         * LPSPI transaction from an electrically silent MISO line (00/FF).
         */
        jedecTransferStatus = MY_SPI0_TransmitReceive(&jedecCommand, 1U, jedecId, sizeof(jedecId));
        statusTransferStatus = MY_SPI0_TransmitReceive(&statusCommand, 1U, &statusRegister1, 1U);
        initPassed = W25qxxInit(FLASH_SPI_MODE);

        passed = (jedecTransferStatus == kStatus_Success) &&
                 (jedecId[0] != 0x00U) && (jedecId[0] != 0xFFU) && initPassed &&
                 W25qxxEraseSector(UART1_FLASH_TEST_ADDRESS) &&
                 W25qxxPageProgram(UART1_FLASH_TEST_ADDRESS, s_uart1FlashTestWriteData,
                                    sizeof(s_uart1FlashTestWriteData)) &&
                 W25qxxRead(UART1_FLASH_TEST_ADDRESS, readData, sizeof(readData)) &&
                 (memcmp(readData, s_uart1FlashTestWriteData, sizeof(readData)) == 0);
        completed = true;
    }

    if (reported)
    {
        return;
    }

    length = snprintf(message, sizeof(message),
                      "FLASH %s addr=%06lX xfer=%ld/%ld jedec=%02X%02X%02X sr1=%02X init=%u\\r\\n",
                      passed ? "PASS" : "FAIL", (unsigned long)UART1_FLASH_TEST_ADDRESS,
                      (long)jedecTransferStatus, (long)statusTransferStatus, jedecId[0], jedecId[1], jedecId[2],
                      statusRegister1, initPassed ? 1U : 0U);
    if ((length > 0) && ((size_t)length < sizeof(message)))
    {
        reported = Uart1TestSend(message, (size_t)length);
    }
}

/*
 * Enable echo after validating the RS485 direction GPIO. P0_19 is asserted
 * only for the duration of each reply, then the driver returns to RX mode.
 */
#define UART0_ECHO_TEST_ENABLE (1U)

static const uint8_t s_uart0TxTestFrame[] = "RS485 UART0 TX TEST\r\n";

status_t UartEchoTest_SendOnce(void)
{
    return Uart0_Send(s_uart0TxTestFrame, sizeof(s_uart0TxTestFrame) - 1U);
}

void UartEchoTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        const uint8_t *frame;
        size_t length;

        frame = Uart0_GetFrame(&length);
		if ((frame != NULL) && (length != 0U))
        {
#if (UART0_ECHO_TEST_ENABLE != 0U)
            status_t status;

            /* Reply copies RX into UART0's private TX buffer before restarting DMA. */
            status = Uart0_Reply(frame, length);
            if (status != kStatus_Success)
            {
                /* Preserve the failure for the debugger instead of silently dropping it. */
                UartEcho_OnReplyFailed(status);
            }
#else
            /* Receive-only diagnostic mode: do not raise RS485_EN for TX. */
            (void)Uart0_ReleaseFrame();
#endif
        }

        (void)osDelay(UART_ECHO_TASK_PERIOD_MS);
    }
}

void Uart1EchoTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        const uint8_t *frame;
        size_t length;

        frame = Uart1_GetFrame(&length);
        if ((frame != NULL) && (length != 0U))
        {
            const status_t status = Uart1_Reply(frame, length);
            if (status != kStatus_Success)
            {
                Uart1Echo_OnReplyFailed(status);
            }
        }

#if (UART1_BOARD_TEST_ENABLE != 0U)
        /* Comment out any individual line after that hardware module is verified. */
        Uart1TestReportGpio();
        Uart1TestReportAdc();
        Uart1TestFlash();
#endif

        (void)osDelay(UART_ECHO_TASK_PERIOD_MS);
    }
}

__WEAK void UartEcho_OnReplyFailed(status_t status)
{
    (void)status;
}

__WEAK void Uart1Echo_OnReplyFailed(status_t status)
{
    (void)status;
}
