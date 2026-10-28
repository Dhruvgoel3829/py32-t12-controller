#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void adc_init(void);
uint16_t adc_read_avg(uint32_t channel, uint8_t samples);
uint16_t adc_read_tip_raw(void);
uint16_t adc_read_vin_raw(void);
uint16_t adc_read_vref_raw(void);
uint16_t adc_read_temp_raw(void);
uint16_t adc_read_vin_mv(void);
int16_t adc_read_chip_temp(void);
uint16_t adc_read_vdda_mv(void);

#endif
