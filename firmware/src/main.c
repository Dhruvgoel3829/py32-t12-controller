// Port of wagiminator/ATmega-Soldering-Station v1.9 to PY32F002A.
// Original project: Copyright (c) Stefan Wagner, CC BY-SA 3.0.

#include <stdint.h>
#include <string.h>
#include "py32f0xx.h"
#include "py32f0xx_ll_bus.h"
#include "py32f0xx_ll_gpio.h"
#include "py32f0xx_ll_rcc.h"
#include "py32f0xx_ll_system.h"
#include "py32f0xx_ll_tim.h"
#include "py32f0xx_ll_utils.h"
#include "adc.h"
#include "encoder.h"
#include "oled.h"
#include "settings.h"

#define FIRMWARE_VERSION "V1.9-PY32"

#define TEMP_MIN       150
#define TEMP_MAX       400
#define TEMP_STEP      10
#define TEMP_SLEEP     150
#define TEMP_BOOST     50
#define TEMP_ERR       500
#define TEMP_CUTOFF    450
#define SLEEP_TEMP_MIN 20
#define SLEEP_TEMP_MAX 200
#define BOOST_TEMP_MIN 10
#define BOOST_TEMP_MAX 100
#define TIME2SLEEP     5
#define TIME2OFF       15
#define TIME_BOOST     40

#define HEATER_ACTIVE_LOW 1
#define BUZZER_PORT       GPIOB
#define BUZZER_PIN        LL_GPIO_PIN_1
#define REED_PORT         GPIOA
#define REED_PIN          LL_GPIO_PIN_5

#define LONG_PRESS_MS     500
#define MEASURE_PERIOD_MS 50
#define UI_PERIOD_MS      100

volatile uint32_t g_ms;

static settings_t config;
static int16_t set_temp;
static int16_t current_temp;
static int16_t show_temp;
static int16_t setpoint;
static int16_t pid_last_input;
static int32_t raw_smooth_x100;
static int32_t pid_integral;
static uint8_t heater_power;
static uint8_t tip_present;
static uint8_t is_worky;
static uint8_t beep_if_worky;
static uint8_t in_sleep;
static uint8_t in_off;
static uint8_t in_boost;
static uint8_t menu_dirty;
static uint8_t button_was_pressed;
static uint8_t last_reed;
static uint8_t vin_divider_counter;
static uint16_t vin_mv;
static uint32_t last_move_ms;
static uint32_t boost_start_ms;
static uint32_t last_measure_ms;
static uint32_t last_ui_ms;
static uint32_t last_pid_ms;
static uint32_t button_press_ms;

static int16_t abs16(int16_t value)
{
  return value < 0 ? (int16_t)-value : value;
}

static void delay_us(uint32_t us)
{
  uint32_t count = us * 6U;
  while (count--) __NOP();
}

static void mdelay(uint32_t ms)
{
  uint32_t start = g_ms;
  while ((uint32_t)(g_ms - start) < ms) {
  }
}

static void u16_to_str(uint16_t value, char *text)
{
  char temp[6];
  uint8_t length = 0;

  if (value == 0) {
    text[0] = '0';
    text[1] = 0;
    return;
  }
  while (value) {
    temp[length++] = (char)('0' + value % 10);
    value /= 10;
  }
  for (uint8_t i = 0; i < length; i++) text[i] = temp[length - 1 - i];
  text[length] = 0;
}

static void volts_to_str(uint16_t mv, char *text)
{
  uint32_t tenths = ((uint32_t)mv + 50U) / 100U;
  uint8_t pos = 0;

  if (tenths >= 100U) text[pos++] = (char)('0' + (tenths / 100U) % 10U);
  text[pos++] = (char)('0' + (tenths / 10U) % 10U);
  text[pos++] = '.';
  text[pos++] = (char)('0' + tenths % 10U);
  text[pos++] = 'V';
  text[pos] = 0;
}

