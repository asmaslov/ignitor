#include "meter.h"
#include "timer.h"
#include <avr/io.h>
#include <stddef.h>
#include <assert.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

//TODO: Keep timings in eeprom
static MeterTimingRecord records[METER_TIMING_RECORD_TOTAL_SLOTS];

static MeterSparkHandler handler;
static Timer timer1;
static uint8_t tickIndex;
static uint8_t ticks[METER_TICKS];
static uint32_t tickSum;
static bool captured;
static uint32_t rpm;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static uint8_t getTiming(uint32_t recordRpm) {
    uint8_t i;

    for (i = 0; i < METER_TIMING_RECORD_TOTAL_SLOTS - 1; i++) {
        if (recordRpm < records[i+1].rpm)
        {
            return records[i].timing;
        }
    }
    return UINT8_MAX;
}

static void ready(uint32_t result) {
    uint8_t i;

    if (result < (2 * UINT16_MAX)) {
        ticks[tickIndex] = result;
        if (captured) {
            if (METER_TICKS == ++tickIndex) {
                tickIndex = 0;
            }
            tickSum = 0;
            for (i = 0; i < METER_TICKS; i++) {
                tickSum += ticks[i];
            }
            rpm = METER_FREQUENCY_HZ / tickSum / 60;
            //TODO: If first or third cycle run timer0 to call
            //      handler(METER_SPARK_0/1) with delay based on rpm
        } else {
            if (METER_TICKS == ++tickIndex) {
                tickIndex = 0;
                //TODO: Find first and third cycle
                captured = true;
            }
        }
    } else {
        tickIndex = 0;
        for (i = 0; i < METER_TICKS; i++) {
            ticks[i] = 0;
        }
        tickSum = 0;
        captured = false;
    }
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void meter_init(MeterSparkHandler sparkHandler) {
    handler = sparkHandler;
    DDRD |= (1 << DDD5) | (1 << DDD6);
    PORTD |= (1 << PD5) | (1 << PD6);
    timer_configMeter(&timer1, TIMER_1, METER_FREQUENCY_HZ, ready);
    timer_run(&timer1);
    //TODO: Setup NEU_SIG IRQ
}

uint32_t meter_getRpm(void) {
    if (captured) {
        return rpm;
    }
    return UINT32_MAX;
}

MeterTimingRecord getTimingRecord(uint8_t slot) {
    MeterTimingRecord error = {.rpm = UINT16_MAX, .timing = UINT8_MAX};

    if (slot < METER_TIMING_RECORD_TOTAL_SLOTS) {
        return records[slot];
    }
    return error;
}

bool meter_setTimingRecord(uint8_t slot, MeterTimingRecord record)
{
    if (slot < METER_TIMING_RECORD_TOTAL_SLOTS) {
        records[slot] = record;
        return true;
    }
    return false;
}
