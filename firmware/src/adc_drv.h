#ifndef ADC_DRV_H
#define ADC_DRV_H

#include <stdint.h>

void adc_drv_init(void);
uint16_t adc_drv_read(uint32_t channel, uint8_t samples);
uint16_t adc_tip_raw(void);
uint16_t adc_vin_raw(void);
uint16_t adc_vref_raw(void);
uint16_t adc_ts_raw(void);
uint16_t adc_vin_mv(void);
uint16_t adc_vdda_mv(void);
int16_t adc_die_temp(void);

#endif
