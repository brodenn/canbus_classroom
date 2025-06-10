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
    uart_print("MCP2515 CAN simulation starting...\r\n");

    mcp2515_init(&hspi1);
    mcp2515_write_register(&hspi1, MCP_RXB0CTRL, 0x60);
    mcp2515_write_register(&hspi1, MCP_RXB1CTRL, 0x60);
    mcp2515_set_mode(&hspi1, MODE_NORMAL);

    uart_print("MCP2515 ready in NORMAL mode.\r\n");

    uint32_t last_send_time = 0;
    uint8_t scenario = 0;
    uint8_t b1_state = 0, button_sent = 0;

    while (1) {
        uint32_t now = HAL_GetTick();

        if (now - last_send_time >= 1000) {
            // === [0x2C2] Levers: 8 bytes ===
            uint8_t levers[8] = {0};
            switch (scenario % 8) {
                case 0: levers[0] = 0x01; break;                 // Left blinker
                case 1: levers[0] = 0x02; break;                 // Right blinker
                case 2: levers[0] = 0x04; break;                 // Lever_towards
                case 3: levers[0] = 0x08; break;                 // Lever_headlights
                case 4: levers[1] = 0x02; levers[2] = 0x01; break; // Sensor mode, low
                case 5: levers[1] = 0x02; levers[2] = 0x05; break; // Sensor mode, med
                case 6: levers[1] = 0x02; levers[2] = 0x0D; break; // Sensor mode, high
                case 7: levers[1] = 0x04; break;                 // Wiper low
                // Add more if needed: levers[1] = 0x08; => Wiper high
            }
            mcp2515_send_message(&hspi1, 0x2C2, levers, 8);
            uart_print("Sent 0x2C2 (levers)\r\n");

            // === [0x451] Lights: 2 bytes ===
            uint8_t lights[2] = {0};
            switch (scenario % 4) {
                case 0: lights[0] = 0x02; break; // Lights_towards
                case 1: lights[0] = 0x04; break; // Headlights
                case 2: lights[1] = 0x01; break; // Left blinker
                case 3: lights[1] = 0x02; break; // Right blinker
            }
            mcp2515_send_message(&hspi1, 0x451, lights, 2);
            uart_print("Sent 0x451 (lights)\r\n");

            // === [0x459] Hood and Wiper Feedback ===
            const uint8_t hood_states[][2] = {
                {0x88, 0x00},  // Hood closed
                {0x88, 0x04},  // Hood open
                {0x81, 0x20}   // Wiping active
            };
            const uint8_t* hood = hood_states[scenario % 3];
            mcp2515_send_message(&hspi1, 0x459, (uint8_t*)hood, 2);

            scenario++;
            last_send_time = now;
        }

        // Button B1 toggle on PC13
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
            if (!button_sent) {
                b1_state ^= 1;
                uint8_t b1[1] = { b1_state };
                mcp2515_send_message(&hspi1, 0x160, b1, 1);
                uart_print(b1_state ? "B1 PRESSED\r\n" : "B1 RELEASED\r\n");
                button_sent = 1;
            }
        } else {
            button_sent = 0;
        }

        // Handle RX
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
