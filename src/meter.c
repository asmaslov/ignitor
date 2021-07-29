#include "meter.h"
#include "timer.h"
#include <avr/io.h>
#include <stddef.h>
#include <assert.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

static MeterSparkHandler handler;
static Timer timer1;
static bool captured;
static uint32_t speed;

static uint32_t a;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static void ready(uint16_t result) {
    uint32_t timeoutUs = 1000000UL * result / timer1.freqReal;
    //TODO: Find first cycle, determine speed, call h(METER_SPARK_X)
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void meter_init(MeterSparkHandler sparkHandler) {
    if(METER_FREQUENCY_HZ)
    handler = sparkHandler;
    DDRD |= (1 << DDD5) | (1 << DDD6);
    PORTD |= (1 << PD5) | (1 << PD6);
    timer_configMeter(&timer1, TIMER_1, METER_FREQUENCY_MAX_HZ, ready);
    timer_run(&timer1);
    //TODO: Setup NEU_SIG IRQ
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
