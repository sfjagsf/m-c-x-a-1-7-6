#include "MySpi.h"

#include <string.h>

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
    uint8_t txBuffer[MY_SPI0_MAX_TRANSACTION_BYTES];

    if ((firstData == NULL) || (firstSize == 0U) ||
        ((secondData == NULL) && (secondSize != 0U)) ||
        ((firstSize + secondSize) > sizeof(txBuffer)))
    {
        return kStatus_InvalidArgument;
    }

    (void)memcpy(txBuffer, firstData, firstSize);
    if (secondSize != 0U)
    {
        (void)memcpy(&txBuffer[firstSize], secondData, secondSize);
    }

    return MY_SPI0_Transfer(txBuffer, NULL, firstSize + secondSize);
}

status_t MY_SPI0_TransmitReceive(const uint8_t *txData,
                                 size_t txSize,
                                 uint8_t *rxData,
                                 size_t rxSize)
{
    uint8_t txBuffer[MY_SPI0_MAX_TRANSACTION_BYTES];
    uint8_t rxBuffer[MY_SPI0_MAX_TRANSACTION_BYTES];
    size_t totalSize = txSize + rxSize;
    status_t status;

    if ((txData == NULL) || (txSize == 0U) || (rxData == NULL) || (rxSize == 0U) ||
        (totalSize > sizeof(txBuffer)))
    {
        return kStatus_InvalidArgument;
    }

    (void)memcpy(txBuffer, txData, txSize);
    (void)memset(&txBuffer[txSize], 0xFF, rxSize);

    status = MY_SPI0_Transfer(txBuffer, rxBuffer, totalSize);
    if (status == kStatus_Success)
    {
        (void)memcpy(rxData, &rxBuffer[txSize], rxSize);
    }

    return status;
}
