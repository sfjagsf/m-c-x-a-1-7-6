#include "UartEchoTask.h"

#include <stdio.h>
#include <string.h>

#include "AdcDma.h"
#include "GpioDmaFilter.h"
#include "GpioInputTask.h"
#include "I2cEepromDriver.h"
#include "MySpi.h"
#include "Pcf8563RtcDriver.h"
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
#define UART1_FLASH_TEST_PAGE_SIZE           (256UL)
#define UART1_FLASH_TEST_PAGE_COUNT          (NOR_FLASH_SECTOR_SIZE / UART1_FLASH_TEST_PAGE_SIZE)
#define UART1_FLASH_TEST_WRITE_PERIOD_MS     (200U)
#define UART1_FLASH_TEST_READ_PERIOD_MS      (400U)
#define UART1_FLASH_TEST_PAYLOAD_SIZE        (32U)
#define UART1_I2C_DEVICE_TEST_ENABLE          (1U)
#define UART1_I2C_DEVICE_TEST_PERIOD_MS       (2000U)
#define UART1_EEPROM_TEST_ADDRESS             (0x0000U)
#define UART1_EEPROM_TEST_READ_SIZE           (16U)
#define UART1_RTC_SET_INVALID_TIME_ENABLE     (1U)
#define UART1_RTC_INITIAL_YEAR                 (2026U)
#define UART1_RTC_INITIAL_MONTH                (9U)
#define UART1_RTC_INITIAL_DAY                  (16U)
#define UART1_RTC_INITIAL_WEEKDAY              (3U) /* Wednesday; Sunday is 0. */
#define UART1_RTC_INITIAL_HOUR                 (14U)
#define UART1_RTC_INITIAL_MINUTE               (13U)
#define UART1_RTC_INITIAL_SECOND               (41U)

/* Bring-up fallback only; disable UART1_RTC_SET_INVALID_TIME_ENABLE for production. */
static const pcf8563_datetime_t s_uart1RtcFallbackTime = {
    .year = UART1_RTC_INITIAL_YEAR,
    .month = UART1_RTC_INITIAL_MONTH,
    .day = UART1_RTC_INITIAL_DAY,
    .weekday = UART1_RTC_INITIAL_WEEKDAY,
    .hour = UART1_RTC_INITIAL_HOUR,
    .minute = UART1_RTC_INITIAL_MINUTE,
    .second = UART1_RTC_INITIAL_SECOND,
    .clockValid = true,
};

/*
 * WARNING: the Flash test erases the complete 4 KiB sector containing this
 * address. It then writes its 16 pages at 200 ms intervals and reads/verifies
 * every page at 400 ms intervals. Change the address to a reserved unused
 * sector before enabling it on a product unit.
 */

static bool Uart1TestIntervalElapsed(uint32_t *ticks, uint32_t intervalMs)
{
    (*ticks)++;
    if (*ticks < intervalMs)
    {
        return false;
    }

    *ticks = 0U;
    return true;
}

