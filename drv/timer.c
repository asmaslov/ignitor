#include "timer.h"
#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

enum TIMER1_WGM {
    TIMER1_WGM_NORMAL = 0,
    TIMER1_WGM_PWM_PH8 = 1,
    TIMER1_WGM_PWM_PH9 = 2,
    TIMER1_WGM_PWM_PH10 = 3,
    TIMER1_WGM_CTC_OCR = 4,
    TIMER1_WGM_PWM_FAST8 = 5,
    TIMER1_WGM_PWM_FAST9 = 6,
    TIMER1_WGM_PWM_FAST10 = 7,
    TIMER1_WGM_PWM_PH_FR_ICR = 8,
    TIMER1_WGM_PWM_PH_FR_OCR = 9,
    TIMER1_WGM_PWM_PH_ICR = 10,
    TIMER1_WGM_PWM_PH_OCR = 11,
    TIMER1_WGM_CTC_ICR = 12,
    TIMER1_WGM_PWM_FAST_ICR = 14,
    TIMER1_WGM_PWM_FAST_OCR = 15
};

enum TIMER02_WGM {
    TIMER02_WGM_NORMAL = 0,
    TIMER02_WGM_PWM_PH = 1,
    TIMER02_WGM_CTC_OCR = 2,
    TIMER02_WGM_PWM_FAST = 3,
    TIMER02_WGM_PWM_PH_OCR = 5,
    TIMER02_WGM_PWM_FAST_OCR = 7
};

static const uint32_t prescale01[] = { 1, 1, 8, 64, 256, 1024 };
static const uint8_t dv01 = 6;
static const uint32_t prescale2[] = { 1, 1, 8, 32, 64, 128, 256, 1024 };
static const uint8_t dv2 = 8;

#ifdef TCCR0A
static Timer *timer0;
#endif
#ifdef TCCR1A
static Timer *timer1;
#endif
#ifdef TCCR2A
static Timer *timer2;
#endif

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static bool calc(const TimerIndex index, const uint32_t freq, const uint32_t *prescale, const uint8_t dm, uint8_t *dv, const uint32_t max, uint32_t *ocr, uint32_t *freqReal) {
    *dv = 1;
    do {
        *ocr = F_CPU / (2 * freq * prescale[(*dv)++]) - 1;
        if (*ocr == 0) {
            break;
        }
    }
    while ((*ocr > max) && (*dv < dm));
    if ((*ocr > max) && (*dv == dm)) {
        return false;
    }
    (*dv)--;
    *freqReal = F_CPU / (2 * prescale[*dv] * (1 + *ocr));
    return true;
}

/****************************************************************************
 * Interrupt handler functions                                              *
 ****************************************************************************/

#ifdef TCCR0A
ISR(TIMER0_COMPA_vect) {
    if (timer0) {
        if (timer0->handler) {
            timer0->handler();
        }
    }
}
#endif

#ifdef TCCR1A
ISR(TIMER1_COMPA_vect) {
    if (timer1) {
        if (timer1->handler) {
            timer1->handler();
        }
    }
}

ISR(TIMER1_CAPT_vect) {
    uint32_t timeoutUs = 1000000UL * ICR1 / timer1->freqReal;
    if (timer1) {
        if (timer1->resultHandler) {
            timer1->resultHandler(timeoutUs);
        }
    }
}
#endif

#ifdef TCCR2A
ISR(TIMER2_COMPA_vect) {
    if (timer2) {
        if (timer2->handler) {
            timer2->handler();
        }
    }
}
#endif

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

bool timer_configSimple(Timer *timer, TimerIndex index, uint32_t freq, TimerHandler handler, TimerOutput out) {
    uint8_t wgm;
    uint32_t ocr;

    if (!timer || (freq > (F_CPU / 2))) {
        return false;
    }
    timer->index = index;
    timer->handler = handler;
    switch (index) {
        #ifdef TCCR0A
        case TIMER_0:
            if (!calc(timer->index, freq, prescale01, dv01, &timer->cs, UINT8_MAX, &ocr, &timer->freqReal)){
                return false;
            }
            TCCR0A = 0;
            TCCR0A = 0;
            TCCR0B = 0;
            TIMSK0 = 0;
            TCNT0 = 0;
            if (ocr == 0) {
                wgm = TIMER02_WGM_NORMAL;
                OCR0A = 0;
                TIMSK0 = (1 << TOIE0);
            } else {
                wgm = TIMER02_WGM_CTC_OCR;
                OCR0A = (uint8_t)ocr;
                TIMSK0 = (1 << OCIE0A);
            }
            TCCR0A = (((out >> 3) & 1) << COM0A1) | (((out >> 2) & 1) << COM0A0)
                    | (((out >> 1) & 1) << COM0B1)
                    | (((out >> 0) & 1) << COM0B0) | (((wgm >> 1) & 1) << WGM01)
                    | (((wgm >> 0) & 1) << WGM00);
            TCCR0B = (0 << FOC0A) | (0 << FOC0B) | (((wgm >> 2) & 1) << WGM02);
            timer0 = timer;
            break;
        #endif
        #ifdef TCCR1A
        case TIMER_1:
            if (!calc(timer->index, freq, prescale01, dv01, &timer->cs, UINT16_MAX, &ocr, &timer->freqReal)){
                return false;
            }
            TCCR1A = 0;
            TCCR1B = 0;
            TCCR1C = 0;
            TIMSK1 = 0;
            TCNT1 = 0;
            if (ocr == 0) {
                wgm = TIMER1_WGM_NORMAL;
                OCR1A = 0;
                TIMSK1 = (1 << TOIE1);
            } else {
                wgm = TIMER1_WGM_CTC_OCR;
                OCR1A = (uint16_t)ocr;
                TIMSK1 = (1 << OCIE1A);
            }
            TCCR1A = (((out >> 3) & 1) << COM1A1) | (((out >> 2) & 1) << COM1A0)
                    | (((out >> 1) & 1) << COM1B1)
                    | (((out >> 0) & 1) << COM1B0) | (((wgm >> 1) & 1) << WGM11)
                    | (((wgm >> 0) & 1) << WGM10);
            TCCR1B = (0 << ICNC1) | (0 << ICES1) | (((wgm >> 3) & 1) << WGM13)
                    | (((wgm >> 2) & 1) << WGM12);
            TCCR1C = (0 << FOC1A) | (0 << FOC1B);
            timer1 = timer;
            break;
        #endif
        #ifdef TCCR2A
        case TIMER_2:
            if (!calc(timer->index, freq, prescale2, dv2, &timer->cs, UINT8_MAX, &ocr, &timer->freqReal)){
                return false;
            }
            TCCR2A = 0;
            TCCR2B = 0;
            TIMSK2 = 0;
            TCNT2 = 0;
            if (ocr == 0) {
                wgm = TIMER02_WGM_NORMAL;
                OCR2A = 0;
                TIMSK2 = (1 << TOIE2);
            } else {
                wgm = TIMER02_WGM_CTC_OCR;
                OCR2A = (uint8_t)ocr;
                TIMSK2 = (1 << OCIE2A);
            }
            TCCR2A = (((out >> 3) & 1) << COM2A1) | (((out >> 2) & 1) << COM2A0)
                    | (((out >> 1) & 1) << COM2B1)
                    | (((out >> 0) & 1) << COM2B0) | (((wgm >> 1) & 1) << WGM21)
                    | (((wgm >> 0) & 1) << WGM20);
            TCCR2B = (0 << FOC2A) | (0 << FOC2B) | (((wgm >> 2) & 1) << WGM22);
            timer2 = timer;
            break;
        #endif
        default:
            return false;
    }
    return true;
}

