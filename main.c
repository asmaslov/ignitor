#include "config.h"
#include "trace.h"
#include "usart.h"
#include "meter.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

static volatile bool run;

const Usart usart0;

void traceOutchar(char c)
{
    usart_putchar((Usart *)&usart0, c);
}

void traceFlush(void)
{
    usart_flush((Usart *)&usart0);
}

void ignite(MeterSpark spark) {
    //TODO: Ignite
}

int main(void)
{
  cli();
  run = true;
  sei();
  usart_init((Usart *)&usart0, USART_0, DEBUG_BAUDRATE);
  trace_setup((OutcharFunc)traceOutchar, traceFlush);
  meter_init(ignite);

  TRACE_INFO("System ready");

  while(!run);
  while(run) {
  }
  return 0;
}
