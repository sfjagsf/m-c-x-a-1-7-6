#include "SpiTransport.h"

const SpiTransport_t g_spi0Transport = {
    .base = LPSPI0,
    .pcsConfigFlag = (uint32_t)kLPSPI_MasterPcs0,
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
    lpspi_transfer_t transfer;

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
