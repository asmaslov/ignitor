#include "config.h"
#include "timer.h"
#include "debug.h"
#include "meter.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

volatile bool run;

static Timer timer2;

static void sparks_init(void) {
    DDRC |= (1 << DDC2) | (1 << DDC3);
    PORTC |= (1 << PC2) | (1 << PC3);
}

static void sparks_ignite(MeterSpark spark) {
    switch (spark) {
        case METER_SPARK_0:

            break;
        case METER_SPARK_1:

            break;
        default:
            break;
    }

    //TODO: Ignite and setup timer0 for spark die depending on speed
}

static void watchdog_init(void)
{
    DDRD |= (1 << DDD3);
    timer_configSimple(&timer2, TIMER_2, WD_RESET_FREQ_HZ, NULL, TIMER_OUTPUT_TOGGLE_B);
    timer_run(&timer2);
}

int main(void)
{
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        run = true;
    }
    debug_init();
    watchdog_init();
    sparks_init();
    meter_init(sparks_ignite);
    while (run) {
        debug_work();
    }
    return 0;
}
