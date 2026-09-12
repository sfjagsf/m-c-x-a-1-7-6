#include "Spi1Digital.h"

#include "SpiTransport.h"

status_t Spi1DigitalSendFrame(const uint8_t *data, size_t dataSize)
{
    /* PCS0 is SYNC on this board; a complete transfer is one SYNC-framed packet. */
    return SpiTransportTransmit(&g_spi1Transport, data, dataSize);
}
