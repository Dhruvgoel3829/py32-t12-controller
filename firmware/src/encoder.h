#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

void encoder_init(void);
void encoder_tick(void);
void encoder_poll_button(void);
int16_t encoder_take_steps(void);
uint8_t encoder_button_down(void);
uint8_t encoder_button_pressed(void);
uint8_t encoder_button_released(void);

#endif
