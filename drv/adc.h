#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>

#define ADC_TOTAL_CHANNELS  5
#define ADC_CLOCK_FREQ_HZ   200000

enum ADC_CHANNEL {
  ADC_CHANNEL_LJX = 0,
  ADC_CHANNEL_LJY = 1,
  ADC_CHANNEL_RJX = 2,
  ADC_CHANNEL_RJY = 3,
  ADC_CHANNEL_BAT = 4
};

#define ADC_REFERENCE_MV  3300
#define ADC_WIDE_BITS     10
#define ADC_SPAN          ((1 << ADC_WIDE_BITS) - 1)

void adc_init(uint32_t freq);
uint16_t adc_read(uint8_t ch);
uint16_t adc_raw(uint8_t ch);

#endif /* ADC_H_ */
