#ifndef BOARD_H
#define BOARD_H

#include "py32f0xx_ll_gpio.h"

#define HEATER_PORT        GPIOA
#define HEATER_PIN         LL_GPIO_PIN_1
#define HEATER_INVERTED    1

#define BUZZER_PORT        GPIOB
#define BUZZER_PIN         LL_GPIO_PIN_1

#define REED_PORT          GPIOA
#define REED_PIN           LL_GPIO_PIN_5

#define DISPLAY_SCL_PORT   GPIOA
#define DISPLAY_SCL_PIN    LL_GPIO_PIN_3
#define DISPLAY_SDA_PORT   GPIOA
#define DISPLAY_SDA_PIN    LL_GPIO_PIN_2

#define ROTARY_A_PORT      GPIOA
#define ROTARY_A_PIN       LL_GPIO_PIN_7
#define ROTARY_B_PORT      GPIOA
#define ROTARY_B_PIN       LL_GPIO_PIN_6
#define ROTARY_SW_PORT     GPIOB
#define ROTARY_SW_PIN      LL_GPIO_PIN_0

#endif
