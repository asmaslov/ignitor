#include "config.h"
#include "trace.h"
#include "usart.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

static volatile bool run;

const Usart usart0;

static void traceOutchar(char c)
{
    usart_putchar((Usart *)&usart0, c);
}

static void traceFlush(void)
{
    usart_flush((Usart *)&usart0);
}

int main(void)
{
  cli();
  run = true;
  sei();
  usart_init((Usart *)&usart0, USART_0, DEBUG_BAUDRATE);
  trace_setup((OutcharFunc)traceOutchar, traceFlush);

  TRACE_INFO_WP("");
  TRACE_INFO("System ready");

  while(!run);
  while(run) {
  }
  return 0;
}
