#include "GpioInputTask.h"

#include "GpioDmaFilter.h"
#include "WatchdogDriver.h"
#include "cmsis_os2.h"

typedef struct
{
    uint32_t pinMask;
    uint16_t debounceMs;
    volatile uint8_t stableState;
    uint16_t changeCount;
} gpio_input_state_t;

/*
 * Values are taken from app/task/GpioUpdateTask.c in dswing-main-control:
 * IN1=20 ms, IN10=5 ms, and IN5/IN6/IN7=50 ms.
 */
static gpio_input_state_t s_inputs[kGpioInputCount] = {
    [kGpioInputIn1]  = {GPIO_DMA_FILTER_IN1_MASK, 20U, 0U, 0U},
    [kGpioInputIn5]  = {GPIO_DMA_FILTER_IN5_MASK, 50U, 0U, 0U},
    [kGpioInputIn6]  = {GPIO_DMA_FILTER_IN6_MASK, 50U, 0U, 0U},
    [kGpioInputIn7]  = {GPIO_DMA_FILTER_IN7_MASK, 50U, 0U, 0U},
    [kGpioInputIn10] = {GPIO_DMA_FILTER_IN10_MASK, 5U, 0U, 0U},
};

static volatile uint32_t s_changeSequence;

void GpioInputInitStatus(void)
{
    uint32_t index;

    for (index = 0U; index < (uint32_t)kGpioInputCount; index++)
    {
        s_inputs[index].stableState = GpioDmaFilterReadPin(s_inputs[index].pinMask) ? 1U : 0U;
        s_inputs[index].changeCount = 0U;
    }

    s_changeSequence = 0U;
}

void GpioInputProcess1ms(void)
{
    uint32_t index;

    for (index = 0U; index < (uint32_t)kGpioInputCount; index++)
    {
        gpio_input_state_t *input = &s_inputs[index];
        uint8_t sampledState = GpioDmaFilterReadPin(input->pinMask) ? 1U : 0U;

        if (sampledState == input->stableState)
        {
            input->changeCount = 0U;
            continue;
        }

        if (input->changeCount < input->debounceMs)
        {
            input->changeCount += GPIO_INPUT_TASK_PERIOD_MS;
        }

        if (input->changeCount >= input->debounceMs)
        {
            input->stableState = sampledState;
            input->changeCount = 0U;
            s_changeSequence++;
        }
    }
}

void GpioInputTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        (void)osDelay(GPIO_INPUT_TASK_PERIOD_MS);
        GpioInputProcess1ms();
        /* Scheduler or input-task stalls therefore cause a WWDT reset in ~7 s. */
//        (void)WatchdogDriver_Refresh();
    }
}

bool GpioInputGetStableState(gpio_input_t input)
{
    if ((uint32_t)input >= (uint32_t)kGpioInputCount)
    {
        return false;
    }

    return s_inputs[input].stableState != 0U;
}

uint32_t GpioInputGetChangeSequence(void)
{
    return s_changeSequence;
}
