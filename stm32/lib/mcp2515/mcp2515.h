#ifndef MCP2515_H
#define MCP2515_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>

// === MCP2515 Command Set ===
#define MCP_RESET         0xC0
#define MCP_READ          0x03
#define MCP_WRITE         0x02
#define MCP_BITMOD        0x05
#define MCP_READ_STATUS   0xA0
#define MCP_RTS_TX0       0x81

// === Register Addresses ===
// Control & Status
#define MCP_CANCTRL       0x0F
#define MCP_CANSTAT       0x0E
#define MCP_CANINTF       0x2C
#define MCP_CANINTE       0x2B  // Interrupt enable

// Bit Timing
#define MCP_CNF1          0x2A
#define MCP_CNF2          0x29
#define MCP_CNF3          0x28

// TX Buffer 0
#define MCP_TXB0CTRL      0x30
#define MCP_TXB0SIDH      0x31
#define MCP_TXB0SIDL      0x32
#define MCP_TXB0EID8      0x33
#define MCP_TXB0EID0      0x34
#define MCP_TXB0DLC       0x35
#define MCP_TXB0D0        0x36
#define MCP_TXB0D1        0x37
#define MCP_TXB0D2        0x38
#define MCP_TXB0D3        0x39
#define MCP_TXB0D4        0x3A
#define MCP_TXB0D5        0x3B
#define MCP_TXB0D6        0x3C
#define MCP_TXB0D7        0x3D

// RX Buffer 0
#define MCP_RXB0CTRL      0x60
#define MCP_RXB0SIDH      0x61
#define MCP_RXB0SIDL      0x62
#define MCP_RXB0EID8      0x63
#define MCP_RXB0EID0      0x64
#define MCP_RXB0DLC       0x65
#define MCP_RXB0D0        0x66
#define MCP_RXB0D1        0x67
#define MCP_RXB0D2        0x68
#define MCP_RXB0D3        0x69
#define MCP_RXB0D4        0x6A
#define MCP_RXB0D5        0x6B
#define MCP_RXB0D6        0x6C
#define MCP_RXB0D7        0x6D

// RX Buffer 1
#define MCP_RXB1CTRL      0x70
#define MCP_RXB1SIDH      0x71
#define MCP_RXB1SIDL      0x72
#define MCP_RXB1DLC       0x75
#define MCP_RXB1D0        0x76

// RX Filters and Masks
#define MCP_RXF0SIDH      0x00
#define MCP_RXF0SIDL      0x01
#define MCP_RXF1SIDH      0x04
#define MCP_RXF1SIDL      0x05
#define MCP_RXF2SIDH      0x08
#define MCP_RXF2SIDL      0x09
#define MCP_RXF3SIDH      0x10
#define MCP_RXF3SIDL      0x11
#define MCP_RXF4SIDH      0x14
#define MCP_RXF4SIDL      0x15
#define MCP_RXF5SIDH      0x18
#define MCP_RXF5SIDL      0x19

#define MCP_RXM0SIDH      0x20
#define MCP_RXM0SIDL      0x21
#define MCP_RXM1SIDH      0x24
#define MCP_RXM1SIDL      0x25

// MCP2515 Operating Modes
#define MODE_NORMAL       0x00
#define MODE_SLEEP        0x20
#define MODE_LOOPBACK     0x40
#define MODE_LISTENONLY   0x60
#define MODE_CONFIG       0x80

// === Function Declarations ===
void mcp2515_reset(SPI_HandleTypeDef* hspi);
void mcp2515_write_register(SPI_HandleTypeDef* hspi, uint8_t reg, uint8_t value);
uint8_t mcp2515_read_register(SPI_HandleTypeDef* hspi, uint8_t reg);
void mcp2515_bit_modify(SPI_HandleTypeDef* hspi, uint8_t reg, uint8_t mask, uint8_t data);
uint8_t mcp2515_read_status(SPI_HandleTypeDef* hspi);
bool mcp2515_set_mode(SPI_HandleTypeDef* hspi, uint8_t mode);
bool mcp2515_init(SPI_HandleTypeDef* hspi);
void mcp2515_send_test_frame(SPI_HandleTypeDef* hspi);
void mcp2515_send_message(SPI_HandleTypeDef* hspi, uint16_t id, uint8_t* data, uint8_t length);
bool mcp2515_receive_message(SPI_HandleTypeDef* hspi, uint16_t* id, uint8_t* data, uint8_t* len);

#endif // MCP2515_H
