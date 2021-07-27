#include "adc.h"
#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include <stdbool.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

enum ADC_VREF {
  ADC_VREF_AREF = ((0 << REFS1) | (0 << REFS0)),
  ADC_VREF_AVCC = ((0 << REFS1) | (1 << REFS0)),
  ADC_VREF_1_1  = ((1 << REFS1) | (0 << REFS0)),
  ADC_VREF_2_56 = ((1 << REFS1) | (1 << REFS0))
};

enum ADC_TRIGGER {
  ADC_TRIGGER_FREE    = ((0 << ADTS2) | (0 << ADTS1) | (0 << ADTS0)),
  ADC_TRIGGER_CMP     = ((0 << ADTS2) | (0 << ADTS1) | (1 << ADTS0)),
  ADC_TRIGGER_EINT0   = ((0 << ADTS2) | (1 << ADTS1) | (0 << ADTS0)),
  ADC_TRIGGER_TC0COMA = ((0 << ADTS2) | (1 << ADTS1) | (1 << ADTS0)),
  ADC_TRIGGER_TC0OVR  = ((1 << ADTS2) | (0 << ADTS1) | (0 << ADTS0)),
  ADC_TRIGGER_TC1COMB = ((1 << ADTS2) | (0 << ADTS1) | (1 << ADTS0)),
  ADC_TRIGGER_TC1OVR  = ((1 << ADTS2) | (1 << ADTS1) | (0 << ADTS0)),
  ADC_TRIGGER_TC1CAP  = ((1 << ADTS2) | (1 << ADTS1) | (1 << ADTS0))
};

#define ADC_ADMUX  (ADC_VREF_AREF | (0 << ADLAR))

static const uint32_t prescaleAdc[] = {2, 2, 4, 8, 16, 32, 64, 128};
static const uint8_t dmAdc = 8;
static uint16_t raw[ADC_TOTAL_CHANNELS];
static volatile bool locked[ADC_TOTAL_CHANNELS];
static uint8_t channel;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

/****************************************************************************
 * Interrupt handler functions                                              *
 ****************************************************************************/

ISR(ADC_vect)
{
  if(!locked[channel])
  {
    locked[channel] = true;
    raw[channel] = ADCW;
    locked[channel] = false;
    if(++channel == ADC_TOTAL_CHANNELS)
    {
      channel = ADC_CHANNEL_LJX;
    }
    ADMUX = ADC_ADMUX | channel;
  }
  ADCSRA |= (1 << ADSC);
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void adc_init(uint32_t freq)
{
  uint8_t dv;
  uint32_t calc;
  int i;
  
  //ASSERT((freq <= (F_CPU / 2)), "ADC frequency too high\n\r");
  dv = 0;
  do {
    calc = F_CPU / prescaleAdc[dv++];
  } while((calc > freq) && (dv < dmAdc));
  if((calc > freq) && (dv == dmAdc))
  {
    //TRACE_ERROR("ADC frequency too low\n\r");
    return;
  }
  dv--;
  //TRACE_DEBUG("ADC divider set %lu\n\r", prescaleAdc[dv]);
  //TRACE_DEBUG("ADC frequency set %lu Hz\n\r", calc);
  channel = ADC_CHANNEL_LJX;
  ADMUX = ADC_ADMUX | channel;
  ADCSRA = (1 << ADEN) | (0 << ADSC) | (0 << ADATE) | (1 << ADIE) |
           (((dv >> 2) & 1) << ADPS2) | (((dv >> 1) & 1) << ADPS1) | (((dv >> 0) & 1) << ADPS0);
  ADCSRB = (0 << ACME) | ADC_TRIGGER_FREE;
  DIDR0 = (1 << ADC_CHANNEL_LJX) |
          (1 << ADC_CHANNEL_LJY) |
          (1 << ADC_CHANNEL_RJX) |
          (1 << ADC_CHANNEL_RJY) |
          (1 << ADC_CHANNEL_BAT);
  for(i = 0; i < ADC_TOTAL_CHANNELS; i++)
  {
    raw[i] = 0;
    locked[i] = false;
  }
  ADCSRA |= (1 << ADSC);
}

uint16_t adc_read(uint8_t ch)
{
  uint32_t value;
  
  if(ch < ADC_TOTAL_CHANNELS)
  {
    while(locked[ch]);
    value = raw[ch];
    value *= ADC_REFERENCE_MV;
    value /= ADC_SPAN;
    return (uint16_t)value;
  }
  return UINT16_MAX;
}

uint16_t adc_raw(uint8_t ch)
{
  if(ch < ADC_TOTAL_CHANNELS)
  {
    while(locked[ch]);
    return raw[ch];
  }
  return UINT16_MAX;
}
