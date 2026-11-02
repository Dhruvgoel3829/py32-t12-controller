#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

typedef struct {
  uint16_t signature;
  uint8_t revision;
  uint8_t reserved;
  uint16_t normal_temp;
  uint16_t standby_temp;
  uint8_t boost_delta;
  uint8_t standby_minutes;
  uint8_t shutdown_minutes;
  uint8_t boost_seconds;
  uint8_t reserved2;
  uint8_t pid_mode;
  uint8_t buzzer;
  uint8_t rotate_display;
  uint8_t reverse_encoder;
  uint16_t cal_low;
  uint16_t cal_mid;
  uint16_t cal_high;
  uint16_t die_temp;
  char tip_label[8];
  uint16_t checksum;
} config_t;

void config_defaults(config_t *config);
uint8_t config_load(config_t *config);
uint8_t config_store(config_t *config);

#endif
