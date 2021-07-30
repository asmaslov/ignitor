#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>
#include <stdbool.h>

typedef void (*TimerHandler)(void);
typedef void (*TimerResultHandler)(uint32_t result);

typedef enum TIMER {
    TIMER_0 = 0,
    TIMER_1 = 1,
    TIMER_2 = 2
} TimerIndex;

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
    TIMER_PWM_MODE_PHASE_CORRECT,
    TIMER_PWM_MODE_EXTERNAL_ICR,
} TimerPwmMode;

typedef struct {
    TimerIndex index;
    TimerHandler handler;
    TimerResultHandler resultHandler;
    uint8_t clockSelect;
    uint32_t freqReal;
    uint32_t overflowCount;
} Timer;

bool timer_configSimple(Timer *timer, const TimerIndex index,
                        const uint32_t freq, const TimerHandler handler,
                        const TimerOutput out);
bool timer_configPwm(Timer *timer, const TimerIndex index, const uint32_t freq,
                     const TimerPwmMode mode);
bool timer_configCounter(Timer *timer, const TimerIndex index,
                         const TimerResultHandler resultHandler);
bool timer_configMeter(Timer *timer, const TimerIndex index,
                       const uint32_t freq,
                       const TimerResultHandler resultHandler);
void timer_run(Timer *timer);
void timer_stop(Timer *timer);
void timer_setPwmDuty(Timer *timer, const uint8_t duty);
uint32_t timer_get(Timer *timer);

#endif /* TIMER_H_ */
