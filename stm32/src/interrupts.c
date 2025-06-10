#include "stm32f4xx_hal.h"
#include "interrupts.h"
#include "mcp2515.h"
#include "can_buffer.h"

// SPI-hanterare deklarerad i annan fil (t.ex. spi.c)
extern SPI_HandleTypeDef hspi1;

// INT från MCP2515 kopplad till PB3 (EXTI3)
#define MCP_INT_PIN    GPIO_PIN_3
#define MCP_INT_PORT   GPIOB

// Initiera GPIO och EXTI för CAN-interrupt från MCP2515
void CAN_Interrupt_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Aktivera klockor
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    // PB3 som EXTI-fallande flank (MCP2515 INT)
    GPIO_InitStruct.Pin = MCP_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MCP_INT_PORT, &GPIO_InitStruct);

    // Aktivera NVIC-avbrott för EXTI3 (PB3)
    HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

// Callback som anropas av HAL när avbrott på PB3 (EXTI3) sker
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == MCP_INT_PIN) {
        uint8_t status = mcp2515_read_status(&hspi1);

        // RXB0 tillgängligt
        if (status & 0x01) {
            buffer_rx_frame(
                mcp2515_read_register(&hspi1, MCP_RXB0SIDH),
                mcp2515_read_register(&hspi1, MCP_RXB0SIDL),
                mcp2515_read_register(&hspi1, MCP_RXB0DLC) & 0x0F,
                MCP_RXB0D0
            );
            mcp2515_bit_modify(&hspi1, MCP_CANINTF, 0x01, 0x00); // Rensa RX0IF
        }

        // RXB1 tillgängligt
        if (status & 0x02) {
            buffer_rx_frame(
                mcp2515_read_register(&hspi1, MCP_RXB1SIDH),
                mcp2515_read_register(&hspi1, MCP_RXB1SIDL),
                mcp2515_read_register(&hspi1, MCP_RXB1DLC) & 0x0F,
                MCP_RXB1D0
            );
            mcp2515_bit_modify(&hspi1, MCP_CANINTF, 0x02, 0x00); // Rensa RX1IF
        }
    }
}
