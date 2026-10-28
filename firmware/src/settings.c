#include "settings.h"
#include <stddef.h>
#include <string.h>
#include "py32f0xx.h"
#include "py32f0xx_ll_flash.h"

#define SETTINGS_MAGIC   0x5A5AU
#define SETTINGS_VERSION 1U
#define SETTINGS_ADDR    (FLASH_BASE + 0x4F80UL)

static uint32_t settings_page[32] __attribute__((aligned(4)));
_Static_assert(sizeof(settings_t) <= sizeof(settings_page), "settings exceed flash page");

static uint16_t crc16(const uint8_t *data, uint32_t length)
{
  uint16_t crc = 0xFFFF;
  while (length--) {
    crc ^= (uint16_t)(*data++) << 8;
    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
      else crc = (uint16_t)(crc << 1);
    }
  }
  return crc;
}

void settings_defaults(settings_t *settings)
{
  memset(settings, 0, sizeof(*settings));
  settings->magic = SETTINGS_MAGIC;
  settings->version = SETTINGS_VERSION;
  settings->default_temp = 320;
  settings->sleep_temp = 150;
  settings->boost_temp = 50;
  settings->time2sleep = 5;
  settings->time2off = 15;
  settings->time_boost = 40;
  settings->main_screen = 0;
  settings->pid_enable = 0;
  settings->beep_enable = 1;
  settings->body_flip = 0;
  settings->ec_reverse = 0;
  settings->cal200 = 216;
  settings->cal280 = 308;
  settings->cal360 = 390;
  settings->chip_temp = 30;
  memcpy(settings->tip_name, "BC1.5", 6);
}

uint8_t settings_load(settings_t *settings)
{
  memcpy(settings, (const void *)SETTINGS_ADDR, sizeof(*settings));
  if (settings->magic != SETTINGS_MAGIC ||
      settings->version != SETTINGS_VERSION ||
      settings->crc != crc16((const uint8_t *)settings, offsetof(settings_t, crc))) {
    settings_defaults(settings);
    return 0;
  }
  return 1;
}

uint8_t settings_save(settings_t *settings)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t error = 0;

  settings->magic = SETTINGS_MAGIC;
  settings->version = SETTINGS_VERSION;
  settings->crc = crc16((const uint8_t *)settings, offsetof(settings_t, crc));

  memset(settings_page, 0xFF, sizeof(settings_page));
  memcpy(settings_page, settings, sizeof(*settings));

  LL_FLASH_Unlock();
  erase.TypeErase = FLASH_TYPEERASE_PAGEERASE;
  erase.PageAddress = SETTINGS_ADDR;
  erase.NbPages = 1;
  if (LL_FLASHEx_Erase(&erase, &error) != SUCCESS) {
    LL_FLASH_Lock();
    return 0;
  }
  if (LL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, SETTINGS_ADDR, settings_page) != SUCCESS) {
    LL_FLASH_Lock();
    return 0;
  }
  LL_FLASH_Lock();
  return 1;
}