static void clock_init(void)
{
  LL_RCC_HSI_Enable();
  LL_RCC_HSI_SetCalibFreq(LL_RCC_HSICALIBRATION_24MHz);
  while (LL_RCC_HSI_IsReady() == 0) {
  }

  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSISYS);
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSISYS) {
  }

  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_SetSystemCoreClock(24000000);
  LL_Init1msTick(24000000);
  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

static void buzzer_init(void)
{
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
  LL_GPIO_InitTypeDef gpio = {0};
  gpio.Pin = BUZZER_PIN;
  gpio.Mode = LL_GPIO_MODE_OUTPUT;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Pull = LL_GPIO_PULL_NO;
  gpio.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(BUZZER_PORT, &gpio);
  LL_GPIO_ResetOutputPin(BUZZER_PORT, BUZZER_PIN);
}

static void beep(void)
{
  if (!config.beep_enable) return;

  for (uint8_t i = 0; i < 220; i++) {
    LL_GPIO_SetOutputPin(BUZZER_PORT, BUZZER_PIN);
    delay_us(125);
    LL_GPIO_ResetOutputPin(BUZZER_PORT, BUZZER_PIN);
    delay_us(125);
  }
}

static void beep_twice(void)
{
  beep();
  mdelay(80);
  beep();
}

static void heater_set_power(uint8_t power)
{
  uint32_t compare = HEATER_ACTIVE_LOW ? (256UL - power) : power;
  LL_TIM_OC_SetCompareCH4(TIM1, compare);
}

static void heater_init(void)
{
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM1);

  LL_GPIO_InitTypeDef gpio = {0};
  gpio.Pin = LL_GPIO_PIN_1;
  gpio.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio.Alternate = LL_GPIO_AF_13;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Pull = LL_GPIO_PULL_NO;
  gpio.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  LL_GPIO_Init(GPIOA, &gpio);

  LL_TIM_InitTypeDef tim = {0};
  tim.Prescaler = (SystemCoreClock / 256000UL) - 1;
  tim.CounterMode = LL_TIM_COUNTERMODE_UP;
  tim.Autoreload = 255;
  tim.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  tim.RepetitionCounter = 0;
  LL_TIM_Init(TIM1, &tim);

  LL_TIM_OC_InitTypeDef oc = {0};
  oc.OCMode = LL_TIM_OCMODE_PWM1;
  oc.OCState = LL_TIM_OCSTATE_ENABLE;
  oc.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
  oc.OCIdleState = LL_TIM_OCIDLESTATE_LOW;
  oc.OCNState = LL_TIM_OCSTATE_DISABLE;
  oc.OCNPolarity = LL_TIM_OCPOLARITY_HIGH;
  oc.OCNIdleState = LL_TIM_OCIDLESTATE_LOW;
  oc.CompareValue = 256;
  LL_TIM_OC_Init(TIM1, LL_TIM_CHANNEL_CH4, &oc);
  LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH4);

  LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH4);
  LL_TIM_EnableAllOutputs(TIM1);
  LL_TIM_EnableCounter(TIM1);
  heater_set_power(0);
}

static void reed_init(void)
{
  LL_GPIO_InitTypeDef gpio = {0};
  gpio.Pin = REED_PIN;
  gpio.Mode = LL_GPIO_MODE_INPUT;
  gpio.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(REED_PORT, &gpio);
  last_reed = LL_GPIO_IsInputPinSet(REED_PORT, REED_PIN) ? 1 : 0;
}

static uint8_t reed_read(void)
{
  return LL_GPIO_IsInputPinSet(REED_PORT, REED_PIN) ? 1 : 0;
}

static int16_t raw_to_temp(int16_t raw)
{
  int32_t value;

  if (raw < 0) raw = 0;
  if (raw < 200) {
    value = 21 + ((int32_t)raw * (config.cal200 - 21)) / 200;
  } else if (raw < 280) {
    value = config.cal200 + ((int32_t)(raw - 200) * (config.cal280 - config.cal200)) / 80;
  } else {
    value = config.cal280 + ((int32_t)(raw - 280) * (config.cal360 - config.cal280)) / 80;
  }

  if (value < -50) value = -50;
  if (value > 999) value = 999;
  return (int16_t)value;
}

