#include "encoder.h"
#include "py32f0xx.h"
#include "py32f0xx_ll_bus.h"
#include "py32f0xx_ll_gpio.h"

#define ENC_CLK_PORT GPIOA
#define ENC_CLK_PIN  LL_GPIO_PIN_7
#define ENC_DT_PORT  GPIOA
#define ENC_DT_PIN   LL_GPIO_PIN_6
#define BTN_PORT     GPIOB
#define BTN_PIN      LL_GPIO_PIN_0
#define ENC_DIV      4
#define BTN_DEBOUNCE_MS 20
#define ENC_STABLE_TICKS 1

extern volatile uint32_t g_ms;

static volatile int16_t encoder_raw;
static volatile uint8_t encoder_state;
static uint8_t encoder_candidate;
static uint8_t encoder_stable_ticks;
static uint8_t encoder_ready;

static volatile uint8_t button_down;
static volatile uint8_t button_press_event;
static volatile uint8_t button_release_event;
static uint8_t button_raw;
static uint8_t button_stable;
static uint32_t button_change_ms;

static const int8_t encoder_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t read_state(void)
{
  return (uint8_t)((LL_GPIO_IsInputPinSet(ENC_CLK_PORT, ENC_CLK_PIN) ? 2 : 0) |
                   (LL_GPIO_IsInputPinSet(ENC_DT_PORT, ENC_DT_PIN) ? 1 : 0));
}

void encoder_init(void)
{
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA | LL_IOP_GRP1_PERIPH_GPIOB);

  LL_GPIO_InitTypeDef gpio = {0};
  gpio.Pin = ENC_CLK_PIN | ENC_DT_PIN;
  gpio.Mode = LL_GPIO_MODE_INPUT;
  gpio.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(ENC_CLK_PORT, &gpio);

  gpio.Pin = BTN_PIN;
  LL_GPIO_Init(BTN_PORT, &gpio);

  encoder_raw = 0;
  encoder_state = read_state();
  encoder_candidate = encoder_state;
  encoder_stable_ticks = ENC_STABLE_TICKS;

  button_stable = LL_GPIO_IsInputPinSet(BTN_PORT, BTN_PIN) ? 0 : 1;
  button_raw = button_stable;
  button_down = button_stable;
  button_change_ms = g_ms;
  button_press_event = 0;
  button_release_event = 0;

  encoder_ready = 1;
}

void encoder_tick(void)
{
  if (!encoder_ready) return;

  uint8_t state = read_state();

  if (state != encoder_candidate) {
    encoder_candidate = state;
    encoder_stable_ticks = 0;
  } else if (encoder_stable_ticks < ENC_STABLE_TICKS) {
    encoder_stable_ticks++;
  }

  if (encoder_stable_ticks >= ENC_STABLE_TICKS && state != encoder_state) {
    int8_t delta = encoder_table[(encoder_state << 2) | state];
    if (delta) {
      int16_t next = encoder_raw + delta;
      if (next > 1024) next = 1024;
      if (next < -1024) next = -1024;
      encoder_raw = next;
    }
    encoder_state = state;
  }

  encoder_poll_button();
}

void encoder_poll_button(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  uint8_t pressed = LL_GPIO_IsInputPinSet(BTN_PORT, BTN_PIN) ? 0 : 1;

  if (pressed != button_raw) {
    button_raw = pressed;
    button_change_ms = g_ms;
  } else if (pressed != button_stable &&
             (uint32_t)(g_ms - button_change_ms) >= BTN_DEBOUNCE_MS) {
    button_stable = pressed;
    if (pressed) {
      button_press_event = 1;
      button_down = 1;
    } else {
      button_release_event = 1;
      button_down = 0;
    }
  }

  __set_PRIMASK(primask);
}

int16_t encoder_take_steps(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  int16_t raw = encoder_raw;
  encoder_raw = (int16_t)(raw % ENC_DIV);
  __set_PRIMASK(primask);
  return (int16_t)(raw / ENC_DIV);
}

uint8_t encoder_button_down(void)
{
  return button_down;
}

uint8_t encoder_button_pressed(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint8_t event = button_press_event;
  button_press_event = 0;
  __set_PRIMASK(primask);
  return event;
}

uint8_t encoder_button_released(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint8_t event = button_release_event;
  button_release_event = 0;
  __set_PRIMASK(primask);
  return event;
}
