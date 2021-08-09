#include "timer.h"
#include "config.h"
#include "remote.h"
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

#if defined(TCCR0A) || defined(TCCR1A)
static const uint16_t prescale01[] = { 1, 8, 64, 256, 1024 };
static const uint8_t dv01 = 5;
#endif
#if defined(TCCR2A)
static const uint16_t prescale2[] = { 1, 8, 32, 64, 128, 256, 1024 };
static const uint8_t dv2 = 7;
#endif

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
                 const uint16_t *prescale, const uint8_t dm, uint8_t *dv,
                 uint16_t max, uint16_t *ocr) {
    uint16_t tmp;

    if (NULL == ocr) {
        //NOTE: If "ocr" is NULL we suppose that no output compare is used so
        //      we are looking only for proper divider for precise frequency
        ocr = &tmp;
        max = 0;
    }
    *dv = 0;
    do {
        *ocr = F_CPU / (((ocr != &tmp) ? 2 : 1) * prescale[(*dv)++] * freq) - 1;
    } while ((*ocr > max) && (*dv < dm));
    if ((*ocr > max) && (dm == *dv)) {
        return false;
    }
    return true;
}

/****************************************************************************
 * Interrupt handler functions                                              *
 ****************************************************************************/

#ifdef TCCR0A
ISR(TIMER0_COMPA_vect) {
    if (timer0) {
        if (timer0->handler) {
            timer0->handler(TIMER_EVENT_COMPARE_A);
        }
    }
}

ISR(TIMER0_COMPB_vect) {
    if (timer0) {
        if (timer0->handler) {
            timer0->handler(TIMER_EVENT_COMPARE_B);
        }
    }
}

ISR(TIMER0_OVF_vect) {
    if (timer0) {
        if (timer0->handler) {
            timer0->handler(TIMER_EVENT_OVERFLOW);
        }
        if (timer0->overflowCount < UINT8_MAX) {
            timer0->overflowCount++;
        }
    }
}
#endif

#ifdef TCCR1A
ISR(TIMER1_COMPA_vect) {
    if (timer1) {
        if (timer1->handler) {
            timer1->handler(TIMER_EVENT_COMPARE_A);
        }
    }
}

ISR(TIMER1_COMPB_vect) {
    if (timer1) {
        if (timer1->handler) {
            timer1->handler(TIMER_EVENT_COMPARE_B);
        }
    }
}

ISR(TIMER1_CAPT_vect) {
    TCNT1 = 0;
    if (timer1) {
        uint32_t count = timer1->overflowCount;
        timer1->overflowCount = 0;
        if (timer1->resultHandler) {
            timer1->resultHandler(ICR1 + count * UINT16_MAX);
        }
    }
}

ISR(TIMER1_OVF_vect) {
    if (timer1) {
        if (timer1->handler) {
            timer1->handler(TIMER_EVENT_OVERFLOW);
        }
        if (timer1->overflowCount < UINT8_MAX) {
            timer1->overflowCount++;
        }
    }
}
#endif

#ifdef TCCR2A
ISR(TIMER2_COMPA_vect) {
    if (timer2) {
        if (timer2->handler) {
            timer2->handler(TIMER_EVENT_COMPARE_A);
        }
    }
}

ISR(TIMER2_COMPB_vect) {
    if (timer2) {
        if (timer2->handler) {
            timer2->handler(TIMER_EVENT_COMPARE_B);
        }
    }
}

