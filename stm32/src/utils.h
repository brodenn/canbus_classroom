#ifndef UTILS_H
#define UTILS_H

#include "can_buffer.h"
#include <stdint.h>

void byte_to_hex(uint8_t byte, char* str);
void print_rx_frame(const CanRxFrame* frame);
void handle_rx_frame(const CanRxFrame* frame);
void send_test_messages(uint8_t* counter, uint8_t* toggle, float* temp, float* batt, uint32_t* last_crash);
void poll_can_rx(void);

#endif
