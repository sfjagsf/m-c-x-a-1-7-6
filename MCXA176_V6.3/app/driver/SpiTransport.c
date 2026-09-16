#include "SpiTransport.h"

const SpiTransport_t g_spi0Transport = {
    .base = LPSPI0,
    /* W25Q command + address/dummy bytes require PCS0 low for the complete
     * multi-byte transaction (for example: 0x9F FF FF FF). */
    .pcsConfigFlag = (uint32_t)kLPSPI_MasterPcs0 | (uint32_t)kLPSPI_MasterPcsContinuous,
};

const SpiTransport_t g_spi1Transport = {
    .base = LPSPI1,
    .pcsConfigFlag = (uint32_t)kLPSPI_MasterPcs0,
};

status_t SpiTransportTransfer(const SpiTransport_t *bus,
                              const uint8_t *txData,
                              uint8_t *rxData,
                              size_t dataSize)
{
    /* Keep this initialization explicit if the SDK adds fields in a future update. */
    lpspi_transfer_t transfer = {0};

    if ((bus == NULL) || (bus->base == NULL) || (txData == NULL) || (dataSize == 0U))
    {
        return kStatus_InvalidArgument;
    }

    transfer.txData = txData;
    transfer.rxData = rxData;
    transfer.dataSize = dataSize;
    transfer.configFlags = bus->pcsConfigFlag;

    return LPSPI_MasterTransferBlocking(bus->base, &transfer);
}

status_t SpiTransportTransmit(const SpiTransport_t *bus,
                              const uint8_t *data,
                              size_t dataSize)
{
    return SpiTransportTransfer(bus, data, NULL, dataSize);
}