ISR(TIMER2_OVF_vect) {
    if (timer2) {
        if (timer2->handler) {
            timer2->handler(TIMER_EVENT_OVERFLOW);
        }
        if (timer2->overflowCount < UINT8_MAX) {
            timer2->overflowCount++;
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
    uint16_t ocr;

    timer->index = index;
    timer->handler = handler;
    switch (index) {
#ifdef TCCR0A
#ifdef TIMER_SIMPLE_0
        case TIMER_0:
        if (!calc(timer->index, freq, prescale01, dv01, &timer->clockSelect,
                        UINT8_MAX, &ocr)) {
            return false;
        }
        if (0 == ocr) {
            wgm = TIMER02_WGM_NORMAL;
            TIMSK0 = (1 << TOIE0);
        } else {
            wgm = TIMER02_WGM_CTC_OCR;
            OCR0A = (uint8_t)ocr;
            TIMSK0 = (1 << OCIE0A);
        }
        TCCR0A = ((out & 0xF) << COM0B0) | ((wgm & 0x3) << WGM00);
        TCCR0B = (0 << FOC0A) | (0 << FOC0B) | (0 << WGM02);
        timer0 = timer;
        return true;
#endif
#endif
#ifdef TCCR1A
#ifdef TIMER_SIMPLE_1
        case TIMER_1:
        if (!calc(timer->index, freq, prescale01, dv01, &timer->clockSelect,
                        UINT16_MAX, &ocr)) {
            return false;
        }
        if (0 == ocr) {
            wgm = TIMER1_WGM_NORMAL;
            TIMSK1 = (1 << TOIE1);
        } else {
            wgm = TIMER1_WGM_CTC_OCR;
            OCR1A = ocr;
            TIMSK1 = (1 << OCIE1A);
        }
        TCCR1A = ((out & 0xF) << COM1B0) | ((wgm & 0x3) << WGM10);
        TCCR1B = (0 << ICNC1) | (0 << ICES1) | (0 << WGM12);
        TCCR1C = (0 << FOC1A) | (0 << FOC1B);
        timer1 = timer;
        return true;
#endif
#endif
#ifdef TCCR2A
#ifdef TIMER_SIMPLE_2
        case TIMER_2:
            if (!calc(timer->index, freq, prescale2, dv2,
                      &timer->clockSelect, UINT8_MAX, &ocr)) {
                return false;
            }
            if (0 == ocr) {
                wgm = TIMER02_WGM_NORMAL;
                TIMSK2 = (1 << TOIE2);
            } else {
                wgm = TIMER02_WGM_CTC_OCR;
                OCR2A = (uint8_t)ocr;
                TIMSK2 = (1 << OCIE2A);
            }
            TCCR2A = ((out & 0xF) << COM2B0) | ((wgm & 0x3) << WGM20);
            TCCR2B = (0 << FOC2A) | (0 << FOC2B) | (0 << WGM22);
            timer2 = timer;
            return true;
#endif
#endif
        default:
            (void)wgm;
            (void)ocr;
            return false;
    }
}

bool timer_configPwm(Timer *timer, const TimerIndex index, const uint32_t freq,
                     const TimerPwmMode mode, const uint16_t duty,
                     const TimerHandler handler, const TimerOutput out) {
    uint8_t wgm;
    uint16_t ocr;

    timer->index = index;
    timer->handler = handler;
    switch (index) {
#ifdef TCCR0A
#ifdef TIMER_PWM_0
        case TIMER_0:
            if (!calc(timer->index, freq, prescale01, dv01,
                      &timer->clockSelect, UINT8_MAX, &ocr))
            {
                return false;
            }
            if (0 == ocr) {
                ocr = UINT8_MAX;
                if (TIMER_PWM_MODE_FAST == mode) {
                    wgm = TIMER02_WGM_PWM_FAST;
                } else {
                    wgm = TIMER02_WGM_PWM_PH;
                }
                TIMSK0 = (1 << TOIE0) | (1 << OCIE0B);
            } else {
                if (TIMER_PWM_MODE_FAST == mode) {
                    wgm = TIMER02_WGM_PWM_FAST_OCR;
                } else {
                    wgm = TIMER02_WGM_PWM_PH_OCR;
                }
                OCR0A = (uint8_t)ocr;
                TIMSK0 = (1 << OCIE0A) | (1 << OCIE0B);
            }
            OCR0B = (uint8_t)(ocr * duty / 100);
            TCCR0A = ((out & 0xF) << COM0B0) | ((wgm & 0x3) << WGM00);
            TCCR0B = (0 << FOC0A) | (0 << FOC0B)
                    | (((wgm >> 2) & 0x1) << WGM02);
            timer0 = timer;
            return true;
#endif
#endif
#ifdef TCCR1A
#ifdef TIMER_PWM_1
            case TIMER_1:
            if (!calc(timer->index, freq, prescale01, dv01, &timer->clockSelect,
                            TIMER_1_PWM_TOP10, &ocr)) {
                return false;
            }
            if (0 == ocr) {
                ocr = UINT18_MAX;
                if (TIMER_PWM_MODE_FAST == mode) {
                    wgm = TIMER1_WGM_PWM_FAST10;
                } else {
                    wgm = TIMER1_WGM_PWM_PH10;
                }
                TIMSK1 = (1 << TOIE1) | (1 << OCIE1B);
            } else {
                if (TIMER_PWM_MODE_FAST == mode) {
                    wgm = TIMER1_WGM_PWM_FAST_OCR;
                } else {
                    wgm = TIMER1_WGM_PWM_PH_OCR;
                }
                OCR1A = ocr;
                TIMSK1 = (1 << OCIE1A) | (1 << OCIE1B);
            }
            OCR0B = ocr * duty / 100;
            TCCR1A = ((out & 0xF) << COM1B0) | ((wgm & 0x3) << WGM10);
            TCCR1B = (0 << ICNC1) | (0 << ICES1)
            | (((wgm >> 2) & 0x3) << WGM12);
            TCCR1C = (0 << FOC1A) | (0 << FOC1B);
            timer1 = timer;
            return true;
#endif
#endif
#ifdef TCCR2A
#ifdef TIMER_PWM_2
            case TIMER_2:
            if (!calc(timer->index, freq, prescale2, dv2, &timer->clockSelect,
                            UINT8_MAX, &ocr)) {
                return false;
            }
            if (0 == ocr) {
                ocr = UINT8_MAX;
                if (TIMER_PWM_MODE_FAST == mode) {
                    wgm = TIMER02_WGM_PWM_FAST;
                } else {
                    wgm = TIMER02_WGM_PWM_PH;
                }
                TIMSK2 = (1 << TOIE2) | (1 << OCIE2B);
            } else {
                if (TIMER_PWM_MODE_FAST == mode) {
                    wgm = TIMER02_WGM_PWM_FAST_OCR;
                } else {
                    wgm = TIMER02_WGM_PWM_PH_OCR;
                }
                OCR2A = (uint8_t)ocr;
                TIMSK2 = (1 << OCIE2A) | (1 << OCIE2B);
            }
            OCR0B = (uint8_t)(ocr * duty / 100);
            TCCR2A = ((out & 0xF) << COM2B0) | ((wgm & 0x3) << WGM20);
            TCCR2B = (0 << FOC2A) | (0 << FOC2B)
            | (((wgm >> 2) & 0x1) << WGM22);
            timer2 = timer;
            return true;
#endif
#endif
        default:
            (void)wgm;
            (void)ocr;
            return false;
    }
}

void timer_configCounter(Timer *timer, const TimerIndex index,
                         const TimerInput input, const uint16_t top,
                         const TimerHandler handler, const TimerOutput out) {
    uint8_t wgm;

    timer->index = index;
    timer->handler = handler;
    switch (index) {
#ifdef TCCR0A
#ifdef TIMER_COUNTER_0
        case TIMER_0:
        if (0 == top) {
            TIMSK0 = 0;
        } else {
            OCR0A = (uint8_t)top;
            if (UINT8_MAX == top) {
                wgm = TIMER1_WGM_NORMAL;
                TIMSK1 = (1 << TOIE1);
            } else {
                wgm = TIMER1_WGM_CTC_OCR;
                TIMSK1 = (1 << OCIE1A);
            }
        }
        TCCR0A = ((out & 0xF) << COM0B0) | ((wgm & 0x3) << WGM00);
        TCCR0B = (0 << FOC0A) | (0 << FOC0B) | (0 << WGM02);
        timer0 = timer;
        break;
#endif
#endif
#ifdef TCCR1A
#ifdef TIMER_COUNTER_1
        case TIMER_1:
        if (0 == top) {
            TIMSK1 = 0;
        } else {
            OCR1A = (uint16_t)top;
            if (UINT16_MAX == top) {
                wgm = TIMER1_WGM_NORMAL;
                TIMSK1 = (1 << TOIE1);
            } else {
                wgm = TIMER1_WGM_CTC_OCR;
                TIMSK1 = (1 << OCIE1A);
            }
        }
        TCCR1A = ((out & 0xF) << COM1B0) | ((wgm & 0x3) << WGM10);
        TCCR1B = (0 << ICNC1) | (0 << ICES1) | (0 << WGM12);
        TCCR1C = (0 << FOC1A) | (0 << FOC1B);
        timer1 = timer;
        break;
#endif
#endif
#ifdef TCCR2A
#ifdef TIMER_COUNTER_2
        case TIMER_2:
        if (0 == top) {
            TIMSK0 = 0;
        } else {
            OCR2A = (uint8_t)top;
            if (UINT8_MAX == top) {
                wgm = TIMER1_WGM_NORMAL;
                TIMSK2 = (1 << TOIE2);
            } else {
                wgm = TIMER1_WGM_CTC_OCR;
                TIMSK2 = (1 << OCIE2A);
            }
        }
        TCCR2A = ((out & 0xF) << COM2B0) | ((wgm & 0x3) << WGM20);
        TCCR2B = (0 << FOC2A) | (0 << FOC2B) | (0 << WGM22);
        timer2 = timer;
        break;
#endif
#endif
        default:
            (void)wgm;
            break;
    }
}

bool timer_configMeter(Timer *timer, TimerIndex index, uint32_t freq,
                       TimerResultHandler resultHandler) {
    timer->index = index;
    timer->resultHandler = resultHandler;
    switch (index) {
#ifdef TCCR1A
#ifdef TIMER_METER_1
        case TIMER_1:
            if (!calc(timer->index, freq, prescale01, dv01,
                      &timer->clockSelect, 0, NULL)) {
                return false;
            }
            TIMSK1 = (1 << ICIE1) | (1 << TOIE1);
            TCCR1A = 0;
            TCCR1B = (1 << ICNC1) | (0 << ICES1);
            TCCR1C = 0;
            timer1 = timer;
            return true;
#endif
#endif
        default:
            return false;
    }
}

void timer_run(Timer *timer, uint16_t start) {
    switch (timer->index) {
#ifdef TCCR0A
        case TIMER_0:
            timer->overflowCount = 0;
            TCNT0 = (uint8_t)start;
            TCCR0B |= ((timer->clockSelect & 0x7) << CS00);
            break;
#endif
#ifdef TCCR1A
        case TIMER_1:
            timer->overflowCount = 0;
            TCNT1 = start;
            TCCR1B |= ((timer->clockSelect & 0x7) << CS10);
            break;
#endif
#ifdef TCCR2A
        case TIMER_2:
            timer->overflowCount = 0;
            TCNT2 = (uint8_t)start;
            TCCR2B |= ((timer->clockSelect & 0x7) << CS20);
            break;
#endif
        default:
            return;
    }
}

void timer_stop(Timer *timer) {
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

void timer_setPwmDuty(Timer *timer, uint8_t duty) {
    uint16_t ocr;

    switch (timer->index) {
#ifdef TCCR0A
        case TIMER_0:
            ocr = OCR0A;
            OCR0B = (uint8_t)(ocr * duty / 100);
            break;
#endif
#ifdef TCCR1A
        case TIMER_1:
            ocr = OCR1A;
            OCR1B = ocr * duty / 100;
            break;
#endif
#ifdef TCCR1A
        case TIMER_2:
            ocr = OCR2A;
            OCR2B = (uint8_t)(ocr * duty / 100);
            break;
#endif
        default:
            return;
    }
}

uint16_t timer_get(Timer *timer) {
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
            return UINT16_MAX;
    }
    return UINT16_MAX;
}
