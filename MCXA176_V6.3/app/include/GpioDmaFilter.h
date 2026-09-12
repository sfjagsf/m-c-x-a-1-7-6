/*
 * GPIO3 DMA majority-vote filter.
 *
 * CTIMER4 M0 requests DMA0 CH7 every 100 us.  Eight GPIO3 PDIR samples are
 * collected, then the selected input bits are updated by a 5-of-8 vote.
 */
#ifndef APP_GPIO_DMA_FILTER_H_
#define APP_GPIO_DMA_FILTER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_DMA_FILTER_SAMPLE_COUNT (8U)

/* GPIO3 pins selected for DMA majority-vote filtering. */
#define GPIO_DMA_FILTER_IN5_MASK  (1UL << 17U)
#define GPIO_DMA_FILTER_IN1_MASK  (1UL << 18U)
#define GPIO_DMA_FILTER_IN10_MASK (1UL << 19U)
#define GPIO_DMA_FILTER_IN6_MASK  (1UL << 20U)
#define GPIO_DMA_FILTER_IN7_MASK  (1UL << 21U)
#define GPIO_DMA_FILTER_MASK      (GPIO_DMA_FILTER_IN5_MASK  | GPIO_DMA_FILTER_IN1_MASK  | \
                                   GPIO_DMA_FILTER_IN10_MASK | GPIO_DMA_FILTER_IN6_MASK  | \
                                   GPIO_DMA_FILTER_IN7_MASK)

/* Starts DMA0 CH7 and CTIMER4 after BOARD_InitBootPeripherals(). */
bool GpioDmaFilterStart(void);

/* Returns the current filtered GPIO3 PDIR shadow value. */
uint32_t GpioDmaFilterGetPortState(void);

/* Returns false for a low level and true for a high level. */
bool GpioDmaFilterReadPin(uint32_t pinMask);

/* Number of failed DMA rearms; useful during board bring-up. */
uint32_t GpioDmaFilterGetErrorCount(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_GPIO_DMA_FILTER_H_ */
