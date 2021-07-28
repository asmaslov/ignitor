#ifndef METER_H_
#define METER_H_

#include <stdint.h>
#include <stdbool.h>

#define METER_FREQUENCY_MAX_HZ  (F_CPU / 2)

typedef enum METER_SPARK {
    METER_SPARK_0 = 0,
    METER_SPARK_1 = 1
} MeterSpark;

typedef void (*MeterSparkHandler)(MeterSpark spark);

void meter_init(MeterSparkHandler handler);
uint8_t meter_getIgnitionTiming(void);
uint32_t meter_getSpeed(void);
uint32_t meter_getAngle(void);
bool meter_setAngle(uint32_t angle);

#endif /* METER_H_ */
