#include <stdint.h>
#include <string.h>
#include "py32f0xx.h"
#include "py32f0xx_ll_bus.h"
#include "py32f0xx_ll_gpio.h"
#include "py32f0xx_ll_rcc.h"
#include "py32f0xx_ll_system.h"
#include "py32f0xx_ll_tim.h"
#include "py32f0xx_ll_utils.h"
#include "adc_drv.h"
#include "board.h"
#include "display.h"
#include "input.h"
#include "settings.h"

#define FW_LABEL             "V1.0-PY32"
#define TEMP_LOW_LIMIT       150
#define TEMP_HIGH_LIMIT      400
#define TEMP_ADJUST_STEP     10
#define STANDBY_TEMP_DEFAULT 150
#define BOOST_DELTA_DEFAULT  50
#define TEMP_FAULT_LIMIT     500
#define TEMP_CEILING         450
#define STANDBY_MIN          20
#define STANDBY_MAX          200
#define BOOST_MIN            10
#define BOOST_MAX            100
#define STANDBY_TIME_DEFAULT 5
#define SHUTDOWN_TIME_DEFAULT 15
#define BOOST_TIME_DEFAULT   40
#define LONG_PRESS_MS        500
#define SENSOR_PERIOD_MS     50
#define RENDER_PERIOD_MS     100

volatile uint32_t system_ticks;

static config_t cfg;
static int16_t target_temp;
static int16_t tip_temp;
static int16_t shown_temp;
static int16_t control_target;
static int16_t pid_prev_temp;
static int32_t tip_avg_x100;
static int32_t pid_accum;
static uint8_t heater_duty;
static uint8_t tip_ok;
static uint8_t at_temp;
static uint8_t beep_when_ready;
static uint8_t standby_mode;
static uint8_t shutdown_mode;
static uint8_t boost_mode;
static uint8_t config_dirty;
static uint8_t button_latched;
static uint8_t reed_now;
static uint8_t vin_counter;
static uint16_t vin_voltage;
static uint32_t activity_ms;
static uint32_t boost_start_ms;
static uint32_t last_sensor_ms;
static uint32_t last_render_ms;
static uint32_t last_pid_ms;
static uint32_t press_start_ms;

static int16_t abs_i16(int16_t value)
{
  return value < 0 ? (int16_t)-value : value;
}

static void spin_us(uint32_t us)
{
  uint32_t count = us * 6U;
  while (count--) __NOP();
}

static void spin_ms(uint32_t ms)
{
  uint32_t start = system_ticks;
  while ((uint32_t)(system_ticks - start) < ms) {
  }
}

static void format_u16(uint16_t value, char *text)
{
  char reversed[6];
  uint8_t length = 0;

  if (value == 0) {
    text[0] = '0';
    text[1] = 0;
    return;
  }
  while (value) {
    reversed[length++] = (char)('0' + value % 10);
    value /= 10;
  }
  for (uint8_t i = 0; i < length; i++) text[i] = reversed[length - 1 - i];
  text[length] = 0;
}

static void format_volts(uint16_t millivolts, char *text)
{
  uint32_t tenths = ((uint32_t)millivolts + 50U) / 100U;
  uint8_t position = 0;

  if (tenths >= 100U) text[position++] = (char)('0' + (tenths / 100U) % 10U);
  text[position++] = (char)('0' + (tenths / 10U) % 10U);
  text[position++] = '.';
  text[position++] = (char)('0' + tenths % 10U);
  text[position++] = 'V';
  text[position] = 0;
}

static void clock_setup(void)
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

static void buzzer_setup(void)
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
  if (!cfg.buzzer) return;

  for (uint8_t i = 0; i < 220; i++) {
    LL_GPIO_SetOutputPin(BUZZER_PORT, BUZZER_PIN);
    spin_us(125);
    LL_GPIO_ResetOutputPin(BUZZER_PORT, BUZZER_PIN);
    spin_us(125);
  }
}

static void beep_twice(void)
{
  beep();
  spin_ms(80);
  beep();
}

static void heater_write(uint8_t duty)
{
  uint32_t compare = HEATER_INVERTED ? (256UL - duty) : duty;
  LL_TIM_OC_SetCompareCH4(TIM1, compare);
}

