#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum TIMER_INDEX {
    TIMER_0 = 0,
    TIMER_1 = 1,
    TIMER_2 = 2
} TimerIndex;

typedef enum TIMER_INPUT {
    TIMER_INPUT_FALLING_EGDE = 0,
    TIMER_INPUT_RISING_EGDE = 1
} TimerInput;

typedef enum TIMER_OUTPUT {
    TIMER_OUTPUT_NONE = 0,
    TIMER_OUTPUT_TOGGLE_A = 4,
    TIMER_OUTPUT_CLEAR_A = 8,
    TIMER_OUTPUT_SET_A = 9,
    TIMER_OUTPUT_TOGGLE_B = 1,
    TIMER_OUTPUT_CLEAR_B = 2,
    TIMER_OUTPUT_SET_B = 3
} TimerOutput;

typedef enum TIMER_PWM_MODE {
    TIMER_PWM_MODE_FAST,
    TIMER_PWM_MODE_PHASE_CORRECT
} TimerPwmMode;

#define TIMER_1_PWM_TOP8   0x00FF
#define TIMER_1_PWM_TOP9   0x01FF
#define TIMER_1_PWM_TOP10  0x03FF

typedef enum TIMER_EVENT {
    TIMER_EVENT_COMPARE_A,
    TIMER_EVENT_COMPARE_B,
    TIMER_EVENT_OVERFLOW
} TimerEvent;

typedef void (*TimerHandler)(TimerEvent event);
typedef void (*TimerResultHandler)(uint16_t result);

typedef struct {
    TimerIndex index;
    TimerHandler handler;
    TimerResultHandler resultHandler;
    uint8_t clockSelect;
    uint16_t maxValue;
} Timer;

bool timer_configSimple(Timer *timer, const TimerIndex index,
                        const uint32_t freq, const TimerHandler handler,
                        const TimerOutput out);
bool timer_configPwm(Timer *timer, const TimerIndex index, const uint32_t freq,
                     const TimerPwmMode mode, const uint16_t duty,
                     const TimerHandler handler, const TimerOutput out);
void timer_configCounter(Timer *timer, const TimerIndex index,
                         const TimerInput input, const uint16_t top,
                         const TimerHandler handler, const TimerOutput out);
bool timer_configMeter(Timer *timer, const TimerIndex index,
                       const uint32_t freq, const TimerHandler handler,
                       const TimerResultHandler resultHandler);
void timer_run(Timer *timer, uint16_t start);
void timer_stop(Timer *timer);
void timer_setPwmDuty(Timer *timer, const uint8_t duty);
uint16_t timer_get(Timer *timer);

#endif /* TIMER_H_ */
