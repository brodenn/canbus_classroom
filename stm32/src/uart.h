#ifndef UART_H
#define UART_H

#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart2;

void MX_USART2_UART_Init(void);
void uart_print(const char* msg);

#endif
