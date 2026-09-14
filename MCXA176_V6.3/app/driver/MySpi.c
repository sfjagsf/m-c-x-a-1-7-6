#include "MySpi.h"

#include <string.h>

#include "fsl_common.h"

/*
 * These buffers must not be automatic variables.  A read transaction needs
 * both buffers (522 bytes in total), which exceeds the 512-byte application
 * task stacks.  The lock makes a concurrent caller fail cleanly instead of
 * corrupting an in-progress SPI transaction.
 */
static uint8_t s_txBuffer[MY_SPI0_MAX_TRANSACTION_BYTES];
static uint8_t s_rxBuffer[MY_SPI0_MAX_TRANSACTION_BYTES];
static volatile bool s_spi0Busy;

static bool MY_SPI0_TryAcquire(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();
    const bool acquired = !s_spi0Busy;

    if (acquired)
    {
        s_spi0Busy = true;
    }
    EnableGlobalIRQ(irqMask);
    return acquired;
}

static void MY_SPI0_Release(void)
{
    const uint32_t irqMask = DisableGlobalIRQ();

    s_spi0Busy = false;
    EnableGlobalIRQ(irqMask);
}

/*
 * LPSPI transfers are full duplex.  The two APIs below pack the two logical
 * segments into one transfer, so hardware PCS0 stays low throughout it.
 */
static status_t MY_SPI0_Transfer(const uint8_t *txData, uint8_t *rxData, size_t dataSize)
{
    if ((txData == NULL) || (dataSize == 0U) || (dataSize > MY_SPI0_MAX_TRANSACTION_BYTES))
    {
        return kStatus_InvalidArgument;
    }

    return SpiTransportTransfer(&g_spi0Transport, txData, rxData, dataSize);
}

status_t MY_SPI0_Transmit(const uint8_t *firstData,
                          size_t firstSize,
                          const uint8_t *secondData,
                          size_t secondSize)
{
    status_t status;

    if ((firstData == NULL) || (firstSize == 0U) ||
        ((secondData == NULL) && (secondSize != 0U)) ||
        (firstSize > sizeof(s_txBuffer)) ||
        (secondSize > (sizeof(s_txBuffer) - firstSize)))
    {
        return kStatus_InvalidArgument;
    }

    if (!MY_SPI0_TryAcquire())
    {
        return kStatus_Busy;
    }

    (void)memcpy(s_txBuffer, firstData, firstSize);
    if (secondSize != 0U)
    {
        (void)memcpy(&s_txBuffer[firstSize], secondData, secondSize);
    }

    status = MY_SPI0_Transfer(s_txBuffer, NULL, firstSize + secondSize);
    MY_SPI0_Release();
    return status;
}

status_t MY_SPI0_TransmitReceive(const uint8_t *txData,
                                 size_t txSize,
                                 uint8_t *rxData,
                                 size_t rxSize)
{
    size_t totalSize;
    status_t status;

    if ((txData == NULL) || (txSize == 0U) || (rxData == NULL) || (rxSize == 0U) ||
        (txSize > sizeof(s_txBuffer)) ||
        (rxSize > (sizeof(s_txBuffer) - txSize)))
    {
        return kStatus_InvalidArgument;
    }

    if (!MY_SPI0_TryAcquire())
    {
        return kStatus_Busy;
    }

    totalSize = txSize + rxSize;
    (void)memcpy(s_txBuffer, txData, txSize);
    (void)memset(&s_txBuffer[txSize], 0xFF, rxSize);

    status = MY_SPI0_Transfer(s_txBuffer, s_rxBuffer, totalSize);
    if (status == kStatus_Success)
    {
        (void)memcpy(rxData, &s_rxBuffer[txSize], rxSize);
    }

    MY_SPI0_Release();
    return status;
}
