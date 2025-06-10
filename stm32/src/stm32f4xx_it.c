#include "stm32f4xx_hal.h"

// Systemtick hanterare för HAL_Delay m.m.
void SysTick_Handler(void) {
    HAL_IncTick();
}

// EXTI3 Interrupt Handler (PB3 från MCP2515)
void EXTI3_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3); // Denna anropar HAL_GPIO_EXTI_Callback
}