static void Uart1TestBuildFlashPayload(uint8_t page, uint8_t data[UART1_FLASH_TEST_PAYLOAD_SIZE])
{
    uint32_t index;

    data[0] = 0xA5U;
    data[1] = 0x5AU;
    data[2] = 'M';
    data[3] = 'C';
    data[4] = 'X';
    data[5] = 'A';
    data[6] = 'F';
    data[7] = 'L';
    data[8] = page;
    data[9] = (uint8_t)~page;
    for (index = 10U; index < UART1_FLASH_TEST_PAYLOAD_SIZE; index++)
    {
        data[index] = (uint8_t)(0x3DU + page + index);
    }
}

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
    static uint32_t startupTicks = UART1_BOARD_TEST_FLASH_PHASE_MS;
    static uint32_t writeTicks;
    static uint32_t readTicks;
    static bool initialized;
    static bool eraseStarted;
    static bool erased;
    static bool failed;
    static bool failureReported;
    static const char *failureReason;
    static bool finalReported;
    static uint8_t writtenPages;
    static uint8_t verifiedPages;
    static uint8_t jedecId[3];
    static uint8_t statusRegister1;
    static status_t jedecTransferStatus;
    static status_t statusTransferStatus;
    uint8_t writeData[UART1_FLASH_TEST_PAYLOAD_SIZE];
    uint8_t readData[UART1_FLASH_TEST_PAYLOAD_SIZE];
    char message[160];
    int length;

    if (!initialized)
    {
        static const uint8_t jedecCommand = 0x9FU;
        static const uint8_t statusCommand = 0x05U;
        if (!Uart1TestPeriodElapsed(&startupTicks))
        {
            return;
        }

        /*
         * Read raw commands before W25qxxInit.  This distinguishes a failed
         * LPSPI transaction from an electrically silent MISO line (00/FF).
         */
        jedecTransferStatus = MY_SPI0_TransmitReceive(&jedecCommand, 1U, jedecId, sizeof(jedecId));
        statusTransferStatus = MY_SPI0_TransmitReceive(&statusCommand, 1U, &statusRegister1, 1U);
        initialized = true;
        failed = (jedecTransferStatus != kStatus_Success) || (statusTransferStatus != kStatus_Success) ||
                 (jedecId[0] == 0x00U) || (jedecId[0] == 0xFFU) || !W25qxxInit(FLASH_SPI_MODE);
        failureReason = "init";
        if (!failed)
        {
            failed = !W25qxxStartEraseSector(UART1_FLASH_TEST_ADDRESS);
            failureReason = "erase-start";
            eraseStarted = !failed;
        }

        length = snprintf(message, sizeof(message),
                          "FLASH %s start addr=%06lX jedec=%02X%02X%02X sr1=%02X\\r\\n",
                          failed ? "FAIL" : "ERASE", (unsigned long)UART1_FLASH_TEST_ADDRESS,
                          jedecId[0], jedecId[1], jedecId[2], statusRegister1);
        if ((length > 0) && ((size_t)length < sizeof(message)))
        {
            (void)Uart1TestSend(message, (size_t)length);
        }
        return;
    }

    if (failed)
    {
        if (!failureReported)
        {
            length = snprintf(message, sizeof(message), "FLASH RW FAIL stage=%s wr=%lu rd=%lu\\r\\n",
                              (failureReason != NULL) ? failureReason : "unknown", (unsigned long)writtenPages,
                              (unsigned long)verifiedPages);
            if ((length > 0) && ((size_t)length < sizeof(message)))
            {
                failureReported = Uart1TestSend(message, (size_t)length);
            }
        }
        return;
    }

    if (!erased)
    {
        bool busy;

        if (!eraseStarted || !W25qxxGetBusy(&busy))
        {
            failed = true;
            failureReason = "erase-poll";
            return;
        }
        if (!busy)
        {
            erased = true;
            length = snprintf(message, sizeof(message), "FLASH ERASE PASS addr=%06lX\\r\\n",
                              (unsigned long)UART1_FLASH_TEST_ADDRESS);
            if ((length > 0) && ((size_t)length < sizeof(message)))
            {
                (void)Uart1TestSend(message, (size_t)length);
            }
        }
        return;
    }

    if ((writtenPages < UART1_FLASH_TEST_PAGE_COUNT) &&
        Uart1TestIntervalElapsed(&writeTicks, UART1_FLASH_TEST_WRITE_PERIOD_MS))
    {
        Uart1TestBuildFlashPayload(writtenPages, writeData);
        if (!W25qxxPageProgram(UART1_FLASH_TEST_ADDRESS + ((uint32_t)writtenPages * UART1_FLASH_TEST_PAGE_SIZE),
                               writeData, sizeof(writeData)))
        {
            failed = true;
            failureReason = "page-program";
            return;
        }
        writtenPages++;
    }

    if ((verifiedPages < writtenPages) && Uart1TestIntervalElapsed(&readTicks, UART1_FLASH_TEST_READ_PERIOD_MS))
    {
        Uart1TestBuildFlashPayload(verifiedPages, writeData);
        if (!W25qxxRead(UART1_FLASH_TEST_ADDRESS + ((uint32_t)verifiedPages * UART1_FLASH_TEST_PAGE_SIZE),
                         readData, sizeof(readData)) || (memcmp(writeData, readData, sizeof(writeData)) != 0))
        {
            failed = true;
            failureReason = "read-compare";
            return;
        }

        length = snprintf(message, sizeof(message), "FLASH RW PASS wr=%lu/%lu rd=%lu addr=%06lX\\r\\n",
                          (unsigned long)writtenPages, (unsigned long)UART1_FLASH_TEST_PAGE_COUNT,
                          (unsigned long)verifiedPages + 1UL,
                          (unsigned long)(UART1_FLASH_TEST_ADDRESS +
                                          ((uint32_t)verifiedPages * UART1_FLASH_TEST_PAGE_SIZE)));
        verifiedPages++;
        if ((length > 0) && ((size_t)length < sizeof(message)))
        {
            (void)Uart1TestSend(message, (size_t)length);
        }
    }

    if ((writtenPages == UART1_FLASH_TEST_PAGE_COUNT) && (verifiedPages == UART1_FLASH_TEST_PAGE_COUNT) &&
        !finalReported)
    {
        length = snprintf(message, sizeof(message), "FLASH RW TEST PASS pages=%lu write=200ms read=400ms\\r\\n",
                          (unsigned long)UART1_FLASH_TEST_PAGE_COUNT);
        if ((length > 0) && ((size_t)length < sizeof(message)))
        {
            finalReported = Uart1TestSend(message, (size_t)length);
        }
    }
}

