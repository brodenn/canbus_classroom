#ifndef UART_LOG_H
#define UART_LOG_H

#include "stm32f4xx_hal.h"

// UART-hanterare (deklareras som extern från uart.c)
extern UART_HandleTypeDef huart2;

// Skickar sträng via UART
void uart_print(const char* msg);

// Skickar sträng följt av ny rad
void uart_println(const char* msg);

#endif
