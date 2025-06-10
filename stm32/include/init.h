#ifndef INIT_H
#define INIT_H

#include "stm32f4xx_hal.h"

void MX_USART2_UART_Init(void);
void MX_SPI1_Init(void);
void MX_GPIO_Init(void);
void SystemClock_Config(void);

#endif