/* Read-only test for the independent RTC (I2C1) and EEPROM (I2C3) devices. */
static void Uart1TestI2cDevices(void)
{
    static uint32_t ticks;
    static bool reportPending;
    static bool rtcSetAttempted;
    uint8_t eepromData[UART1_EEPROM_TEST_READ_SIZE] = {0U};
    uint8_t rtcRegisters[9U] = {0U};
    pcf8563_datetime_t rtc = {0};
    status_t rtcStatus;
    status_t rtcRawStatus = kStatus_Success;
    status_t rtcSetStatus = kStatus_Success;
    const char *rtcSetResult = "SKIP";
    status_t eepromStatus;
    char message[192];
    int length;

    if (!reportPending && Uart1TestIntervalElapsed(&ticks, UART1_I2C_DEVICE_TEST_PERIOD_MS))
    {
        reportPending = true;
    }
    if (!reportPending || Uart1_IsBusy())
    {
        return;
    }

    rtcStatus = Pcf8563_ReadDateTime(&rtc);
#if (UART1_RTC_SET_INVALID_TIME_ENABLE != 0U)
    if ((rtcStatus == kStatus_Fail) && !rtcSetAttempted)
    {
        rtcSetAttempted = true;
        rtcSetStatus = Pcf8563_SetDateTime(&s_uart1RtcFallbackTime);
        rtcSetResult = (rtcSetStatus == kStatus_Success) ? "PASS" : "FAIL";
        if (rtcSetStatus == kStatus_Success)
        {
            rtcStatus = Pcf8563_ReadDateTime(&rtc);
        }
    }
#endif
    if (rtcStatus != kStatus_Success)
    {
        rtcRawStatus = Pcf8563_ReadRegisters(0x00U, rtcRegisters, sizeof(rtcRegisters));
    }
    eepromStatus = I2cEeprom_Read(UART1_EEPROM_TEST_ADDRESS, eepromData, sizeof(eepromData));

    if (rtcStatus == kStatus_Success)
    {
        length = snprintf(message, sizeof(message),
                          "RTC i2c1=PASS st=0 set=%s setst=%ld time=%04u-%02u-%02u %02u:%02u:%02u valid=%u "
                          "EEPROM i2c3=%s st=%ld addr=%04X data=%02X%02X%02X%02X\r\n",
                          rtcSetResult, (long)rtcSetStatus,
                          rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second,
                          rtc.clockValid ? 1U : 0U,
                          (eepromStatus == kStatus_Success) ? "PASS" : "FAIL", (long)eepromStatus,
                          UART1_EEPROM_TEST_ADDRESS, eepromData[0], eepromData[1], eepromData[2], eepromData[3]);
    }
    else
    {
        length = snprintf(message, sizeof(message),
                          "RTC i2c1=FAIL st=%ld set=%s setst=%ld rawst=%ld reg00-08=%02X%02X%02X%02X%02X%02X%02X%02X%02X "
                          "EEPROM i2c3=%s st=%ld addr=%04X data=%02X%02X%02X%02X\r\n",
                          (long)rtcStatus, rtcSetResult, (long)rtcSetStatus, (long)rtcRawStatus,
                          rtcRegisters[0], rtcRegisters[1], rtcRegisters[2], rtcRegisters[3], rtcRegisters[4],
                          rtcRegisters[5], rtcRegisters[6], rtcRegisters[7], rtcRegisters[8],
                          (eepromStatus == kStatus_Success) ? "PASS" : "FAIL", (long)eepromStatus,
                          UART1_EEPROM_TEST_ADDRESS, eepromData[0], eepromData[1], eepromData[2], eepromData[3]);
    }
    if ((length > 0) && ((size_t)length < sizeof(message)))
    {
        reportPending = !Uart1TestSend(message, (size_t)length);
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
#if (UART1_I2C_DEVICE_TEST_ENABLE != 0U)
        Uart1TestI2cDevices();
#endif
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
