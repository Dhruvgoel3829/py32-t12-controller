#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

void input_init(void);
void input_tick(void);
void input_poll(void);
int16_t input_take_steps(void);
uint8_t input_is_down(void);
uint8_t input_take_press(void);
uint8_t input_take_release(void);

#endif
