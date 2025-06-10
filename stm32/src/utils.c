#include "utils.h"
#include "mcp2515.h"
#include "uart_log.h"
#include "stm32f4xx_hal.h"  // Needed for GPIO functions

extern SPI_HandleTypeDef hspi1;

void byte_to_hex(uint8_t byte, char* str) {
    const char hex[] = "0123456789ABCDEF";
    str[0] = hex[(byte >> 4) & 0x0F];
    str[1] = hex[byte & 0x0F];
    str[2] = '\0';
}

void print_rx_frame(const CanRxFrame* frame) {
    char buf[64];
    sprintf(buf, "RX Buffered: ID=0x%03X DLC=%d Data: ", frame->id, frame->dlc);
    uart_print(buf);
    for (uint8_t i = 0; i < frame->dlc; i++) {
        char hex[4];
        byte_to_hex(frame->data[i], hex);
        uart_print(hex);
        uart_print(" ");
    }
    uart_print("\r\n");
}

void handle_rx_frame(const CanRxFrame* frame) {
    if (frame->id == 0x170 && frame->dlc >= 1) {
        // Simulate LED state from received data
        uint8_t led_on = frame->data[0];
        uart_print(led_on ? "Simulated LD2 ON (0x170)\r\n" : "Simulated LD2 OFF (0x170)\r\n");

        // Send back LED status response (just echo what we received)
        uint8_t status = led_on ? 0x01 : 0x00;
        mcp2515_send_message(&hspi1, 0x171, &status, 1);
        uart_print("↩️ Sent simulated LED status on 0x171\r\n");
    }
}



void send_test_messages(uint8_t* counter, uint8_t* toggle, float* temp, float* batt, uint32_t* last_crash) {
    uint8_t test[4] = { 0x55, 0xAA, 0x00, (*counter)++ };
    mcp2515_send_message(&hspi1, 0x321, test, 4);

    uint8_t beam[1] = { *toggle };
    mcp2515_send_message(&hspi1, 0x110, beam, 1);

    float v = *batt + ((rand() % 5) - 2) * 0.05f;
    if (v < 11.4f) v = 11.4f;
    if (v > 12.6f) v = 12.6f;
    uint16_t v_fixed = (uint16_t)(v * 100);
    uint8_t batt_msg[2] = { v_fixed >> 8, v_fixed & 0xFF };
    mcp2515_send_message(&hspi1, 0x120, batt_msg, 2);

    uint32_t now = HAL_GetTick();
    if ((rand() % 10 == 0) || (now - *last_crash > 10000)) {
        uint8_t crash[2] = { 0xDE, 0xAD };
        mcp2515_send_message(&hspi1, 0x130, crash, 2);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        *last_crash = now;
    }

    *temp += 0.2f;
    if (*temp > 28.0f) *temp = 22.0f;
    uint16_t temp_fixed = (uint16_t)(*temp * 256);
    uint8_t temp_msg[2] = { temp_fixed >> 8, temp_fixed & 0xFF };
    mcp2515_send_message(&hspi1, 0x140, temp_msg, 2);

    uint8_t blink[1] = { *toggle };
    mcp2515_send_message(&hspi1, 0x150, blink, 1);

    *toggle ^= 1;
}

void poll_can_rx(void) {
    uint8_t status = mcp2515_read_status(&hspi1);
    if (status & 0x01) {
        buffer_rx_frame(
            mcp2515_read_register(&hspi1, MCP_RXB0SIDH),
            mcp2515_read_register(&hspi1, MCP_RXB0SIDL),
            mcp2515_read_register(&hspi1, MCP_RXB0DLC) & 0x0F,
            MCP_RXB0D0
        );
        mcp2515_bit_modify(&hspi1, MCP_CANINTF, 0x01, 0x00);
    }

    if (status & 0x02) {
        buffer_rx_frame(
            mcp2515_read_register(&hspi1, MCP_RXB1SIDH),
            mcp2515_read_register(&hspi1, MCP_RXB1SIDL),
            mcp2515_read_register(&hspi1, MCP_RXB1DLC) & 0x0F,
            MCP_RXB1D0
        );
        mcp2515_bit_modify(&hspi1, MCP_CANINTF, 0x02, 0x00);
    }
}
