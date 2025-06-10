#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void SysTick_Handler(void);
void EXTI3_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_IT_H */