static int16_t measure_tip_raw(void)
{
  heater_set_power(0);
  delay_us(950);
  uint16_t raw = adc_read_tip_raw();
  heater_set_power(heater_power);
  return (int16_t)(raw >> 2);
}

static void update_temperature(int16_t raw)
{
  int32_t target = (int32_t)raw * 100;
  raw_smooth_x100 += ((target - raw_smooth_x100) * 5) / 100;
  current_temp = raw_to_temp((int16_t)((raw_smooth_x100 + 50) / 100));
}

static void normalize_config(void)
{
  if (config.default_temp < TEMP_MIN || config.default_temp > TEMP_MAX)
    config.default_temp = 320;
  if (config.sleep_temp < SLEEP_TEMP_MIN || config.sleep_temp > SLEEP_TEMP_MAX)
    config.sleep_temp = TEMP_SLEEP;
  if (config.boost_temp < BOOST_TEMP_MIN || config.boost_temp > BOOST_TEMP_MAX)
    config.boost_temp = TEMP_BOOST;
  if (config.time2sleep > 30) config.time2sleep = TIME2SLEEP;
  if (config.time2off > 60) config.time2off = TIME2OFF;
  if (config.time_boost > 180) config.time_boost = TIME_BOOST;
  if (config.cal200 < 50 || config.cal280 <= config.cal200 ||
      config.cal360 <= config.cal280) {
    config.cal200 = 216;
    config.cal280 = 308;
    config.cal360 = 390;
  }
  if (config.tip_name[0] == 0 || config.tip_name[0] == (char)0xFF) {
    memcpy(config.tip_name, "BC1.5", 6);
  }
  config.tip_name[7] = 0;
}

static void update_setpoint(void)
{
  if (in_off) setpoint = 0;
  else if (in_sleep) setpoint = config.sleep_temp;
  else if (in_boost) {
    int16_t boost = set_temp + config.boost_temp;
    if (boost > TEMP_CUTOFF) boost = TEMP_CUTOFF;
    setpoint = boost;
  } else {
    setpoint = set_temp;
  }
}

static void register_activity(void)
{
  if (in_sleep || in_off) {
    in_sleep = 0;
    in_off = 0;
    in_boost = 0;
    beep_if_worky = 1;
    beep();
  }
  last_move_ms = g_ms;
}

static void timer_service(void)
{
  uint32_t now = g_ms;
  uint32_t gone_minutes = (now - last_move_ms) / 60000UL;

  if (!in_sleep && config.time2sleep > 0 && gone_minutes >= config.time2sleep) {
    in_sleep = 1;
    beep();
  }
  if (!in_off && config.time2off > 0 && gone_minutes >= config.time2off) {
    in_off = 1;
    beep();
  }
  if (in_boost &&
      (uint32_t)(now - boost_start_ms) >= (uint32_t)config.time_boost * 1000UL) {
    in_boost = 0;
    beep_if_worky = 1;
    beep();
  }
}

static void sensor_service(void)
{
  update_setpoint();

  int16_t raw = measure_tip_raw();
  update_temperature(raw);

  if (++vin_divider_counter >= 40) {
    vin_divider_counter = 0;
    vin_mv = adc_read_vin_mv();
  }

  uint8_t reed = reed_read();
  if (reed != last_reed) {
    last_reed = reed;
    register_activity();
  }

  if ((show_temp != setpoint) || (abs16((int16_t)(show_temp - current_temp)) > 5))
    show_temp = current_temp;
  if (abs16((int16_t)(show_temp - setpoint)) <= 1) show_temp = setpoint;

  if (show_temp > TEMP_ERR) tip_present = 0;
  if (!tip_present && show_temp < TEMP_ERR) {
    tip_present = 1;
    heater_power = 0;
    heater_set_power(0);
    raw_smooth_x100 = (int32_t)raw * 100;
    pid_integral = 0;
    pid_last_input = current_temp;
    beep();
    register_activity();
  }

  int16_t gap = abs16((int16_t)(set_temp - current_temp));
  if (gap < 5) {
    if (!is_worky && beep_if_worky) beep();
    is_worky = 1;
    beep_if_worky = 0;
  } else {
    is_worky = 0;
  }
}

