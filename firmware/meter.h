#ifndef METER_H_
#define METER_H_

#include <stdint.h>
#include <stdbool.h>

#define METER_RPM_MIN     100
#define METER_RPM_LOW    1650
#define METER_RPM_HIGH   3500
#define METER_RPM_MAX   13000

#define METER_TIMING_RECORD_TOTAL_SLOTS  11

#define METER_TIMING_UNDER_LOW  5
#define METER_TIMING_OVER_HIGH  30

#define METER_FREQUENCY_HZ  (F_CPU / 64)
#define METER_SENSIBLE_VALUE_MAX  (2 * UINT16_MAX)
#define METER_TICKS  4
#define METER_SPARK_PWM_DUTY  10

typedef enum METER_SPARK {
    METER_SPARK_0 = 0,
    METER_SPARK_1 = 1
} MeterSpark;

typedef struct _MeterTimingRecord {
    uint16_t rpm;
    uint8_t value;
} MeterTimingRecord;

typedef void (*MeterSparkHandler)(MeterSpark spark, bool on);

extern MeterSpark nextSpark;

void meter_init(MeterSparkHandler sparkHandler);
uint16_t meter_getRpm(void);
MeterTimingRecord *getTimingRecord(uint8_t slot);
void meter_setTimingRecord(uint8_t slot, const uint16_t rpm,
                           const uint8_t value);

#endif /* METER_H_ */