static void heater_setup(void)
{
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM1);

  LL_GPIO_InitTypeDef gpio = {0};
  gpio.Pin = HEATER_PIN;
  gpio.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio.Alternate = LL_GPIO_AF_13;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Pull = LL_GPIO_PULL_NO;
  gpio.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  LL_GPIO_Init(HEATER_PORT, &gpio);

  LL_TIM_InitTypeDef timer = {0};
  timer.Prescaler = (SystemCoreClock / 256000UL) - 1;
  timer.CounterMode = LL_TIM_COUNTERMODE_UP;
  timer.Autoreload = 255;
  timer.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  timer.RepetitionCounter = 0;
  LL_TIM_Init(TIM1, &timer);

  LL_TIM_OC_InitTypeDef channel = {0};
  channel.OCMode = LL_TIM_OCMODE_PWM1;
  channel.OCState = LL_TIM_OCSTATE_ENABLE;
  channel.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
  channel.OCIdleState = LL_TIM_OCIDLESTATE_LOW;
  channel.OCNState = LL_TIM_OCSTATE_DISABLE;
  channel.OCNPolarity = LL_TIM_OCPOLARITY_HIGH;
  channel.OCNIdleState = LL_TIM_OCIDLESTATE_LOW;
  channel.CompareValue = 256;
  LL_TIM_OC_Init(TIM1, LL_TIM_CHANNEL_CH4, &channel);
  LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH4);

  LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH4);
  LL_TIM_EnableAllOutputs(TIM1);
  LL_TIM_EnableCounter(TIM1);
  heater_write(0);
}

static void reed_setup(void)
{
  LL_GPIO_InitTypeDef gpio = {0};
  gpio.Pin = REED_PIN;
  gpio.Mode = LL_GPIO_MODE_INPUT;
  gpio.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(REED_PORT, &gpio);
  reed_now = LL_GPIO_IsInputPinSet(REED_PORT, REED_PIN) ? 1 : 0;
}

static uint8_t reed_read(void)
{
  return LL_GPIO_IsInputPinSet(REED_PORT, REED_PIN) ? 1 : 0;
}

static int16_t raw_to_celsius(int16_t raw)
{
  int32_t value;

  if (raw < 0) raw = 0;
  if (raw < 200) {
    value = 21 + ((int32_t)raw * (cfg.cal_low - 21)) / 200;
  } else if (raw < 280) {
    value = cfg.cal_low + ((int32_t)(raw - 200) * (cfg.cal_mid - cfg.cal_low)) / 80;
  } else {
    value = cfg.cal_mid + ((int32_t)(raw - 280) * (cfg.cal_high - cfg.cal_mid)) / 80;
  }

  if (value < -50) value = -50;
  if (value > 999) value = 999;
  return (int16_t)value;
}

static int16_t sample_tip(void)
{
  heater_write(0);
  spin_us(950);
  uint16_t raw = adc_tip_raw();
  heater_write(heater_duty);
  return (int16_t)(raw >> 2);
}

static void update_tip_temp(int16_t raw)
{
  int32_t scaled = (int32_t)raw * 100;
  tip_avg_x100 += ((scaled - tip_avg_x100) * 5) / 100;
  tip_temp = raw_to_celsius((int16_t)((tip_avg_x100 + 50) / 100));
}

static void sanitize_config(void)
{
  if (cfg.normal_temp < TEMP_LOW_LIMIT || cfg.normal_temp > TEMP_HIGH_LIMIT)
    cfg.normal_temp = 320;
  if (cfg.standby_temp < STANDBY_MIN || cfg.standby_temp > STANDBY_MAX)
    cfg.standby_temp = STANDBY_TEMP_DEFAULT;
  if (cfg.boost_delta < BOOST_MIN || cfg.boost_delta > BOOST_MAX)
    cfg.boost_delta = BOOST_DELTA_DEFAULT;
  if (cfg.standby_minutes > 30) cfg.standby_minutes = STANDBY_TIME_DEFAULT;
  if (cfg.shutdown_minutes > 60) cfg.shutdown_minutes = SHUTDOWN_TIME_DEFAULT;
  if (cfg.boost_seconds > 180) cfg.boost_seconds = BOOST_TIME_DEFAULT;
  if (cfg.cal_low < 50 || cfg.cal_mid <= cfg.cal_low || cfg.cal_high <= cfg.cal_mid) {
    cfg.cal_low = 216;
    cfg.cal_mid = 308;
    cfg.cal_high = 390;
  }
  if (cfg.tip_label[0] == 0 || cfg.tip_label[0] == (char)0xFF) {
    memcpy(cfg.tip_label, "BC1.5", 6);
  }
  cfg.tip_label[7] = 0;
}

