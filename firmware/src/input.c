#include "input.h"
#include "board.h"
#include "py32f0xx.h"
#include "py32f0xx_ll_bus.h"
#include "py32f0xx_ll_gpio.h"

#define STEP_DIVIDER      4
#define BUTTON_SETTLE_MS  20
#define STATE_SETTLE_TICKS 1

extern volatile uint32_t system_ticks;

static volatile int16_t step_counter;
static volatile uint8_t sampled_state;
static uint8_t candidate_state;
static uint8_t state_settle_count;
static uint8_t initialized;

static volatile uint8_t button_state;
static volatile uint8_t press_event;
static volatile uint8_t release_event;
static uint8_t button_raw_state;
static uint8_t button_stable_state;
static uint32_t button_edge_ms;

static const int8_t phase_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t read_phase(void)
{
  return (uint8_t)((LL_GPIO_IsInputPinSet(ROTARY_A_PORT, ROTARY_A_PIN) ? 2 : 0) |
                   (LL_GPIO_IsInputPinSet(ROTARY_B_PORT, ROTARY_B_PIN) ? 1 : 0));
}

void input_init(void)
{
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA | LL_IOP_GRP1_PERIPH_GPIOB);

  LL_GPIO_InitTypeDef gpio = {0};
  gpio.Pin = ROTARY_A_PIN | ROTARY_B_PIN;
  gpio.Mode = LL_GPIO_MODE_INPUT;
  gpio.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(ROTARY_A_PORT, &gpio);

  gpio.Pin = ROTARY_SW_PIN;
  LL_GPIO_Init(ROTARY_SW_PORT, &gpio);

  step_counter = 0;
  sampled_state = read_phase();
  candidate_state = sampled_state;
  state_settle_count = STATE_SETTLE_TICKS;

  button_stable_state = LL_GPIO_IsInputPinSet(ROTARY_SW_PORT, ROTARY_SW_PIN) ? 0 : 1;
  button_raw_state = button_stable_state;
  button_state = button_stable_state;
  button_edge_ms = system_ticks;
  press_event = 0;
  release_event = 0;

  initialized = 1;
}

void input_tick(void)
{
  if (!initialized) return;

  uint8_t phase = read_phase();

  if (phase != candidate_state) {
    candidate_state = phase;
    state_settle_count = 0;
  } else if (state_settle_count < STATE_SETTLE_TICKS) {
    state_settle_count++;
  }

  if (state_settle_count >= STATE_SETTLE_TICKS && phase != sampled_state) {
    int8_t delta = phase_table[(sampled_state << 2) | phase];
    if (delta) {
      int16_t next = step_counter + delta;
      if (next > 1024) next = 1024;
      if (next < -1024) next = -1024;
      step_counter = next;
    }
    sampled_state = phase;
  }

  input_poll();
}

void input_poll(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  uint8_t pressed = LL_GPIO_IsInputPinSet(ROTARY_SW_PORT, ROTARY_SW_PIN) ? 0 : 1;

  if (pressed != button_raw_state) {
    button_raw_state = pressed;
    button_edge_ms = system_ticks;
  } else if (pressed != button_stable_state &&
             (uint32_t)(system_ticks - button_edge_ms) >= BUTTON_SETTLE_MS) {
    button_stable_state = pressed;
    if (pressed) {
      press_event = 1;
      button_state = 1;
    } else {
      release_event = 1;
      button_state = 0;
    }
  }

  __set_PRIMASK(primask);
}

int16_t input_take_steps(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  int16_t raw = step_counter;
  step_counter = (int16_t)(raw % STEP_DIVIDER);
  __set_PRIMASK(primask);
  return (int16_t)(raw / STEP_DIVIDER);
}

uint8_t input_is_down(void)
{
  return button_state;
}

uint8_t input_take_press(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint8_t event = press_event;
  press_event = 0;
  __set_PRIMASK(primask);
  return event;
}

uint8_t input_take_release(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint8_t event = release_event;
  release_event = 0;
  __set_PRIMASK(primask);
  return event;
}
