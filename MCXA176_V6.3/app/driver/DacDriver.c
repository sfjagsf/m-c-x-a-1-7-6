#include "DacDriver.h"

#include "fsl_dac.h"
#include "peripherals.h"

bool DacDriverWriteCode(uint16_t code)
{
    if (code > DAC_DRIVER_CODE_MAX)
    {
        return false;
    }

    /* DAC0 is configured for software-triggered, FIFO-disabled operation. */
    DAC_SetData(DAC0_PERIPHERAL, code);
    DAC_DoSoftwareTriggerFIFO(DAC0_PERIPHERAL);
    return true;
}

uint16_t DacDriverReadCode(void)
{
    return (uint16_t)(DAC0_PERIPHERAL->DATA & LPDAC_DATA_DATA_MASK);
}

void DacDriverWriteZero(void)
{
    (void)DacDriverWriteCode(DAC_DRIVER_CODE_MIN);
}
