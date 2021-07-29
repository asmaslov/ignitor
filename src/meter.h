#ifndef METER_H_
#define METER_H_

#include <stdint.h>
#include <stdbool.h>

#define METER_RPM_LOW   1650
#define METER_RPM_HIGH  3500
#define METER_RPM_MAX   10000

#define METER_TIMING_UNDER_LOW  5
#define METER_TIMING_OVER_HIGH  30

#define METER_FREQUENCY_MAX_HZ  (F_CPU / 2)

#define METER_FREQUENCY_HZ  (F_CPU / 2)

typedef enum METER_SPARK {
    METER_SPARK_0 = 0,
    METER_SPARK_1 = 1
} MeterSpark;

typedef void (*MeterSparkHandler)(MeterSpark spark);

void meter_init(MeterSparkHandler sparkHandler);
uint8_t meter_getIgnitionTiming(void);
uint32_t meter_getSpeed(void);
uint32_t meter_getAngle(void);
bool meter_setAngle(uint32_t angle);

#endif /* METER_H_ */
