#include "stm32f4xx_hal.h"
#include "mcp2515.h"
#include "can_buffer.h"
#include "uart_log.h"
#include "utils.h"

extern UART_HandleTypeDef huart2;
extern SPI_HandleTypeDef hspi1;

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);
void MX_SPI1_Init(void);

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_SPI1_Init();

    HAL_Delay(500);
    uart_print("MCP2515 CAN TX/RX Setup...\r\n");

    mcp2515_init(&hspi1);
    mcp2515_write_register(&hspi1, MCP_RXB0CTRL, 0x60);
    mcp2515_write_register(&hspi1, MCP_RXB1CTRL, 0x60);
    mcp2515_set_mode(&hspi1, MODE_NORMAL);
    uart_print("MCP2515 ready in NORMAL mode.\r\n");

    uint32_t last_send_time = 0;
    uint8_t scenario = 0;
    uint8_t button_sent = 0, b1_state = 0;

    const uint8_t blinker_states[][4] = {
        {0x01, 0x80, 0x00, 0x00}, // Vänster blinker
        {0x02, 0x80, 0x00, 0x00}, // Höger blinker
        {0x00, 0x82, 0x01, 0x00}, // Sensorläge, låg
        {0x00, 0x82, 0x05, 0x00}, // Sensorläge, medel
        {0x00, 0x82, 0x0D, 0x00}, // Sensorläge, hög
        {0x00, 0x85, 0x00, 0x00}, // Torkare LOW
        {0x00, 0x88, 0x00, 0x00}, // Torkare HIGH
        {0x08, 0x80, 0x00, 0x00}, // Helljus ON
        {0x00, 0x80, 0x00, 0x00}, // Helljus OFF
        {0x04, 0x80, 0x00, 0x00}  // Helljus blink
    };

    const uint8_t hood_msgs[][2] = {
        {0x88, 0x00}, // Huv stängd
        {0x88, 0x04}, // Huv öppen
        {0x81, 0x20}  // Torkare aktiv
    };

    while (1) {
        uint32_t now = HAL_GetTick();

        if (now - last_send_time >= 1000) {
            mcp2515_send_message(&hspi1, 0x2C2, (uint8_t*)blinker_states[scenario % 10], 4);
            char msgbuf[64];
            sprintf(msgbuf, "Sent 0x2C2: scenario %d\r\n", scenario % 10);
            uart_print(msgbuf);

            mcp2515_send_message(&hspi1, 0x459, (uint8_t*)hood_msgs[scenario % 3], 2);
            sprintf(msgbuf, "Sent 0x459: state %d\r\n", scenario % 3);
            uart_print(msgbuf);

            scenario++;
            last_send_time = now;
        }

        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
            if (!button_sent) {
                b1_state ^= 1;
                uint8_t button_msg[1] = { b1_state };
                mcp2515_send_message(&hspi1, 0x160, button_msg, 1);
                uart_print(b1_state ? "Sent 0x160: B1 PRESSED\r\n" : "Sent 0x160: B1 RELEASED\r\n");
                button_sent = 1;
            }
        } else {
            button_sent = 0;
        }

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

        while (can_rx_tail != can_rx_head) {
            CanRxFrame* f = &can_rx_buffer[can_rx_tail];
            print_rx_frame(f);
            handle_rx_frame(f);
            can_rx_tail = (can_rx_tail + 1) % CAN_RX_BUFFER_SIZE;
        }

        HAL_Delay(1);
    }
}