static void thermostat(void)
{
  update_setpoint();

  if (!tip_present || in_off || show_temp > TEMP_ERR || current_temp > TEMP_CUTOFF) {
    heater_power = 0;
    pid_integral = 0;
    heater_set_power(0);
    return;
  }

  if (config.pid_enable) {
    uint32_t now = g_ms;
    if ((uint32_t)(now - last_pid_ms) >= 100) {
      int16_t error = setpoint - current_temp;
      int16_t gap = abs16(error);
      int32_t kp, ki, kd;

      if (gap < 30) {
        kp = 110;
        ki = 3;
        kd = 500;
      } else {
        kp = 110;
        ki = 5;
        kd = 100;
      }

      pid_integral += ki * error;
      if (pid_integral > 2550) pid_integral = 2550;
      if (pid_integral < 0) pid_integral = 0;

      int32_t derivative = current_temp - pid_last_input;
      int32_t output = (kp * error + pid_integral - kd * derivative) / 10;
      if (output < 0) output = 0;
      if (output > 255) output = 255;

      heater_power = (uint8_t)output;
      pid_last_input = current_temp;
      last_pid_ms = now;
    }
  } else {
    heater_power = (current_temp + 1 < setpoint) ? 255 : 0;
    pid_integral = 0;
  }

  heater_set_power(heater_power);
}

static const char *state_text(void)
{
  if (!tip_present || show_temp > TEMP_ERR) return "ERROR";
  if (in_off) return "OFF";
  if (in_sleep) return "SLEEP";
  if (in_boost) return "BOOST";
  if (is_worky) return "WORKY";
  if (heater_power > 75) return "HEAT";
  return "HOLD";
}

static void draw_main_screen(void)
{
  char text[16];

  oled_clear();
  oled_text(0, 0, "SET:", 1);
  u16_to_str((uint16_t)set_temp, text);
  oled_text(24, 0, text, 1);
  oled_text(84, 0, state_text(), 1);

  if (show_temp > TEMP_ERR) {
    oled_text_center(22, "---", 3);
  } else {
    u16_to_str((uint16_t)show_temp, text);
    oled_text_center(22, text, 3);
  }

  oled_text(0, 56, config.tip_name, 1);

  volts_to_str(vin_mv, text);
  oled_text(92, 56, text, 1);

  oled_flush();
}

static void menu_draw(const char *title, const char *const *items,
                      uint8_t count, uint8_t selected)
{
  uint8_t start = selected > 3 ? (uint8_t)(selected - 3) : 0;

  oled_clear();
  oled_text(0, 0, title, 1);
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t index = start + i;
    if (index >= count) break;
    uint8_t y = (uint8_t)(12 + i * 12);
    if (index == selected) oled_text(0, y, ">", 1);
    oled_text(8, y, items[index], 1);
  }
  oled_flush();
}

static void ui_clear_events(void)
{
  encoder_button_pressed();
  encoder_button_released();
}

