#include "can_buffer.h"
#include "mcp2515.h"
#include "stm32f4xx_hal.h"

// SPI-hanterare definierad externt (t.ex. i spi.c)
extern SPI_HandleTypeDef hspi1;

// Intern CAN RX-ringbuffer
volatile CanRxFrame can_rx_buffer[CAN_RX_BUFFER_SIZE];
volatile uint8_t can_rx_head = 0;
volatile uint8_t can_rx_tail = 0;

/**
 * @brief Buffrar ett mottaget CAN-meddelande från MCP2515 till lokal ringbuffer.
 *
 * @param sidh SIDH-register (höga ID-bitar)
 * @param sidl SIDL-register (låga ID-bitar)
 * @param dlc Data Length Code (1-8)
 * @param base_reg Startadress för data (t.ex. MCP_RXB0D0 eller MCP_RXB1D0)
 */
void buffer_rx_frame(uint8_t sidh, uint8_t sidl, uint8_t dlc, uint8_t base_reg) {
    uint16_t id = ((sidh << 3) | (sidl >> 5)) & 0x7FF;
    if (dlc == 0 || dlc > 8) return;

    uint8_t next = (can_rx_head + 1) % CAN_RX_BUFFER_SIZE;
    if (next == can_rx_tail) {
        // Buffern är full – ignorera
        return;
    }

    // Remove 'volatile' from the local pointer, only the array needs to be volatile
    CanRxFrame* frame = (CanRxFrame*)&can_rx_buffer[can_rx_head];
    frame->id = id;
    frame->dlc = dlc;

    for (uint8_t i = 0; i < dlc; i++) {
        frame->data[i] = mcp2515_read_register(&hspi1, base_reg + i);
    }

    can_rx_head = next;
}
