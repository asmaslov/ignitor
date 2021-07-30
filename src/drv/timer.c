#include "timer.h"
#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
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

static const uint32_t prescale01[] = { 1, 8, 64, 256, 1024 };
static const uint8_t dv01 = 5;
static const uint32_t prescale2[] = { 1, 8, 32, 64, 128, 256, 1024 };
static const uint8_t dv2 = 7;

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

static bool calc(const TimerIndex index, const uint32_t freq,
                 const uint32_t *prescale, const uint8_t dm, uint8_t *dv,
                 const uint32_t max, uint32_t *ocr, uint32_t *freqReal) {
    uint32_t tmp;

    //NOTE: If "ocr" is NULL we suppose that no output compare is used so we
    //      are looking only for proper divider to provide precise frequency
    *dv = 0;
    if (NULL == ocr) {
        ocr = &tmp;
    }
    do {
        *ocr = F_CPU / (((ocr != &tmp) ? 2 : 1) * freq * prescale[(*dv)++]) - 1;
        if (*ocr == 0) {
            break;
        }
    } while (((&tmp == ocr) || (*ocr > max)) && (*dv < dm));
    if ((((&tmp == ocr) && (*ocr != 0)) || ((ocr != &tmp) && (*ocr > max)))
            && (*dv == dm)) {
        return false;
    }
    (*dv)--;
    *freqReal = F_CPU / (((ocr != &tmp) ? 2 : 1) * prescale[*dv] * (1 + *ocr));
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

ISR(TIMER0_OVF_vect) {
    if (timer0) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            if (timer0->overflowCount < UINT32_MAX) {
                timer0->overflowCount++;
            }
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
    uint32_t count;

    TCNT1 = 0;
    if (timer1) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            count = timer1->overflowCount;
            timer1->overflowCount = 0;
        }
        if (timer1->resultHandler) {
            timer1->resultHandler(ICR1 + count * UINT16_MAX);
        }
    }
}

ISR(TIMER1_OVF_vect) {
    if (timer1) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            if (timer1->overflowCount < UINT32_MAX) {
                timer1->overflowCount++;
            }
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

ISR(TIMER2_OVF_vect) {
    if (timer2) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            if (timer2->overflowCount < UINT32_MAX) {
                timer2->overflowCount++;
            }
        }
    }
}
#endif

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

bool timer_configSimple(Timer *timer, TimerIndex index, uint32_t freq,
                        TimerHandler handler, TimerOutput out) {
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
            if (!calc(timer->index, freq, prescale01, dv01, &timer->clockSelect,
                      UINT8_MAX, &ocr, &timer->freqReal)) {
                return false;
            }
            OCR0A = (uint8_t)ocr;
            if (0 == ocr) {
                wgm = TIMER02_WGM_NORMAL;
                TIMSK0 = (1 << TOIE0);
            } else {
                wgm = TIMER02_WGM_CTC_OCR;
                TIMSK0 = (1 << OCIE0A);
            }
            TCCR0A = ((out & 0xF) << COM0B0) | ((wgm & 0x3) << WGM00);
            TCCR0B = (0 << FOC0A) | (0 << FOC0B) | (((wgm >> 2) & 0x1) << WGM02);
            timer0 = timer;
            break;
#endif
#ifdef TCCR1A
        case TIMER_1:
            if (!calc(timer->index, freq, prescale01, dv01, &timer->clockSelect,
                      UINT16_MAX, &ocr, &timer->freqReal)) {
                return false;
            }
            OCR1A = (uint16_t)ocr;
            if (0 == ocr) {
                wgm = TIMER1_WGM_NORMAL;
                TIMSK1 = (1 << TOIE1);
            } else {
                wgm = TIMER1_WGM_CTC_OCR;
                TIMSK1 = (1 << OCIE1A);
            }
            TCCR1A = ((out & 0xF) << COM1B0) | ((wgm & 0x3) << WGM10);
            TCCR1B = (0 << ICNC1) | (0 << ICES1) | (((wgm >> 2) & 0x3) << WGM12);
            TCCR1C = (0 << FOC1A) | (0 << FOC1B);
            timer1 = timer;
            break;
#endif
#ifdef TCCR2A
        case TIMER_2:
            if (!calc(timer->index, freq, prescale2, dv2, &timer->clockSelect,
                      UINT8_MAX, &ocr, &timer->freqReal)) {
                return false;
            }
            OCR2A = (uint8_t)ocr;
            if (0 == ocr) {
                wgm = TIMER02_WGM_NORMAL;
                TIMSK2 = (1 << TOIE2);
            } else {
                wgm = TIMER02_WGM_CTC_OCR;
                TIMSK2 = (1 << OCIE2A);
            }
            TCCR2A = ((out & 0xF) << COM2B0) | ((wgm & 0x3) << WGM20);
            TCCR2B = (0 << FOC2A) | (0 << FOC2B) | (((wgm >> 2) & 0x1) << WGM22);
            timer2 = timer;
            break;
#endif
        default:
            return false;
    }
    return true;
}

bool timer_configPwm(Timer *timer, TimerIndex index, uint32_t freq,
                     TimerPwmMode mode) {
    if (!timer || (freq > (F_CPU / 2))) {
        return false;
    }
    //TODO: configPwm
    return false;
}

bool timer_configCounter(Timer *timer, TimerIndex index,
                         TimerResultHandler resultHandler) {
    if (!timer) {
        return false;
    }
    //TODO: configCounter
    return false;
}

bool timer_configMeter(Timer *timer, TimerIndex index, uint32_t freq,
                       TimerResultHandler resultHandler) {
    if (!timer || (freq > F_CPU)) {
        return false;
    }
    timer->index = index;
    timer->resultHandler = resultHandler;
    switch (index) {
#ifdef TCCR1A
        case TIMER_1:
            if (!calc(timer->index, freq, prescale01, dv01, &timer->clockSelect,
                      UINT16_MAX, NULL, &timer->freqReal)) {
                return false;
            }
            TIMSK1 = (1 << ICIE1) | (1 << TOIE1);
            TCCR1A = 0;
            TCCR1B = (1 << ICNC1) | (0 << ICES1);
            TCCR1C = 0;
            timer1 = timer;
            break;
#endif
        default:
            return false;
    }
    return true;
}

void timer_run(Timer *timer) {
    if (timer) {
        switch (timer->index) {
#ifdef TCCR0A
            case TIMER_0:
                timer->overflowCount = 0;
                TCNT0 = 0;
                TCCR0B |= ((timer->clockSelect & 0x7) << CS00);
                break;
#endif
#ifdef TCCR1A
            case TIMER_1:
                timer->overflowCount = 0;
                TCNT1 = 0;
                TCCR1B |= ((timer->clockSelect & 0x7) << CS10);
                break;
#endif
#ifdef TCCR2A
            case TIMER_2:
                timer->overflowCount = 0;
                TCNT2 = 0;
                TCCR2B |= ((timer->clockSelect & 0x7) << CS20);
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
                TCCR0B &= ~(0x7 << CS00);
                break;
#endif
#ifdef TCCR1A
            case TIMER_1:
                TCCR1B &= ~(0x7 << CS10);
                break;
#endif
#ifdef TCCR2A
            case TIMER_2:
                TCCR2B &= ~(0x7 << CS20);
                break;
#endif
            default:
                return;
        }
    }
}

void timer_setPwmDuty(Timer *timer, uint8_t duty) {
    if (timer) {
        switch (timer->index) {
            //TODO: Set pwm duty
#ifdef TCCR0A
            case TIMER_0:

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
