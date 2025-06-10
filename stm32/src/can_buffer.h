#ifndef CAN_BUFFER_H
#define CAN_BUFFER_H

#include <stdint.h>

#define CAN_RX_BUFFER_SIZE 32

/**
 * @brief Representerar en mottagen CAN-frame.
 */
typedef struct {
    uint16_t id;       // 11-bit CAN ID
    uint8_t dlc;       // Data Length Code (0–8)
    uint8_t data[8];   // Upp till 8 bytes data
} CanRxFrame;

// Ringbuffert och dess pekare
extern volatile CanRxFrame can_rx_buffer[CAN_RX_BUFFER_SIZE];
extern volatile uint8_t can_rx_head;
extern volatile uint8_t can_rx_tail;

/**
 * @brief Lägger till en frame i RX-bufferten från MCP2515-register.
 *
 * @param sidh SIDH-register från MCP2515 (höga ID-bitar)
 * @param sidl SIDL-register från MCP2515 (låga ID-bitar)
 * @param dlc Data Length Code (antal bytes)
 * @param base_reg Startadress för databyte (t.ex. MCP_RXB0D0)
 */
void buffer_rx_frame(uint8_t sidh, uint8_t sidl, uint8_t dlc, uint8_t base_reg);

#endif // CAN_BUFFER_H
