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

bool DacDriverSetOutputMilliVolts(uint16_t millivolts)
{
    uint32_t code;

    if (millivolts > DAC_DRIVER_OUTPUT_MAX_MV)
    {
        return false;
    }

    /* Rounded linear conversion: 10.000 V maps to the legacy STM32 code 3896. */
    code = (((uint32_t)millivolts * DAC_DRIVER_OUTPUT_10V_CODE) +
            (DAC_DRIVER_OUTPUT_MAX_MV / 2U)) /
           DAC_DRIVER_OUTPUT_MAX_MV;

    return DacDriverWriteCode((uint16_t)code);
}

uint16_t DacDriverReadCode(void)
{
    return (uint16_t)(DAC0_PERIPHERAL->DATA & LPDAC_DATA_DATA_MASK);
}

void DacDriverWriteZero(void)
{
    (void)DacDriverWriteCode(DAC_DRIVER_CODE_MIN);
}
