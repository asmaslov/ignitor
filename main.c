#include "config.h"
#include "timer.h"
#include "debug.h"
#include "meter.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

volatile bool run;

static Timer timer2;

static void ignite(MeterSpark spark) {
    //TODO: Ignite
}

static void watchdog_init(void)
{
    DDRD |= (1 << DDD3);
    timer_configSimple(&timer2, TIMER_2, WD_RESET_FREQ_HZ, NULL, TIMER_OUTPUT_TOGGLE_B);
    timer_run(&timer2);
}

int main(void)
{
    cli();
    run = true;
    sei();

    debug_init();
    watchdog_init();
    meter_init(ignite);
    while (run) {
        debug_work();
    }
    return 0;
}
