#include "config.h"
#include "timer.h"
#include "usart.h"
#include "meter.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

static volatile bool run;

Timer timer2;
Usart usart0;

void traceOutchar(char c)
{
    usart_putchar(&usart0, c);
}

void traceFlush(void)
{
    usart_flush(&usart0);
}

void ignite(MeterSpark spark) {
    //TODO: Ignite
}

int main(void)
{
  cli();
  run = true;
  sei();
  usart_init(&usart0, USART_0, DEBUG_BAUDRATE);
  usart_putstr(&usart0, "System ready");
  //timer_configSimple(&timer2, TIMER_2, WD_RESET_FREQ_HZ, NULL, TIMER_OUTPUT_TOGGLE_B);
  //meter_init(ignite);

  while(!run);
  while(run) {
  }
  return 0;
}