static uint8_t menu_run(const char *title, const char *const *items,
                        uint8_t count, uint8_t selected)
{
  uint8_t drawn = 0xFF;

  ui_clear_events();
  for (;;) {
    encoder_poll_button();
    int16_t steps = encoder_take_steps();
    if (config.ec_reverse) steps = (int16_t)-steps;
    if (steps) {
      int16_t next = (int16_t)selected + steps;
      while (next < 0) next += count;
      while (next >= count) next -= count;
      selected = (uint8_t)next;
    }

    if (selected != drawn) {
      menu_draw(title, items, count, selected);
      drawn = selected;
    }

    if (encoder_button_pressed()) {
      while (encoder_button_down()) {
        encoder_poll_button();
        LL_mDelay(2);
      }
      ui_clear_events();
      beep();
      return selected;
    }
    LL_mDelay(2);
  }
}

static uint16_t input_run(const char *title, const char *unit, uint16_t value,
                          uint16_t minimum, uint16_t maximum, uint16_t step)
{
  char text[8];
  uint16_t drawn = 0xFFFF;

  ui_clear_events();
  for (;;) {
    encoder_poll_button();
    int16_t steps = encoder_take_steps();
    if (config.ec_reverse) steps = (int16_t)-steps;
    if (steps) {
      int32_t next = (int32_t)value + (int32_t)steps * step;
      if (next < minimum) next = minimum;
      if (next > maximum) next = maximum;
      value = (uint16_t)next;
    }

    if (value != drawn) {
      oled_clear();
      oled_text(0, 0, title, 1);
      if (value == 0) strcpy(text, "OFF");
      else u16_to_str(value, text);
      oled_text_center(20, text, 2);
      oled_text(0, 40, unit, 1);
      oled_text(0, 54, "PRESS TO SET", 1);
      oled_flush();
      drawn = value;
    }

    if (encoder_button_pressed()) {
      while (encoder_button_down()) {
        encoder_poll_button();
        LL_mDelay(2);
      }
      ui_clear_events();
      beep();
      return value;
    }
    LL_mDelay(2);
  }
}

static uint8_t option_run(const char *title, uint8_t value)
{
  static const char *const items[] = { "DISABLE", "ENABLE" };
  return menu_run(title, items, 2, value ? 1 : 0);
}

static uint8_t control_type_run(uint8_t value)
{
  static const char *const items[] = { "DIRECT", "PID" };
  return menu_run("CONTROL TYPE", items, 2, value ? 1 : 0);
}

static void message_run(const char *line1, const char *line2)
{
  oled_clear();
  oled_text(0, 8, line1, 1);
  oled_text(0, 24, line2, 1);
  oled_text(0, 54, "PRESS TO CONTINUE", 1);
  oled_flush();

  ui_clear_events();
  for (;;) {
    encoder_poll_button();
    if (encoder_button_pressed()) {
      while (encoder_button_down()) {
        encoder_poll_button();
        LL_mDelay(2);
      }
      ui_clear_events();
      beep();
      return;
    }
    LL_mDelay(2);
  }
}

static void adc_monitor_screen(void)
{
  char text[8];
  uint32_t last = 0;

  heater_power = 0;
  heater_set_power(0);
  ui_clear_events();

  for (;;) {
    encoder_poll_button();
    encoder_take_steps();

    if ((uint32_t)(g_ms - last) >= 250) {
      last = g_ms;
      uint16_t tip = adc_read_tip_raw();
      uint16_t vin = adc_read_vin_raw();
      uint16_t vref = adc_read_vref_raw();
      uint16_t tsen = adc_read_temp_raw();
      heater_set_power(0);

      oled_clear();
      oled_text(0, 0, "ADC RAW  HEATER OFF", 1);
      oled_text(0, 12, "PA4 TIP", 1);
      u16_to_str(tip, text);
      oled_text(72, 12, text, 1);
      oled_text(0, 22, "PA0 VIN", 1);
      u16_to_str(vin, text);
      oled_text(72, 22, text, 1);
      oled_text(0, 32, "VREF", 1);
      u16_to_str(vref, text);
      oled_text(72, 32, text, 1);
      oled_text(0, 42, "TEMP SENS", 1);
      u16_to_str(tsen, text);
      oled_text(72, 42, text, 1);
      oled_text(0, 54, "PRESS TO EXIT", 1);
      oled_flush();
    }

    if (encoder_button_pressed()) {
      while (encoder_button_down()) {
        encoder_poll_button();
        LL_mDelay(2);
      }
      ui_clear_events();
      beep();
      return;
    }
    LL_mDelay(5);
  }
}

