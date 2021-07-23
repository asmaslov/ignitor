#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>
#include <stdbool.h>

typedef void (*TimerHandler)(void);
typedef void (*TimerResultHandler)(uint32_t data);

typedef enum TIMER {
  TIMER_0 = 0,
  TIMER_1 = 1,
  TIMER_2 = 2
} TimerIndex;

typedef enum TIMER_OUTPUT {
  TIMER_OUTPUT_NONE        = 0,
  TIMER_OUTPUT_TOGGLE_OC0A = 4,
  TIMER_OUTPUT_CLEAR_OC0A  = 8,
  TIMER_OUTPUT_SET_OC0A    = 9,
  TIMER_OUTPUT_TOGGLE_OC0B = 1,
  TIMER_OUTPUT_CLEAR_OC0B  = 2,
  TIMER_OUTPUT_SET_OC0B    = 3
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
    uint8_t cs;
} Timer;

bool timer_configSimple(Timer *timer, TimerIndex index, uint32_t freq, TimerHandler handler, TimerOutput out);
void timer_configPwm(Timer *timer, TimerIndex index, uint32_t freq, TimerPwmMode mode);
void timer_configCounter(Timer *timer, TimerIndex index, TimerResultHandler resultHandler);
void timer_configMeter(Timer *timer, TimerIndex index, uint32_t freq, TimerResultHandler resultHandler);
void timer_run(Timer *timer);
void timer_stop(Timer *timer);
void timer_set(Timer *timer, uint8_t duty);
uint32_t timer_get(Timer *timer);

#endif /* TIMER_H_ */
