#include "meter.h"
#include "timer.h"
#include <stdarg.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

MeterSparkHandler h;
Timer timer1;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

void ready(uint32_t timeoutUs) {
    //TODO: Find first cycle, determine speed, call h(METER_SPARK_X)
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void meter_init(MeterSparkHandler handler) {
    if (handler) {
        timer_configMeter(&timer1, TIMER_1, METER_FREQUENCY_MAX_HZ, ready);
        h = handler;
    }
}

uint8_t meter_getIgnitionTiming(void) {
    //TODO: Calculate and return angle based on speed
    return 0;
}

uint32_t meter_getSpeed(void) {
    //TODO: Calculate and return speed
    return 0;
}
