/* Five safety/process input debounce service, matching dswing-main-control. */
#ifndef APP_GPIO_INPUT_TASK_H_
#define APP_GPIO_INPUT_TASK_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    kGpioInputIn1 = 0,
    kGpioInputIn5,
    kGpioInputIn6,
    kGpioInputIn7,
    kGpioInputIn10,
    kGpioInputCount
} gpio_input_t;

/* dswing-main-control uses a 1-ms task period for these debounce counters. */
#define GPIO_INPUT_TASK_PERIOD_MS (1U)

/* Initialise confirmed states from the filtered GPIO shadow. */
void GpioInputInitStatus(void);

/* One 1-ms debounce iteration; exposed to allow deterministic unit testing. */
void GpioInputProcess1ms(void);

/* CMSIS-RTX thread entry. */
void GpioInputTask(void *argument);

/*
 * Default task creator.  It is weak so a product-level AppStartup module can
 * replace the task attributes or creation policy without changing this driver.
 */
bool GpioUpdateTask_Create(void);

/* Returns the confirmed raw pin level: false=low, true=high. */
bool GpioInputGetStableState(gpio_input_t input);

/* Incremented only after a confirmed input state changes. */
uint32_t GpioInputGetChangeSequence(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_GPIO_INPUT_TASK_H_ */
