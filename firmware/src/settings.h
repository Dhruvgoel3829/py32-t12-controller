#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

typedef struct {
  uint16_t magic;
  uint8_t version;
  uint8_t flags;
  uint16_t default_temp;
  uint16_t sleep_temp;
  uint8_t boost_temp;
  uint8_t time2sleep;
  uint8_t time2off;
  uint8_t time_boost;
  uint8_t main_screen;
  uint8_t pid_enable;
  uint8_t beep_enable;
  uint8_t body_flip;
  uint8_t ec_reverse;
  uint16_t cal200;
  uint16_t cal280;
  uint16_t cal360;
  uint16_t chip_temp;
  char tip_name[8];
  uint16_t crc;
} settings_t;

void settings_defaults(settings_t *settings);
uint8_t settings_load(settings_t *settings);
uint8_t settings_save(settings_t *settings);

#endif
