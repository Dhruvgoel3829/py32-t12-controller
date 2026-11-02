#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64

extern volatile uint8_t display_ack_primary;
extern volatile uint8_t display_ack_secondary;
extern volatile uint8_t display_ready;
extern volatile uint32_t display_nack_total;

void display_init(void);
void display_flip(uint8_t flipped);
void display_clear(void);
void display_refresh(void);
void display_text(uint8_t x, uint8_t y, const char *text, uint8_t scale);
void display_text_center(uint8_t y, const char *text, uint8_t scale);
uint8_t display_text_width(const char *text, uint8_t scale);

#endif
