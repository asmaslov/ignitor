#include "config.h"
#include "timer.h"
#ifdef REMOTE
#include "remote.h"
#endif
#include <cdi.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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
#ifdef REMOTE
    remote_init();
#endif
    watchdog_init();
    cdi_init();
    while (true) {
#ifdef REMOTE
        remote_work();
#endif
    }
    return 0;
}