static void info_screen(void)
{
  char text[16];
  uint8_t drawn = 0;

  ui_clear_events();
  for (;;) {
    encoder_poll_button();
    encoder_take_steps();

    if (!drawn) {
      int16_t chip = adc_read_chip_temp();
      uint16_t vdda = adc_read_vdda_mv();
      uint16_t vin = adc_read_vin_mv();

      oled_clear();
      oled_text(0, 0, "INFORMATION", 1);
      oled_text(0, 12, "FW:", 1);
      oled_text(24, 12, FIRMWARE_VERSION, 1);

      oled_text(0, 24, "CHIP:", 1);
      u16_to_str((uint16_t)(chip < 0 ? 0 : chip), text);
      oled_text(42, 24, text, 1);
      oled_text(60, 24, "C", 1);

      oled_text(0, 36, "VIN:", 1);
      volts_to_str(vin, text);
      oled_text(42, 36, text, 1);

      oled_text(0, 48, "VCC:", 1);
      volts_to_str(vdda, text);
      oled_text(42, 48, text, 1);

      oled_flush();
      drawn = 1;
    }

    if (encoder_button_pressed()) {
      while (encoder_button_down()) {
        encoder_poll_button();
        LL_mDelay(2);
      }
      ui_clear_events();
      beep();
      return;
    }
    LL_mDelay(10);
  }
}

static void temp_settings(void)
{
  static const char *const items[] = { "DEFAULT TEMP", "SLEEP TEMP", "BOOST TEMP", "RETURN" };
  uint8_t selected = 0;

  for (;;) {
    selected = menu_run("TEMP SETTINGS", items, 4, selected);
    if (selected == 0) {
      uint16_t value = input_run("DEFAULT TEMP", "CELSIUS", config.default_temp,
                                 TEMP_MIN, TEMP_MAX, TEMP_STEP);
      if (value != config.default_temp) {
        config.default_temp = value;
        menu_dirty = 1;
      }
    } else if (selected == 1) {
      uint16_t value = input_run("SLEEP TEMP", "CELSIUS", config.sleep_temp,
                                 SLEEP_TEMP_MIN, SLEEP_TEMP_MAX, TEMP_STEP);
      if (value != config.sleep_temp) {
        config.sleep_temp = value;
        menu_dirty = 1;
      }
    } else if (selected == 2) {
      uint16_t value = input_run("BOOST TEMP", "CELSIUS", config.boost_temp,
                                 BOOST_TEMP_MIN, BOOST_TEMP_MAX, TEMP_STEP);
      if (value != config.boost_temp) {
        config.boost_temp = (uint8_t)value;
        menu_dirty = 1;
      }
    } else {
      return;
    }
  }
}

static void timer_settings(void)
{
  static const char *const items[] = { "SLEEP TIMER", "OFF TIMER", "BOOST TIMER", "RETURN" };
  uint8_t selected = 0;

  for (;;) {
    selected = menu_run("TIMER SETTINGS", items, 4, selected);
    if (selected == 0) {
      uint16_t value = input_run("SLEEP TIMER", "MINUTES", config.time2sleep, 0, 30, 1);
      if (value != config.time2sleep) {
        config.time2sleep = (uint8_t)value;
        menu_dirty = 1;
      }
    } else if (selected == 1) {
      uint16_t value = input_run("OFF TIMER", "MINUTES", config.time2off, 0, 60, 5);
      if (value != config.time2off) {
        config.time2off = (uint8_t)value;
        menu_dirty = 1;
      }
    } else if (selected == 2) {
      uint16_t value = input_run("BOOST TIMER", "SECONDS", config.time_boost, 0, 180, 10);
      if (value != config.time_boost) {
        config.time_boost = (uint8_t)value;
        menu_dirty = 1;
      }
    } else {
      return;
    }
  }
}

