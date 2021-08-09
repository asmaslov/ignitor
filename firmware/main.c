#include "config.h"
#include "timer.h"
#include "remote.h"
#include "meter.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static Timer timer2;

static void sparks_init(void) {
    DDRC |= (1 << DDC2) | (1 << DDC3);
    PORTC |= (1 << PC2) | (1 << PC3);
}

static void sparks_ignite(MeterSpark spark, bool on) {
    if (METER_SPARK_0 == spark) {
        if (on) {
            PORTC &= ~(1 << PC2);
        } else {
            PORTC |= (1 << PC2);
        }
    } else if (METER_SPARK_1 == spark) {
        if (on) {
            PORTC &= ~(1 << PC3);
        } else {
            PORTC |= (1 << PC3);
        }
    }
}

static void watchdog_init(void)
{
    DDRD |= (1 << DDD3);
    timer_configSimple(&timer2, TIMER_2, WD_RESET_FREQ_HZ, NULL, TIMER_OUTPUT_TOGGLE_B);
    timer_run(&timer2, 0);
}

int main(void)
{
    sei();
#ifdef REMOTE
    remote_init();
#endif
    watchdog_init();
    sparks_init();
    meter_init(sparks_ignite);
    while (true) {
#ifdef REMOTE
        remote_work();
#endif
    }
    return 0;
}
