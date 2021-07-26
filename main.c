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

uint8_t usart0Data[6];
char usart0DataStr[128];

void ignite(MeterSpark spark) {
    //TODO: Ignite
}

int main(void)
{
    cli();
    run = true;
    sei();
    usart_init(&usart0, USART_0, DEBUG_BAUDRATE);
    usart0Data[0] = UCSR0A;
    usart0Data[1] = UCSR0B;
    usart0Data[2] = UCSR0C;
    usart0Data[3] = UBRR0L;
    usart0Data[4] = UBRR0H;
    sprintf(usart0DataStr, "UCSR0A = 0x%02X; UCSR0B = 0x%02X; UCSR0C = 0x%02X; UBRR0L = 0x%02X; UBRR0H = 0x%02X;",
            usart0Data[0], usart0Data[1], usart0Data[2], usart0Data[3], usart0Data[4]);
    UDR0='!';
    usart_putstr(&usart0, usart0DataStr);
    usart_putstr(&usart0, "System ready");
    //timer_configSimple(&timer2, TIMER_2, WD_RESET_FREQ_HZ, NULL, TIMER_OUTPUT_TOGGLE_B);
    //meter_init(ignite);

    while(!run);
    while(run) {
    }
    return 0;
}