static void refresh_control_target(void)
{
  if (shutdown_mode) control_target = 0;
  else if (standby_mode) control_target = cfg.standby_temp;
  else if (boost_mode) {
    int16_t boosted = target_temp + cfg.boost_delta;
    if (boosted > TEMP_CEILING) boosted = TEMP_CEILING;
    control_target = boosted;
  } else {
    control_target = target_temp;
  }
}

static void note_activity(void)
{
  if (standby_mode || shutdown_mode) {
    standby_mode = 0;
    shutdown_mode = 0;
    boost_mode = 0;
    beep_when_ready = 1;
    beep();
  }
  activity_ms = system_ticks;
}

static void service_timers(void)
{
  uint32_t now = system_ticks;
  uint32_t idle_minutes = (now - activity_ms) / 60000UL;

  if (!standby_mode && cfg.standby_minutes > 0 && idle_minutes >= cfg.standby_minutes) {
    standby_mode = 1;
    beep();
  }
  if (!shutdown_mode && cfg.shutdown_minutes > 0 && idle_minutes >= cfg.shutdown_minutes) {
    shutdown_mode = 1;
    beep();
  }
  if (boost_mode &&
      (uint32_t)(now - boost_start_ms) >= (uint32_t)cfg.boost_seconds * 1000UL) {
    boost_mode = 0;
    beep_when_ready = 1;
    beep();
  }
}

static void service_sensor(void)
{
  refresh_control_target();

  int16_t raw = sample_tip();
  update_tip_temp(raw);

  if (++vin_counter >= 40) {
    vin_counter = 0;
    vin_voltage = adc_vin_mv();
  }

  uint8_t reed = reed_read();
  if (reed != reed_now) {
    reed_now = reed;
    note_activity();
  }

  if ((shown_temp != control_target) || (abs_i16((int16_t)(shown_temp - tip_temp)) > 5))
    shown_temp = tip_temp;
  if (abs_i16((int16_t)(shown_temp - control_target)) <= 1) shown_temp = control_target;

  if (shown_temp > TEMP_FAULT_LIMIT) tip_ok = 0;
  if (!tip_ok && shown_temp < TEMP_FAULT_LIMIT) {
    tip_ok = 1;
    heater_duty = 0;
    heater_write(0);
    tip_avg_x100 = (int32_t)raw * 100;
    pid_accum = 0;
    pid_prev_temp = tip_temp;
    beep();
    note_activity();
  }

  int16_t gap = abs_i16((int16_t)(target_temp - tip_temp));
  if (gap < 5) {
    if (!at_temp && beep_when_ready) beep();
    at_temp = 1;
    beep_when_ready = 0;
  } else {
    at_temp = 0;
  }
}

static void service_control(void)
{
  refresh_control_target();

  if (!tip_ok || shutdown_mode || shown_temp > TEMP_FAULT_LIMIT || tip_temp > TEMP_CEILING) {
    heater_duty = 0;
    pid_accum = 0;
    heater_write(0);
    return;
  }

  if (cfg.pid_mode) {
    uint32_t now = system_ticks;
    if ((uint32_t)(now - last_pid_ms) >= 100) {
      int16_t error = control_target - tip_temp;
      int16_t gap = abs_i16(error);
      int32_t kp;
      int32_t ki;
      int32_t kd;

      if (gap < 30) {
        kp = 110;
        ki = 3;
        kd = 500;
      } else {
        kp = 110;
        ki = 5;
        kd = 100;
      }

      pid_accum += ki * error;
      if (pid_accum > 2550) pid_accum = 2550;
      if (pid_accum < 0) pid_accum = 0;

      int32_t derivative = tip_temp - pid_prev_temp;
      int32_t output = (kp * error + pid_accum - kd * derivative) / 10;
      if (output < 0) output = 0;
      if (output > 255) output = 255;

      heater_duty = (uint8_t)output;
      pid_prev_temp = tip_temp;
      last_pid_ms = now;
    }
  } else {
    heater_duty = (tip_temp + 1 < control_target) ? 255 : 0;
    pid_accum = 0;
  }

  heater_write(heater_duty);
}

