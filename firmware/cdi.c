#include "cdi.h"
#include "timer.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stddef.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

EEMEM CdiTimingRecord recordsEeprom[CDI_TIMING_RECORD_SLOTS] = {
        { .rps = CDI_RPM_MIN / 60,                         .timing = CDI_TIMING_UNDER_LOW },
        { .rps = CDI_RPM_LOW / 60,                         .timing = CDI_TIMING_UNDER_LOW +     CDI_TIMING_INCR },
        { .rps = CDI_RPM_LOW / 60 +     CDI_RPM_INCR / 60, .timing = CDI_TIMING_UNDER_LOW + 2 * CDI_TIMING_INCR },
        { .rps = CDI_RPM_LOW / 60 + 2 * CDI_RPM_INCR / 60, .timing = CDI_TIMING_UNDER_LOW + 3 * CDI_TIMING_INCR},
        { .rps = CDI_RPM_LOW / 60 + 3 * CDI_RPM_INCR / 60, .timing = CDI_TIMING_UNDER_LOW + 4 * CDI_TIMING_INCR},
        { .rps = CDI_RPM_LOW / 60 + 4 * CDI_RPM_INCR / 60, .timing = CDI_TIMING_UNDER_LOW + 5 * CDI_TIMING_INCR},
        { .rps = CDI_RPM_LOW / 60 + 5 * CDI_RPM_INCR / 60, .timing = CDI_TIMING_UNDER_LOW + 6 * CDI_TIMING_INCR},
        { .rps = CDI_RPM_LOW / 60 + 6 * CDI_RPM_INCR / 60, .timing = CDI_TIMING_UNDER_LOW + 7 * CDI_TIMING_INCR},
        { .rps = CDI_RPM_LOW / 60 + 7 * CDI_RPM_INCR / 60, .timing = CDI_TIMING_UNDER_LOW + 8 * CDI_TIMING_INCR},
        { .rps = CDI_RPM_HIGH / 60,                        .timing = CDI_TIMING_OVER_HIGH },
        { .rps = CDI_RPM_MAX / 60,                         .timing = CDI_TIMING_OVER_HIGH },
};

EEMEM uint8_t globalShiftEeprom = CDI_SHIFT_DEFAULT;

static CdiTimingRecord records[CDI_TIMING_RECORD_SLOTS];
static uint8_t globalShift;
static Timer timer0, timer1;
static volatile uint8_t tickIndex, senseIndex;
static uint8_t indexes[CDI_TICKS];
static uint16_t ticks[CDI_TICKS];
static uint8_t rps;
static volatile CdiSpark spark;
static volatile uint32_t waitCycles, cycleIndex;
static volatile bool captured, busy;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static uint8_t getValue(uint32_t recordRpm) {
    for (uint8_t i = 0; i < CDI_TIMING_RECORD_SLOTS - 1; i++) {
        if (recordRpm < records[i + 1].rps) {
            return (globalShift - records[i].timing);
        }
    }
    return (globalShift - CDI_TIMING_OVER_HIGH);
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
    sparkOut(spark, true);
    busy = false;
}

static void cycle(TimerEvent event) {
    if (++cycleIndex == waitCycles) {
        ignite(event);
    }
}

static void ready(uint16_t result) {
    ticks[tickIndex] = result;
    if (captured) {
        if ((tickIndex == indexes[1]) || (tickIndex == indexes[3])) {
            sparkOut(spark, false);
            if (!busy) {
                busy = true;
                uint32_t tickSum = 0;
                for (uint8_t i = 0; i < CDI_TICKS; i++) {
                    tickSum += ticks[i];
                }
                rps = CDI_FREQUENCY_HZ / tickSum;
                uint8_t value = getValue(rps);
                spark = (tickIndex == indexes[3]) ?
                        CDI_SPARK_FRONT : CDI_SPARK_BACK;
                if (value != 0) {
                    if (rps < CDI_SENSIBLE_RPS_MIN) {
                        waitCycles = CDI_DELAY_FREQUENCY_HZ * value /
                                (rps * CDI_SPARKS * CDI_VALUE_MAX);
                        if (timer_configSimple(&timer0, TIMER_0,
                                               CDI_DELAY_FREQUENCY_HZ,
                                               cycle, TIMER_OUTPUT_NONE)) {
                            cycleIndex = 0;
                            timer_run(&timer0, 0);
                        }
                    } else {
                        uint32_t freq = rps * CDI_SPARKS * CDI_VALUE_MAX / value;
                        if (timer_configSimple(&timer0, TIMER_0, freq,
                                               ignite, TIMER_OUTPUT_NONE)) {
                            timer_run(&timer0, 0);
                        }
                    }
                } else {
                    ignite(TIMER_EVENT_OVERFLOW);
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

uint8_t cdi_getRps(void) {
    if (captured) {
        return rps;
    }
    return 0;
}

CdiTimingRecord *getTimingRecord(uint8_t slot) {
    return &records[slot];
}

void cdi_setTimingRecord(uint8_t slot, const uint8_t rps, const uint8_t timing) {
    records[slot].rps = rps;
    records[slot].timing = timing;
}

uint8_t cdi_getShift(void) {
    return globalShift;
}

void cdi_setShift(uint8_t shift) {
    globalShift = shift;
}

void cdi_saveMem(void) {
    eeprom_update_block(records, recordsEeprom,
                        sizeof(CdiTimingRecord) * CDI_TIMING_RECORD_SLOTS);
    eeprom_update_block(&globalShift, &globalShiftEeprom, sizeof(uint8_t));
}
