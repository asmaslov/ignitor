#include "meter.h"
#include "timer.h"
#include "remote.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stddef.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

EEMEM MeterTimingRecord recordsEeprom[METER_TIMING_RECORD_TOTAL_SLOTS];

static MeterTimingRecord records[METER_TIMING_RECORD_TOTAL_SLOTS];
static MeterSparkHandler handler;
static Timer timer0, timer1;
static uint8_t tickIndex, indexes[METER_TICKS];
static uint32_t ticks[METER_TICKS];
static uint16_t rpm;
static volatile bool captured;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

MeterSpark nextSpark;

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static int8_t getValue(uint32_t recordRpm) {
    for (uint8_t i = 0; i < METER_TIMING_RECORD_TOTAL_SLOTS - 1; i++) {
        if (recordRpm < records[i + 1].rpm) {
            return records[i].value;
        }
    }
    return UINT8_MAX;
}

static void pwm(TimerEvent event) {
    if ((TIMER_EVENT_COMPARE_A == event) || (TIMER_EVENT_OVERFLOW == event)) {
        timer_stop(&timer0);
        handler(nextSpark, false);
    } else {
        handler(nextSpark, true);
    }
}

static void ready(uint32_t result) {
    if (result < METER_SENSIBLE_VALUE_MAX) {
        ticks[tickIndex] = result;
        if (captured) {
            uint32_t tickSum = 0;
            for (uint8_t i = 0; i < METER_TICKS; i++) {
                tickSum += ticks[i];
            }
            uint32_t rps = METER_FREQUENCY_HZ / tickSum;
            rpm = rps * 60;
            if ((tickIndex == indexes[1]) || (tickIndex == indexes[3])) {
                nextSpark =
                        (tickIndex == indexes[1]) ?
                                METER_SPARK_1 : METER_SPARK_0;
                if (timer_configPwm(&timer0, TIMER_0, rps * METER_TICKS,
                                    TIMER_PWM_MODE_FAST,
                                    METER_SPARK_PWM_DUTY,
                                    pwm, TIMER_OUTPUT_NONE)) {
                    timer_run(&timer0, getValue(rpm));
                }
            }
            if (METER_TICKS == ++tickIndex) {
                tickIndex = 0;
            }
        } else {
            if (METER_TICKS == ++tickIndex) {
                tickIndex = 0;
                uint32_t min = METER_SENSIBLE_VALUE_MAX;
                for (uint8_t i = 0; i < METER_TICKS; i++) {
                    if (ticks[i] < min) {
                        min = ticks[i];
                        indexes[1] = i;
                    }
                }
                indexes[0] = (indexes[1] + 3) % METER_TICKS;
                indexes[2] = (indexes[1] + 1) % METER_TICKS;
                indexes[3] = (indexes[1] + 2) % METER_TICKS;
                captured = true;
            }
        }
    } else {
        captured = false;
    }
}

ISR(INT0_vect) {
    //TODO: Handle NEU_SIG
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void meter_init(MeterSparkHandler sparkHandler) {
    handler = sparkHandler;
    DDRD |= (1 << DDD5) | (1 << DDD6);
    PORTD |= (1 << PD5) | (1 << PD6);
    meter_applyTimings();
    if (timer_configMeter(&timer1, TIMER_1, METER_FREQUENCY_HZ, ready)) {
        timer_run(&timer1, 0);
    }
    EICRA = (0 << ISC11) | (0 << ISC10) | (1 << ISC01) | (0 << ISC00);
    EIMSK = (0 << INT1) | (1 << INT0);
    tickIndex = 0;
}

uint16_t meter_getRpm(void) {
    if (captured) {
        return rpm;
    }
    return 0;
}

MeterTimingRecord *getTimingRecord(uint8_t slot) {
    return &records[slot];
}

void meter_setTimingRecord(uint8_t slot, const uint16_t rpm,
                           const uint8_t value) {
    records[slot].rpm = rpm;
    records[slot].value = value;
    eeprom_update_block(records + slot, recordsEeprom + slot,
                        sizeof(MeterTimingRecord));
}

void meter_applyTimings(void) {
    eeprom_read_block(
            records, recordsEeprom,
            sizeof(MeterTimingRecord) * METER_TIMING_RECORD_TOTAL_SLOTS);
}