bool timer_configPwm(Timer *timer, TimerIndex index, uint32_t freq, TimerPwmMode mode) {
    if (!timer || (freq > (F_CPU / 2))) {
        return false;
    }
    //TODO: configPwm
    return false;
}

bool timer_configCounter(Timer *timer, TimerIndex index, TimerResultHandler resultHandler) {
    if (!timer) {
        return false;
    }
    //TODO: configCounter
    return false;
}

bool timer_configMeter(Timer *timer, TimerIndex index, uint32_t freq, TimerResultHandler resultHandler) {
    uint32_t ocr;
    if (!timer || (freq > (F_CPU / 2))) {
        return false;
    }
    timer->index = index;
    timer->resultHandler = resultHandler;
    switch (index) {
        #ifdef TCCR1A
        case TIMER_1:
            if (!calc(timer->index, freq, prescale01, dv01, &timer->cs, UINT16_MAX, &ocr, &timer->freqReal)){
                return false;
            }
            TCCR1A = 0;
            TCCR1B = 0;
            TCCR1C = 0;
            TIMSK1 = (1 << ICIE1);
            TCNT1 = 0;
            TCCR1B = (1 << ICNC1) | (0 << ICES1);
            timer1 = timer;
            break;
        #endif
        default:
            return false;
    }


    //TODO: configMeter
    return true;
}

void timer_run(Timer *timer) {
    if (timer) {
        switch (timer->index) {
            #ifdef TCCR0A
            case TIMER_0:
                TCNT0 = 0;
                TCCR0B |= ((((timer->cs >> 2) & 1) << CS02) | (((timer->cs >> 1) & 1) << CS01)
                        | (((timer->cs >> 0) & 1) << CS00));
                break;
            #endif
            #ifdef TCCR1A
            case TIMER_1:
                TCNT1 = 0;
                TCCR1B |= ((((timer->cs >> 2) & 1) << CS12) | (((timer->cs >> 1) & 1) << CS11)
                        | (((timer->cs >> 0) & 1) << CS10));
                break;
            #endif
            #ifdef TCCR2A
            case TIMER_2:
                TCNT2 = 0;
                TCCR2B |= ((((timer->cs >> 2) & 1) << CS22) | (((timer->cs >> 1) & 1) << CS21)
                        | (((timer->cs >> 0) & 1) << CS20));
                break;
            #endif
            default:
                return;
        }
    }
}

void timer_stop(Timer *timer) {
    if (timer) {
        switch (timer->index) {
            #ifdef TCCR0A
            case TIMER_0:
                TCCR0B &= ~((1 << CS02) | (1 << CS01) | (1 << CS00));
                break;
            #endif
            #ifdef TCCR1A
                case TIMER_1:
                TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
                break;
            #endif
            #ifdef TCCR2A
                case TIMER_2:
                TCCR2B &= ~((1 << CS22) | (1 << CS21) | (1 << CS20));
                break;
            #endif
            default:
                return;
        }
    }
}

void timer_set(Timer *timer, uint8_t duty) {
    if (timer) {
        switch (timer->index) {
            #ifdef TCCR0A
            case TIMER_0:
                //TODO: Set pwm duty
                break;
            #endif
            #ifdef TCCR1A
            case TIMER_1:

                break;
            #endif
            #ifdef TCCR1A
            case TIMER_2:

                break;
            #endif
            default:
                return;
        }
    }
}

uint32_t timer_get(Timer *timer) {
    if (timer) {
        switch (timer->index) {
            #ifdef TCCR0A
            case TIMER_0:
                return TCNT0;
            #endif
            #ifdef TCCR1A
            case TIMER_1:
                return TCNT1;
            #endif
            #ifdef TCCR2A
            case TIMER_2:
                return TCNT2;
            #endif
            default:
                return UINT32_MAX;
        }
    }
    return UINT32_MAX;
}
