#include "adc.h"
#include "py32f0xx_ll_adc.h"
#include "py32f0xx_ll_bus.h"
#include "py32f0xx_ll_gpio.h"
#include "py32f0xx_ll_utils.h"

#define TIP_CHANNEL  LL_ADC_CHANNEL_4
#define VIN_CHANNEL  LL_ADC_CHANNEL_0
#define VIN_DIV_X10  57U

void adc_init(void)
{
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_ADC1);

  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_0, LL_GPIO_MODE_ANALOG);
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);

  LL_ADC_Reset(ADC1);
  if (LL_ADC_IsEnabled(ADC1) == 0) {
    LL_ADC_StartCalibration(ADC1);
    while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0) {
    }
    LL_mDelay(1);
  }

  LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
                                 LL_ADC_PATH_INTERNAL_VREFINT |
                                 LL_ADC_PATH_INTERNAL_TEMPSENSOR);
  LL_ADC_SetClock(ADC1, LL_ADC_CLOCK_SYNC_PCLK_DIV2);
  LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
  LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
  LL_ADC_SetLowPowerMode(ADC1, LL_ADC_LP_MODE_NONE);
  LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_239CYCLES_5);
  LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
  LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_CONTINUOUS);
  LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
  LL_ADC_REG_SetSequencerDiscont(ADC1, LL_ADC_REG_SEQ_DISCONT_DISABLE);
  LL_ADC_REG_SetSequencerChannels(ADC1, TIP_CHANNEL);
  LL_ADC_Enable(ADC1);
  LL_mDelay(1);
}

uint16_t adc_read_avg(uint32_t channel, uint8_t samples)
{
  uint32_t sum = 0;

  LL_ADC_Disable(ADC1);
  LL_ADC_REG_SetSequencerChannels(ADC1, channel);
  LL_ADC_ClearFlag_EOSMP(ADC1);
  LL_ADC_ClearFlag_EOC(ADC1);
  LL_ADC_ClearFlag_EOS(ADC1);
  LL_ADC_ClearFlag_OVR(ADC1);
  LL_ADC_Enable(ADC1);
  LL_mDelay(1);
  LL_ADC_REG_StartConversion(ADC1);

  for (uint8_t i = 0; i < samples; i++) {
    while (LL_ADC_IsActiveFlag_EOC(ADC1) == 0) {
    }
    LL_ADC_ClearFlag_EOC(ADC1);
    sum += LL_ADC_REG_ReadConversionData12(ADC1);
  }

  LL_ADC_Disable(ADC1);
  return (uint16_t)(sum / samples);
}

uint16_t adc_read_tip_raw(void)
{
  return adc_read_avg(TIP_CHANNEL, 8);
}

uint16_t adc_read_vin_raw(void)
{
  return adc_read_avg(VIN_CHANNEL, 8);
}

uint16_t adc_read_vref_raw(void)
{
  return adc_read_avg(LL_ADC_CHANNEL_VREFINT, 4);
}

uint16_t adc_read_temp_raw(void)
{
  return adc_read_avg(LL_ADC_CHANNEL_TEMPSENSOR, 8);
}

uint16_t adc_read_vdda_mv(void)
{
  uint32_t raw = adc_read_avg(LL_ADC_CHANNEL_VREFINT, 4);
  if (raw == 0) return 3300;
  return (uint16_t)((4095UL * VREFINT_CAL_VREF) / raw);
}

uint16_t adc_read_vin_mv(void)
{
  uint32_t adc = adc_read_avg(VIN_CHANNEL, 8);
  uint32_t vdda = adc_read_vdda_mv();
  return (uint16_t)((adc * vdda / 4095UL) * VIN_DIV_X10 / 10UL);
}

int16_t adc_read_chip_temp(void)
{
  uint32_t vdda = adc_read_vdda_mv();
  uint32_t raw = adc_read_avg(LL_ADC_CHANNEL_TEMPSENSOR, 8);
  return (int16_t)__LL_ADC_CALC_TEMPERATURE(vdda, raw, LL_ADC_RESOLUTION_12B);
}
