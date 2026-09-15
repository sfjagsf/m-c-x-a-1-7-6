#include "Spi1Digital.h"

#include "Spi1PointDma.h"
#include "SpiTransport.h"

status_t Spi1DigitalSendFrame(const uint8_t *data, size_t dataSize)
{
    status_t status;

    status = Spi1OutputAcquireDirectFrame();
    if (status != kStatus_Success)
    {
        return status;
    }

    /* PCS0 is SYNC on this board; a complete transfer is one SYNC-framed packet. */
    status = SpiTransportTransmit(&g_spi1Transport, data, dataSize);
    Spi1OutputReleaseDirectFrame(status);
    return status;
}