static void calibration_screen(void)
{
  uint16_t measured[3];
  uint16_t target[3] = { config.cal200, config.cal280, config.cal360 };

  heater_power = 255;
  heater_set_power(255);

  for (uint8_t step = 0; step < 3; step++) {
    uint16_t value = target[step];
    uint32_t last_measure = 0;
    char text[8];

    ui_clear_events();
    for (;;) {
      encoder_poll_button();
      int16_t steps = encoder_take_steps();
      if (config.ec_reverse) steps = (int16_t)-steps;
      if (steps) {
        int32_t next = (int32_t)value + (int32_t)steps;
        if (next < 100) next = 100;
        if (next > 500) next = 500;
        value = (uint16_t)next;
      }

      if ((uint32_t)(g_ms - last_measure) >= MEASURE_PERIOD_MS) {
        last_measure = g_ms;
        int16_t raw = measure_tip_raw();
        update_temperature(raw);
        heater_power = (current_temp + 1 < (int16_t)target[step]) ? 255 : 0;
        heater_set_power(heater_power);
      }

      oled_clear();
      oled_text(0, 0, "CALIBRATION", 1);
      oled_text(0, 10, "STEP", 1);
      text[0] = (char)('1' + step);
      text[1] = '/';
      text[2] = '3';
      text[3] = 0;
      oled_text(30, 10, text, 1);
      oled_text(0, 20, "SET", 1);
      u16_to_str(target[step], text);
      oled_text(30, 20, text, 1);
      oled_text(0, 30, "ACT", 1);
      u16_to_str((uint16_t)(current_temp < 0 ? 0 : current_temp), text);
      oled_text(30, 30, text, 1);
      oled_text(0, 40, "MEAS", 1);
      u16_to_str(value, text);
      oled_text(36, 40, text, 1);
      oled_text(0, 54, "CLICK WHEN READY", 1);
      oled_flush();

      if (encoder_button_pressed()) {
        while (encoder_button_down()) {
          encoder_poll_button();
          LL_mDelay(2);
        }
        ui_clear_events();
        break;
      }
      LL_mDelay(5);
    }
    measured[step] = value;
    beep();
  }

  heater_power = 0;
  heater_set_power(0);

  if (measured[0] + 10 < measured[1] && measured[1] + 10 < measured[2]) {
    config.cal200 = measured[0];
    config.cal280 = measured[1];
    config.cal360 = measured[2];
    menu_dirty = 1;
    message_run("CALIBRATION", "SAVED");
  } else {
    message_run("CALIBRATION", "FAILED");
  }
}

static void settings_menu(void)
{
  static const char *const items[] = {
    "TEMPERATURE", "TIMERS", "CONTROL", "BUZZER",
    "SCREEN FLIP", "EC REVERSE", "CALIBRATE", "INFORMATION",
    "ADC MONITOR", "SAVE & EXIT"
  };
  uint8_t selected = 0;
  uint8_t old_power = heater_power;
  uint16_t old_set_temp = set_temp;

  heater_power = 0;
  heater_set_power(0);
  in_boost = 0;
  in_sleep = 0;
  in_off = 0;
  beep();

  for (;;) {
    selected = menu_run("SETUP MENU", items, 10, selected);
    if (selected == 0) {
      temp_settings();
    } else if (selected == 1) {
      timer_settings();
    } else if (selected == 2) {
      uint8_t value = control_type_run(config.pid_enable);
      if (value != config.pid_enable) {
        config.pid_enable = value;
        menu_dirty = 1;
      }
    } else if (selected == 3) {
      uint8_t value = option_run("BUZZER", config.beep_enable);
      if (value != config.beep_enable) {
        config.beep_enable = value;
        menu_dirty = 1;
      }
    } else if (selected == 4) {
      uint8_t value = option_run("SCREEN FLIP", config.body_flip);
      if (value != config.body_flip) {
        config.body_flip = value;
        menu_dirty = 1;
        oled_flip(config.body_flip);
      }
    } else if (selected == 5) {
      uint8_t value = option_run("EC REVERSE", config.ec_reverse);
      if (value != config.ec_reverse) {
        config.ec_reverse = value;
        menu_dirty = 1;
      }
    } else if (selected == 6) {
      calibration_screen();
    } else if (selected == 7) {
      info_screen();
    } else if (selected == 8) {
      adc_monitor_screen();
    } else {
      break;
    }
  }

  if (menu_dirty) {
    settings_save(&config);
    menu_dirty = 0;
  }
  set_temp = old_set_temp;
  heater_power = old_power;
  register_activity();
  beep();
}

