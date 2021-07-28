#include "meter.h"
#include "timer.h"
#include <stddef.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

static MeterSparkHandler h;
static Timer timer1;
static uint32_t a;

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
    a = 246;
}

uint8_t meter_getIgnitionTiming(void) {
    //TODO: Calculate and return angle based on speed
    return 0;
}

uint32_t meter_getSpeed(void) {
    //TODO: Calculate and return speed
    return 0x12345678;
}

uint32_t meter_getAngle(void) {
    //TODO: Calculate and return speed
    return a;
}

bool meter_setAngle(uint32_t angle)
{
    //TODO: Check and set angle
    a = angle;
    return true;
}
