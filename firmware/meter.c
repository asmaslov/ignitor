#include "meter.h"
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

EEMEM MeterTimingRecord recordsEeprom[METER_TIMING_RECORD_TOTAL_SLOTS];

static MeterTimingRecord records[METER_TIMING_RECORD_TOTAL_SLOTS];
static MeterSparkHandler handler;
static Timer timer0, timer1;
static volatile uint8_t tickIndex;
static uint8_t  senseIndex, indexes[METER_TICKS];
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
    if (TIMER_EVENT_COMPARE_B == event) {
        handler(nextSpark, true);
    } else {
        timer_stop(&timer0);
        handler(nextSpark, false);
    }
}

static void ready(uint16_t result) {
    ticks[tickIndex] = result;
    if (captured) {
        if ((tickIndex == indexes[1]) || (tickIndex == indexes[3])) {
            uint32_t tickSum = 0;
            for (uint8_t i = 0; i < METER_TICKS; i++) {
                tickSum += ticks[i];
            }
            uint32_t rps = METER_FREQUENCY_HZ / tickSum;
            rpm = rps * 60;
            nextSpark =
                    (tickIndex == indexes[1]) ?
                            METER_SPARK_1 : METER_SPARK_0;
            if (rps < METER_SENSIBLE_RPS_MIN) {
                //TODO: Make delayed PWM start for slow speed
            } else {
                //FIXME: When RPM is down from 1860 to 1800 detected PWM freeze
                timer_stop(&timer0);
                if (timer_configPwm(&timer0, TIMER_0, rps * METER_TICKS,
                                    TIMER_PWM_MODE_FAST, METER_SPARK_PWM_DUTY,
                                    pwm, TIMER_OUTPUT_NONE)) {
                    timer_run(&timer0, getValue(rpm));
                }
            }
        }
        if (METER_TICKS == ++tickIndex) {
            tickIndex = 0;
        }
    } else {
        if (METER_TICKS == ++tickIndex) {
            tickIndex = 0;
            if (METER_SENSE_CYCLES == ++senseIndex) {
                senseIndex = 0;
                uint32_t min = METER_SENSIBLE_VALUE_MAX;
                for (uint8_t i = 0; i < METER_TICKS; i++) {
                    if (ticks[i] < min) {
                        min = ticks[i];
                        indexes[1] = i;
                    }
                }
                indexes[2] = (indexes[1] + 1) % METER_TICKS;
                indexes[3] = (indexes[1] + 2) % METER_TICKS;
                indexes[0] = (indexes[1] + 3) % METER_TICKS;
                captured = true;
            }
        }
    }
}

static void error(TimerEvent event) {
    if (TIMER_EVENT_OVERFLOW == event) {
        senseIndex = 0;
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
    if (timer_configMeter(&timer1, TIMER_1, METER_FREQUENCY_HZ, error, ready)) {
        timer_run(&timer1, 0);
    }
    EICRA = (0 << ISC11) | (0 << ISC10) | (1 << ISC01) | (0 << ISC00);
    EIMSK = (0 << INT1) | (1 << INT0);
    tickIndex = 0;
    senseIndex = 0;
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
