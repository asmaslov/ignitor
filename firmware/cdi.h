#ifndef CDI_H_
#define CDI_H_

#include <stdint.h>
#include <stdbool.h>

#define CDI_RPM_STEP    60
#define CDI_RPM_MIN     60
#define CDI_RPM_LOW   1620
#define CDI_RPM_INCR   240
#define CDI_RPM_HIGH  3540
#define CDI_RPM_MAX   5100

#define CDI_VALUE_MAX  180

#define CDI_TIMING_RECORD_SLOTS  11

#define CDI_TIMING_UNDER_LOW  5
#define CDI_TIMING_INCR       3
#define CDI_TIMING_OVER_HIGH  32

#define CDI_TICKS  4
#define CDI_SPARKS  2
#define CDI_DELAY_FREQUENCY_HZ  (F_CPU / 256)
#define CDI_SENSE_CYCLES  2
#define CDI_SPARK_PWM_DUTY  90

#define CDI_FREQUENCY_HZ  (F_CPU / 64)
#define CDI_SENSIBLE_RPS_MIN  16  // (F_CPU / (CDI_SPARKS * 1024 * (UINT8_MAX + 1)))

typedef enum CDI_SPARK {
    CDI_SPARK_FRONT = 0,
    CDI_SPARK_BACK = 1
} CdiSpark;

typedef struct _CdiTimingRecord {
    uint8_t rps;
    uint8_t timing;
} CdiTimingRecord;

void cdi_init();
uint8_t cdi_getRps(void);
CdiTimingRecord *getTimingRecord(uint8_t slot);
void cdi_setTimingRecord(uint8_t slot, const uint8_t rps,
                         const uint8_t timing);
uint8_t cdi_getShift(void);
void cdi_setShift(uint8_t shift);
void cdi_saveMem(void);

#endif /* CDI_H_ */
