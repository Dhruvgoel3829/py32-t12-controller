#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

void oled_init(void);
void oled_flip(uint8_t flipped);
void oled_clear(void);
void oled_flush(void);
void oled_text(uint8_t x, uint8_t y, const char *text, uint8_t scale);
void oled_text_center(uint8_t y, const char *text, uint8_t scale);
uint8_t oled_text_width(const char *text, uint8_t scale);

#endif
