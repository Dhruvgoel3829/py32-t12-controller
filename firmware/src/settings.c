#include "settings.h"
#include <stddef.h>
#include <string.h>
#include "py32f0xx.h"
#include "py32f0xx_ll_flash.h"

#define CONFIG_SIGNATURE  0x5A5AU
#define CONFIG_REVISION   1U
#define CONFIG_FLASH_ADDR (FLASH_BASE + 0x4F80UL)

static uint32_t config_page[32] __attribute__((aligned(4)));
_Static_assert(sizeof(config_t) <= sizeof(config_page), "config exceeds flash page");

static uint16_t config_sum(const uint8_t *data, uint32_t length)
{
  uint16_t sum = 0xFFFF;
  while (length--) {
    sum ^= (uint16_t)(*data++) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (sum & 0x8000) sum = (uint16_t)((sum << 1) ^ 0x1021);
      else sum = (uint16_t)(sum << 1);
    }
  }
  return sum;
}

void config_defaults(config_t *config)
{
  memset(config, 0, sizeof(*config));
  config->signature = CONFIG_SIGNATURE;
  config->revision = CONFIG_REVISION;
  config->normal_temp = 320;
  config->standby_temp = 150;
  config->boost_delta = 50;
  config->standby_minutes = 5;
  config->shutdown_minutes = 15;
  config->boost_seconds = 40;
  config->pid_mode = 0;
  config->buzzer = 1;
  config->rotate_display = 0;
  config->reverse_encoder = 0;
  config->cal_low = 216;
  config->cal_mid = 308;
  config->cal_high = 390;
  config->die_temp = 30;
  memcpy(config->tip_label, "BC1.5", 6);
}

uint8_t config_load(config_t *config)
{
  memcpy(config, (const void *)CONFIG_FLASH_ADDR, sizeof(*config));
  if (config->signature != CONFIG_SIGNATURE ||
      config->revision != CONFIG_REVISION ||
      config->checksum != config_sum((const uint8_t *)config, offsetof(config_t, checksum))) {
    config_defaults(config);
    return 0;
  }
  return 1;
}

uint8_t config_store(config_t *config)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t error = 0;

  config->signature = CONFIG_SIGNATURE;
  config->revision = CONFIG_REVISION;
  config->checksum = config_sum((const uint8_t *)config, offsetof(config_t, checksum));

  memset(config_page, 0xFF, sizeof(config_page));
  memcpy(config_page, config, sizeof(*config));

  LL_FLASH_Unlock();
  erase.TypeErase = FLASH_TYPEERASE_PAGEERASE;
  erase.PageAddress = CONFIG_FLASH_ADDR;
  erase.NbPages = 1;
  if (LL_FLASHEx_Erase(&erase, &error) != SUCCESS) {
    LL_FLASH_Lock();
    return 0;
  }
  if (LL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, CONFIG_FLASH_ADDR, config_page) != SUCCESS) {
    LL_FLASH_Lock();
    return 0;
  }
  LL_FLASH_Lock();
  return 1;
}