static void handle_main_button(void)
{
  if (encoder_button_pressed()) {
    button_press_ms = g_ms;
    button_was_pressed = 1;
  }

  if (encoder_button_released()) {
    if (button_was_pressed) {
      uint32_t duration = g_ms - button_press_ms;
      button_was_pressed = 0;
      if (duration >= LONG_PRESS_MS) {
        settings_menu();
        ui_clear_events();
      } else {
        in_boost = !in_boost;
        if (in_boost) {
          boost_start_ms = g_ms;
          beep();
          beep_if_worky = 1;
        } else {
          beep();
        }
        register_activity();
      }
      last_ui_ms = 0;
    }
  }

  if (button_was_pressed && encoder_button_down() &&
      (uint32_t)(g_ms - button_press_ms) >= LONG_PRESS_MS) {
    button_was_pressed = 0;
    encoder_button_released();
    settings_menu();
    ui_clear_events();
    last_ui_ms = 0;
  }
}

int main(void)
{
  clock_init();
  buzzer_init();
  heater_init();
  reed_init();
  adc_init();
  encoder_init();
  oled_init();

  g_ms = 0;

  if (!settings_load(&config)) {
    settings_save(&config);
  }
  normalize_config();

  oled_flip(config.body_flip);
  set_temp = config.default_temp;
  beep_if_worky = 1;

  raw_smooth_x100 = (int32_t)measure_tip_raw() * 100;
  current_temp = raw_to_temp((int16_t)(raw_smooth_x100 / 100));
  show_temp = current_temp;
  tip_present = show_temp < TEMP_ERR;
  pid_last_input = current_temp;
  vin_mv = adc_read_vin_mv();
  last_move_ms = g_ms;
  last_measure_ms = g_ms;
  last_ui_ms = g_ms;
  last_pid_ms = g_ms;

  update_setpoint();
  heater_power = (tip_present && current_temp + 20 < setpoint) ? 255 : 0;
  heater_set_power(heater_power);

  oled_clear();
  oled_text_center(8, "T12 STATION", 1);
  oled_text_center(24, FIRMWARE_VERSION, 1);
  oled_text_center(44, "PY32F002A", 1);
  oled_flush();
  beep_twice();

  for (;;) {
    encoder_poll_button();
    handle_main_button();

    int16_t steps = encoder_take_steps();
    if (config.ec_reverse) steps = (int16_t)-steps;
    if (steps) {
      int16_t next = set_temp + steps * TEMP_STEP;
      if (next < TEMP_MIN) next = TEMP_MIN;
      if (next > TEMP_MAX) next = TEMP_MAX;
      set_temp = next;
      register_activity();
    }

    timer_service();

    if ((uint32_t)(g_ms - last_measure_ms) >= MEASURE_PERIOD_MS) {
      last_measure_ms = g_ms;
      sensor_service();
      thermostat();
    }

    if ((uint32_t)(g_ms - last_ui_ms) >= UI_PERIOD_MS) {
      last_ui_ms = g_ms;
      draw_main_screen();
    }

    LL_mDelay(0);
  }
}
