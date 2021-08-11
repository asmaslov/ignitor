#include "cdi.h"
#include "timer.h"
#ifdef REMOTE
#include "remote.h"
#endif
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stddef.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

EEMEM CdiTimingRecord recordsEeprom[CDI_TIMING_RECORD_SLOTS] = {
        { .rpm = CDI_RPM_MIN,                    .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_LOW,                    .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_LOW + CDI_RPM_INCR,     .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_LOW + 2 * CDI_RPM_INCR, .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_LOW + 3 * CDI_RPM_INCR, .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_LOW + 4 * CDI_RPM_INCR, .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_LOW + 5 * CDI_RPM_INCR, .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_LOW + 6 * CDI_RPM_INCR, .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_LOW + 7 * CDI_RPM_INCR, .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_HIGH,                   .timing = CDI_TIMING_UNDER_LOW },
        { .rpm = CDI_RPM_MAX,                    .timing = CDI_TIMING_UNDER_LOW },
};

EEMEM uint8_t globalShiftEeprom = CDI_TIMING_UNDER_LOW;

static CdiTimingRecord records[CDI_TIMING_RECORD_SLOTS];
static uint8_t globalShift;
static Timer timer0, timer1;
static volatile uint8_t tickIndex, senseIndex;
static uint8_t indexes[CDI_TICKS];
static uint16_t ticks[CDI_TICKS];
static uint16_t rpm;
static volatile CdiSpark prevSpark, nextSpark;
static volatile uint32_t last;
static volatile uint16_t waitCycles, cycleIndex;
static volatile bool captured, busy;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static uint8_t getValue(uint32_t recordRpm) {
    for (uint8_t i = 0; i < CDI_TIMING_RECORD_SLOTS - 1; i++) {
        if (recordRpm < records[i + 1].rpm) {
            return (globalShift - records[i].timing);
        }
    }
    return 0;
}

static void sparkOut(CdiSpark spark, bool on) {
    if (CDI_SPARK_FRONT == spark) {
        if (on) {
            PORTC |= (1 << PC2);
        } else {
            PORTC &= ~(1 << PC2);
        }
    } else if (CDI_SPARK_BACK == spark) {
        if (on) {
            PORTC |= (1 << PC3);
        } else {
            PORTC &= ~(1 << PC3);
        }
    }
}

static void ignite(TimerEvent event) {
    timer_stop(&timer0);
    sparkOut(prevSpark, false);
    sparkOut(nextSpark, true);
    busy = false;
}

static void cycle(TimerEvent event) {
    if (++cycleIndex > waitCycles) {
        timer_stop(&timer0);
        if (timer_configSimple(&timer0, TIMER_0, last, ignite,
                               TIMER_OUTPUT_NONE)) {
            timer_run(&timer0, 0);
        }
    }
}

static void ready(uint16_t result) {
    ticks[tickIndex] = result;
    if (captured) {
        if (!busy && ((tickIndex == indexes[1]) || (tickIndex == indexes[3]))) {
            busy = true;
            uint32_t tickSum = 0;
            for (uint8_t i = 0; i < CDI_TICKS; i++) {
                tickSum += ticks[i];
            }
            uint32_t rps = CDI_FREQUENCY_HZ / tickSum;
            rpm = rps * 60;
            prevSpark = nextSpark;
            nextSpark =
                    (tickIndex == indexes[1]) ?
                            CDI_SPARK_FRONT : CDI_SPARK_BACK;
            if (rps < CDI_SENSIBLE_RPS_MIN) {
                //TODO: Fix last value
                waitCycles = CDI_DELAY_FREQUENCY_HZ / (rps * CDI_SPARKS);
                cycleIndex = 0;
                last = rps * CDI_SPARKS * CDI_DELAY_FREQUENCY_HZ
                        / (CDI_DELAY_FREQUENCY_HZ
                                - rps * CDI_SPARKS * waitCycles);
                if (timer_configSimple(&timer0, TIMER_0,
                CDI_DELAY_FREQUENCY_HZ,
                                       cycle, TIMER_OUTPUT_NONE)) {
                    timer_run(&timer0, 0);
                }
            } else {
                if (timer_configSimple(&timer0, TIMER_0, rps * CDI_SPARKS,
                                       ignite, TIMER_OUTPUT_NONE)) {
                    timer_run(
                            &timer0,
                            (uint16_t)(timer0.maxValue
                                    - ((uint32_t)timer0.maxValue * getValue(rpm)
                                            / CDI_VALUE_MAX)));
                }
            }
        }
        if (CDI_TICKS == ++tickIndex) {
            tickIndex = 0;
        }
    } else {
        if (CDI_TICKS == ++tickIndex) {
            tickIndex = 0;
            if (CDI_SENSE_CYCLES == ++senseIndex) {
                senseIndex = 0;
                uint16_t min = UINT16_MAX;
                for (uint8_t i = 0; i < CDI_TICKS; i++) {
                    if (ticks[i] < min) {
                        min = ticks[i];
                        indexes[1] = i;
                    }
                }
                indexes[2] = (indexes[1] + 1) % CDI_TICKS;
                indexes[3] = (indexes[1] + 2) % CDI_TICKS;
                indexes[0] = (indexes[1] + 3) % CDI_TICKS;
                captured = true;
            }
        }
    }
}

static void over(TimerEvent event) {
    if (TIMER_EVENT_OVERFLOW == event) {
        senseIndex = 0;
        captured = false;
        sparkOut(CDI_SPARK_FRONT, false);
        sparkOut(CDI_SPARK_BACK, false);
    }
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void cdi_init() {
    DDRC |= (1 << DDC2) | (1 << DDC3);
    PORTC |= (1 << PC2) | (1 << PC3);
    DDRD |= (1 << DDD5) | (1 << DDD6);
    PORTD |= (1 << PD5) | (1 << PD6);
    eeprom_read_block(records, recordsEeprom,
                      sizeof(CdiTimingRecord) * CDI_TIMING_RECORD_SLOTS);
    eeprom_read_block(&globalShift, &globalShiftEeprom, sizeof(uint8_t));
    if (timer_configMeter(&timer1, TIMER_1, CDI_FREQUENCY_HZ, over, ready)) {
        timer_run(&timer1, 0);
    }
}

uint16_t cdi_getRpm(void) {
    if (captured) {
        return rpm;
    }
    return 0;
}

CdiTimingRecord *getTimingRecord(uint8_t slot) {
    return &records[slot];
}

void cdi_setTimingRecord(uint8_t slot, const uint16_t rpm, const uint8_t timing) {
    records[slot].rpm = rpm;
    records[slot].timing = timing;
    eeprom_update_block(records + slot, recordsEeprom + slot,
                        sizeof(CdiTimingRecord));
}

uint8_t cdi_getShift(void) {
    return globalShift;
}

void cdi_setShift(uint8_t shift) {
    globalShift = shift;
    eeprom_update_block(&globalShift, &globalShiftEeprom, sizeof(uint8_t));
}