static const char *status_label(void)
{
  if (!tip_ok || shown_temp > TEMP_FAULT_LIMIT) return "ERROR";
  if (shutdown_mode) return "OFF";
  if (standby_mode) return "SLEEP";
  if (boost_mode) return "BOOST";
  if (at_temp) return "WORKY";
  if (heater_duty > 75) return "HEAT";
  return "HOLD";
}

static void render_main(void)
{
  char text[16];

  display_clear();
  display_text(0, 0, "SET:", 1);
  format_u16((uint16_t)target_temp, text);
  display_text(24, 0, text, 1);
  display_text(84, 0, status_label(), 1);

  if (shown_temp > TEMP_FAULT_LIMIT) {
    display_text_center(22, "---", 3);
  } else {
    format_u16((uint16_t)shown_temp, text);
    display_text_center(22, text, 3);
  }

  display_text(0, 56, cfg.tip_label, 1);

  format_volts(vin_voltage, text);
  display_text(92, 56, text, 1);

  display_refresh();
}

static void render_list(const char *title, const char *const *items,
                        uint8_t count, uint8_t selected)
{
  uint8_t first = selected > 3 ? (uint8_t)(selected - 3) : 0;

  display_clear();
  display_text(0, 0, title, 1);
  for (uint8_t row = 0; row < 4; row++) {
    uint8_t index = first + row;
    if (index >= count) break;
    uint8_t y = (uint8_t)(12 + row * 12);
    if (index == selected) display_text(0, y, ">", 1);
    display_text(8, y, items[index], 1);
  }
  display_refresh();
}

static void flush_input_events(void)
{
  input_take_press();
  input_take_release();
}

static uint8_t pick_list(const char *title, const char *const *items,
                         uint8_t count, uint8_t selected)
{
  uint8_t drawn = 0xFF;

  flush_input_events();
  for (;;) {
    input_poll();
    int16_t steps = input_take_steps();
    if (cfg.reverse_encoder) steps = (int16_t)-steps;
    if (steps) {
      int16_t next = (int16_t)selected + steps;
      while (next < 0) next += count;
      while (next >= count) next -= count;
      selected = (uint8_t)next;
    }

    if (selected != drawn) {
      render_list(title, items, count, selected);
      drawn = selected;
    }

    if (input_take_press()) {
      while (input_is_down()) {
        input_poll();
        LL_mDelay(2);
      }
      flush_input_events();
      beep();
      return selected;
    }
    LL_mDelay(2);
  }
}

static uint16_t pick_value(const char *title, const char *unit, uint16_t value,
                           uint16_t minimum, uint16_t maximum, uint16_t step)
{
  char text[8];
  uint16_t drawn = 0xFFFF;

  flush_input_events();
  for (;;) {
    input_poll();
    int16_t steps = input_take_steps();
    if (cfg.reverse_encoder) steps = (int16_t)-steps;
    if (steps) {
      int32_t next = (int32_t)value + (int32_t)steps * step;
      if (next < minimum) next = minimum;
      if (next > maximum) next = maximum;
      value = (uint16_t)next;
    }

    if (value != drawn) {
      display_clear();
      display_text(0, 0, title, 1);
      if (value == 0) strcpy(text, "OFF");
      else format_u16(value, text);
      display_text_center(20, text, 2);
      display_text(0, 40, unit, 1);
      display_text(0, 54, "PRESS TO SET", 1);
      display_refresh();
      drawn = value;
    }

    if (input_take_press()) {
      while (input_is_down()) {
        input_poll();
        LL_mDelay(2);
      }
      flush_input_events();
      beep();
      return value;
    }
    LL_mDelay(2);
  }
}

static uint8_t pick_option(const char *title, uint8_t value)
{
  static const char *const items[] = { "DISABLE", "ENABLE" };
  return pick_list(title, items, 2, value ? 1 : 0);
}

static uint8_t pick_control(uint8_t value)
{
  static const char *const items[] = { "DIRECT", "PID" };
  return pick_list("CONTROL TYPE", items, 2, value ? 1 : 0);
}

static void show_notice(const char *line1, const char *line2)
{
  display_clear();
  display_text(0, 8, line1, 1);
  display_text(0, 24, line2, 1);
  display_text(0, 54, "PRESS TO CONTINUE", 1);
  display_refresh();

  flush_input_events();
  for (;;) {
    input_poll();
    if (input_take_press()) {
      while (input_is_down()) {
        input_poll();
        LL_mDelay(2);
      }
      flush_input_events();
      beep();
      return;
    }
    LL_mDelay(2);
  }
}

