#include "timer.h"
#include "cdi.h"
#include "remote.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define WD_RESET_FREQ_HZ  126

static Timer timer2;

static void watchdog_init(void)
{
    DDRD |= (1 << DDD3);
    timer_configSimple(&timer2, TIMER_2, WD_RESET_FREQ_HZ, NULL, TIMER_OUTPUT_TOGGLE_B);
    timer_run(&timer2, 0);
}

int main(void)
{
    sei();
    watchdog_init();
    cdi_init();
    remote_init();
    while (true) {
        remote_work();
    }
    return 0;
}
