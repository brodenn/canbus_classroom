#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "stm32f4xx_hal.h"

void CAN_Interrupt_Init(void);  // Initiera avbrott för MCP2515 INT-pin
void EXTI3_IRQHandler(void);    // ISR för PB3 (INT från MCP2515)

#endif // INTERRUPTS_H