static void adc_monitor(void)
{
  char text[8];
  uint32_t last = 0;

  heater_duty = 0;
  heater_write(0);
  flush_input_events();

  for (;;) {
    input_poll();
    input_take_steps();

    if ((uint32_t)(system_ticks - last) >= 250) {
      last = system_ticks;
      uint16_t tip = adc_tip_raw();
      uint16_t vin = adc_vin_raw();
      uint16_t vref = adc_vref_raw();
      uint16_t temperature = adc_ts_raw();
      heater_write(0);

      display_clear();
      display_text(0, 0, "ADC RAW  HEATER OFF", 1);
      display_text(0, 12, "PA4 TIP", 1);
      format_u16(tip, text);
      display_text(72, 12, text, 1);
      display_text(0, 22, "PA0 VIN", 1);
      format_u16(vin, text);
      display_text(72, 22, text, 1);
      display_text(0, 32, "VREF", 1);
      format_u16(vref, text);
      display_text(72, 32, text, 1);
      display_text(0, 42, "TEMP SENS", 1);
      format_u16(temperature, text);
      display_text(72, 42, text, 1);
      display_text(0, 54, "PRESS TO EXIT", 1);
      display_refresh();
    }

    if (input_take_press()) {
      while (input_is_down()) {
        input_poll();
        LL_mDelay(2);
      }
      flush_input_events();
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

  flush_input_events();
  for (;;) {
    input_poll();
    input_take_steps();

    if (!drawn) {
      int16_t die = adc_die_temp();
      uint16_t supply = adc_vdda_mv();
      uint16_t vin = adc_vin_mv();

      display_clear();
      display_text(0, 0, "INFORMATION", 1);
      display_text(0, 12, "FW:", 1);
      display_text(24, 12, FW_LABEL, 1);

      display_text(0, 24, "CHIP:", 1);
      format_u16((uint16_t)(die < 0 ? 0 : die), text);
      display_text(42, 24, text, 1);
      display_text(60, 24, "C", 1);

      display_text(0, 36, "VIN:", 1);
      format_volts(vin, text);
      display_text(42, 36, text, 1);

      display_text(0, 48, "VCC:", 1);
      format_volts(supply, text);
      display_text(42, 48, text, 1);

      display_refresh();
      drawn = 1;
    }

    if (input_take_press()) {
      while (input_is_down()) {
        input_poll();
        LL_mDelay(2);
      }
      flush_input_events();
      beep();
      return;
    }
    LL_mDelay(10);
  }
}

static void temp_menu(void)
{
  static const char *const items[] = { "DEFAULT TEMP", "SLEEP TEMP", "BOOST TEMP", "RETURN" };
  uint8_t selected = 0;

  for (;;) {
    selected = pick_list("TEMP SETTINGS", items, 4, selected);
    if (selected == 0) {
      uint16_t value = pick_value("DEFAULT TEMP", "CELSIUS", cfg.normal_temp,
                                  TEMP_LOW_LIMIT, TEMP_HIGH_LIMIT, TEMP_ADJUST_STEP);
      if (value != cfg.normal_temp) {
        cfg.normal_temp = value;
        config_dirty = 1;
      }
    } else if (selected == 1) {
      uint16_t value = pick_value("SLEEP TEMP", "CELSIUS", cfg.standby_temp,
                                  STANDBY_MIN, STANDBY_MAX, TEMP_ADJUST_STEP);
      if (value != cfg.standby_temp) {
        cfg.standby_temp = value;
        config_dirty = 1;
      }
    } else if (selected == 2) {
      uint16_t value = pick_value("BOOST TEMP", "CELSIUS", cfg.boost_delta,
                                  BOOST_MIN, BOOST_MAX, TEMP_ADJUST_STEP);
      if (value != cfg.boost_delta) {
        cfg.boost_delta = (uint8_t)value;
        config_dirty = 1;
      }
    } else {
      return;
    }
  }
}

static void timer_menu(void)
{
  static const char *const items[] = { "SLEEP TIMER", "OFF TIMER", "BOOST TIMER", "RETURN" };
  uint8_t selected = 0;

  for (;;) {
    selected = pick_list("TIMER SETTINGS", items, 4, selected);
    if (selected == 0) {
      uint16_t value = pick_value("SLEEP TIMER", "MINUTES", cfg.standby_minutes, 0, 30, 1);
      if (value != cfg.standby_minutes) {
        cfg.standby_minutes = (uint8_t)value;
        config_dirty = 1;
      }
    } else if (selected == 1) {
      uint16_t value = pick_value("OFF TIMER", "MINUTES", cfg.shutdown_minutes, 0, 60, 5);
      if (value != cfg.shutdown_minutes) {
        cfg.shutdown_minutes = (uint8_t)value;
        config_dirty = 1;
      }
    } else if (selected == 2) {
      uint16_t value = pick_value("BOOST TIMER", "SECONDS", cfg.boost_seconds, 0, 180, 10);
      if (value != cfg.boost_seconds) {
        cfg.boost_seconds = (uint8_t)value;
        config_dirty = 1;
      }
    } else {
      return;
    }
  }
}

static void calibration(void)
{
  uint16_t measured[3];
  uint16_t target[3] = { cfg.cal_low, cfg.cal_mid, cfg.cal_high };

  heater_duty = 255;
  heater_write(255);

  for (uint8_t step = 0; step < 3; step++) {
    uint16_t value = target[step];
    uint32_t last_measure = 0;
    char text[8];

    flush_input_events();
    for (;;) {
      input_poll();
      int16_t steps = input_take_steps();
      if (cfg.reverse_encoder) steps = (int16_t)-steps;
      if (steps) {
        int32_t next = (int32_t)value + (int32_t)steps;
        if (next < 100) next = 100;
        if (next > 500) next = 500;
        value = (uint16_t)next;
      }

      if ((uint32_t)(system_ticks - last_measure) >= SENSOR_PERIOD_MS) {
        last_measure = system_ticks;
        int16_t raw = sample_tip();
        update_tip_temp(raw);
        heater_duty = (tip_temp + 1 < (int16_t)target[step]) ? 255 : 0;
        heater_write(heater_duty);
      }

      display_clear();
      display_text(0, 0, "CALIBRATION", 1);
      display_text(0, 10, "STEP", 1);
      text[0] = (char)('1' + step);
      text[1] = '/';
      text[2] = '3';
      text[3] = 0;
      display_text(30, 10, text, 1);
      display_text(0, 20, "SET", 1);
      format_u16(target[step], text);
      display_text(30, 20, text, 1);
      display_text(0, 30, "ACT", 1);
      format_u16((uint16_t)(tip_temp < 0 ? 0 : tip_temp), text);
      display_text(30, 30, text, 1);
      display_text(0, 40, "MEAS", 1);
      format_u16(value, text);
      display_text(36, 40, text, 1);
      display_text(0, 54, "CLICK WHEN READY", 1);
      display_refresh();

      if (input_take_press()) {
        while (input_is_down()) {
          input_poll();
          LL_mDelay(2);
        }
        flush_input_events();
        break;
      }
      LL_mDelay(5);
    }
    measured[step] = value;
    beep();
  }

  heater_duty = 0;
  heater_write(0);

  if (measured[0] + 10 < measured[1] && measured[1] + 10 < measured[2]) {
    cfg.cal_low = measured[0];
    cfg.cal_mid = measured[1];
    cfg.cal_high = measured[2];
    config_dirty = 1;
    show_notice("CALIBRATION", "SAVED");
  } else {
    show_notice("CALIBRATION", "FAILED");
  }
}

static void setup_menu(void)
{
  static const char *const items[] = {
    "TEMPERATURE", "TIMERS", "CONTROL", "BUZZER",
    "SCREEN FLIP", "EC REVERSE", "CALIBRATE", "INFORMATION",
    "ADC MONITOR", "SAVE & EXIT"
  };
  uint8_t selected = 0;
  uint8_t previous_duty = heater_duty;
  uint16_t previous_target = target_temp;

  heater_duty = 0;
  heater_write(0);
  boost_mode = 0;
  standby_mode = 0;
  shutdown_mode = 0;
  beep();

  for (;;) {
    selected = pick_list("SETUP MENU", items, 10, selected);
    if (selected == 0) {
      temp_menu();
    } else if (selected == 1) {
      timer_menu();
    } else if (selected == 2) {
      uint8_t value = pick_control(cfg.pid_mode);
      if (value != cfg.pid_mode) {
        cfg.pid_mode = value;
        config_dirty = 1;
      }
    } else if (selected == 3) {
      uint8_t value = pick_option("BUZZER", cfg.buzzer);
      if (value != cfg.buzzer) {
        cfg.buzzer = value;
        config_dirty = 1;
      }
    } else if (selected == 4) {
      uint8_t value = pick_option("SCREEN FLIP", cfg.rotate_display);
      if (value != cfg.rotate_display) {
        cfg.rotate_display = value;
        config_dirty = 1;
        display_flip(cfg.rotate_display);
      }
    } else if (selected == 5) {
      uint8_t value = pick_option("EC REVERSE", cfg.reverse_encoder);
      if (value != cfg.reverse_encoder) {
        cfg.reverse_encoder = value;
        config_dirty = 1;
      }
    } else if (selected == 6) {
      calibration();
    } else if (selected == 7) {
      info_screen();
    } else if (selected == 8) {
      adc_monitor();
    } else {
      break;
    }
  }

  if (config_dirty) {
    config_store(&cfg);
    config_dirty = 0;
  }
  target_temp = previous_target;
  heater_duty = previous_duty;
  note_activity();
  beep();
}

static void service_button(void)
{
  if (input_take_press()) {
    press_start_ms = system_ticks;
    button_latched = 1;
  }

  if (input_take_release()) {
    if (button_latched) {
      uint32_t duration = system_ticks - press_start_ms;
      button_latched = 0;
      if (duration >= LONG_PRESS_MS) {
        setup_menu();
        flush_input_events();
      } else {
        boost_mode = !boost_mode;
        if (boost_mode) {
          boost_start_ms = system_ticks;
          beep();
          beep_when_ready = 1;
        } else {
          beep();
        }
        note_activity();
      }
      last_render_ms = 0;
    }
  }

  if (button_latched && input_is_down() &&
      (uint32_t)(system_ticks - press_start_ms) >= LONG_PRESS_MS) {
    button_latched = 0;
    input_take_release();
    setup_menu();
    flush_input_events();
    last_render_ms = 0;
  }
}

int main(void)
{
  clock_setup();
  buzzer_setup();
  heater_setup();
  reed_setup();
  adc_drv_init();
  input_init();
  display_init();

  system_ticks = 0;

  if (!config_load(&cfg)) {
    config_store(&cfg);
  }
  sanitize_config();

  display_flip(cfg.rotate_display);
  target_temp = cfg.normal_temp;
  beep_when_ready = 1;

  tip_avg_x100 = (int32_t)sample_tip() * 100;
  tip_temp = raw_to_celsius((int16_t)(tip_avg_x100 / 100));
  shown_temp = tip_temp;
  tip_ok = shown_temp < TEMP_FAULT_LIMIT;
  pid_prev_temp = tip_temp;
  vin_voltage = adc_vin_mv();
  activity_ms = 0;
  last_sensor_ms = 0;
  last_render_ms = 0;
  last_pid_ms = 0;

  refresh_control_target();
  heater_duty = (tip_ok && tip_temp + 20 < control_target) ? 255 : 0;
  heater_write(heater_duty);

  display_clear();
  display_text_center(8, "T12 STATION", 1);
  display_text_center(24, FW_LABEL, 1);
  display_text_center(44, "PY32F002A", 1);
  display_refresh();
  beep_twice();

  for (;;) {
    input_poll();
    service_button();

    int16_t steps = input_take_steps();
    if (cfg.reverse_encoder) steps = (int16_t)-steps;
    if (steps) {
      int16_t next = target_temp + steps * TEMP_ADJUST_STEP;
      if (next < TEMP_LOW_LIMIT) next = TEMP_LOW_LIMIT;
      if (next > TEMP_HIGH_LIMIT) next = TEMP_HIGH_LIMIT;
      target_temp = next;
      note_activity();
    }

    service_timers();

    if ((uint32_t)(system_ticks - last_sensor_ms) >= SENSOR_PERIOD_MS) {
      last_sensor_ms = system_ticks;
      service_sensor();
      service_control();
    }

    if ((uint32_t)(system_ticks - last_render_ms) >= RENDER_PERIOD_MS) {
      last_render_ms = system_ticks;
      render_main();
    }

    LL_mDelay(0);
  }
}
