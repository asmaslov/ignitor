#include "usart.h"
#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <limits.h>
#include <string.h>

#include "trace.h"

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

#ifdef UCSR0A
#define RECEIVE_COMPLETE_0     (UCSR0A & (1 << RXC0))
#define TRANSMIT_COMPLETE_0    (UCSR0A & (1 << TXC0))
#define DATA_REGISTER_EMPTY_0  (UCSR0A & (1 << UDRE0))
#define FRAME_ERROR_0          (UCSR0A & (1 << FE0))
#define DATA_OVERRUN_0         (UCSR0A & (1 << DOR0))
#define PARITY_ERROR_0         (UCSR0A & (1 << UPE0))
Usart *usart0;
#endif
#ifdef UCSR1A
#define RECEIVE_COMPLETE_1     (UCSR1A & (1 << RXC1))
#define TRANSMIT_COMPLETE_1    (UCSR1A & (1 << TXC1))
#define DATA_REGISTER_EMPTY_1  (UCSR1A & (1 << UDRE1))
#define FRAME_ERROR_1          (UCSR1A & (1 << FE1))
#define DATA_OVERRUN_1         (UCSR1A & (1 << DOR1))
#define PARITY_ERROR_1         (UCSR1A & (1 << UPE1))
Usart *usart1;
#endif

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static inline bool dataRegisterEmpty(UsartIndex index) {
    switch (index) {
        #ifdef UCSR0A
        case USART_0:
            return (UCSR0A & (1 << UDRE0));
        #endif
        #ifdef UCSR1A
        case USART_1:
            return (UCSR1A & (1 << UDRE1));
            break;
        #endif
        default:
            return false;
    }
}

/****************************************************************************
 * Interrupt handler functions                                              *
 ****************************************************************************/

#ifdef UCSR0A
#ifndef UCSR1A
ISR(USART_RX_vect) {
#else
ISR(USART0_RX_vect) {
#endif
    if (usart0 && !FRAME_ERROR_0 && !PARITY_ERROR_0 && !DATA_OVERRUN_0) {
        usart0->rxBuffer[usart0->rxBufferIndexWrite++] = UDR0;
        if (USART_BUFFER_SIZE == usart0->rxBufferIndexWrite) {
            usart0->rxBufferIndexWrite = 0;
        }
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            usart0->rxBufferLocked = true;
            if (++usart0->rxBufferCount >= USART_BUFFER_SIZE) {
                usart0->rxBufferOverflow = true;
            }
            usart0->rxBufferLocked = false;
        }
        usart0->rxBufferEmpty = false;
    }
}

#ifndef UCSR1A
ISR(USART_TX_vect) {
#else
ISR(USART0_TX_vect) {
#endif
    if (usart0) {
        if (!usart0->txBufferEmpty)
        {
            UDR0 = usart0->txBuffer[usart0->txBufferIndexRead++];
            if (USART_BUFFER_SIZE == usart0->txBufferIndexRead) {
                usart0->txBufferIndexRead = 0;
            }
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                usart0->txBufferLocked = true;
                if (--usart0->txBufferCount == 0) {
                    usart0->txBufferEmpty = true;
                }
                usart0->txBufferLocked = false;
            }
        }
    }
}
#endif

#ifdef UCSR1A
ISR(USART1_RX_vect) {
    if (usart1 && !FRAME_ERROR_1 && !PARITY_ERROR_1 && !DATA_OVERRUN_1)
    {
        usart1->rxBuffer[usart1->rxBufferIndexWrite++] = UDR1;
        if (USART_BUFFER_SIZE == usart1->rxBufferIndexWrite)
        {
            usart1->rxBufferIndexWrite = 0;
        }
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            usart1->rxBufferLocked = true;
            if (++usart1->rxBufferCount >= USART_BUFFER_SIZE)
            {
                usart1->rxBufferOverflow = true;
            }
            usart1->rxBufferLocked = false;
        }
        usart1->rxBufferEmpty = false;
    }
}

ISR(USART1_TX_vect) {
    if (usart1) {
        if (!usart1->txBufferEmpty)
        {
            UDR1 = usart1->txBuffer[usart1->txBufferIndexRead++];
            if (USART_BUFFER_SIZE == usart1->txBufferIndexRead)
            {
                usart1->txBufferIndexRead = 0;
            }
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                usart1->txBufferLocked = true;
                if (--usart1->txBufferCount == 0)
                {
                    usart1->txBufferEmpty = true;
                }
                usart1->txBufferLocked = false;
            }
        }
}
#endif

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

bool usart_init(Usart *usart, const UsartIndex index, const uint32_t baudrate) {
    if (!usart) {
        return false;
    }
    memset(usart, 0, sizeof(Usart));
    usart->index = index;
    usart->rxBufferEmpty = true;
    usart->txBufferEmpty = true;
    switch (index) {
        #ifdef UCSR0A
        case USART_0:
            UCSR0A = 0;
            UCSR0B = (1 << RXCIE0) | (1 << TXCIE0) | (1 << RXEN0) | (1 << TXEN0);
            UCSR0C = (1 << UCSZ00) | (1 << UCSZ01);
            UBRR0 = F_CPU / (16 * baudrate) - 1;
            usart->dataReg = &UDR0;
            usart0 = usart;
            break;
        #endif
        #ifdef UCSR1A
        case USART_1:
            UCSR1A = 0;
            UCSR1B = (1 << RXCIE1) | (1 << TXCIE1) | (1 << RXEN1) | (1 << TXEN1);
            UCSR1C = (1 << UCSZ10) | (1 << UCSZ11);
            UBRR1 = F_CPU / (16 * baudrate) - 1;
            usart->dataReg = &UDR1;
            usart1 = usart;
            break;
        #endif
        default:
            return false;
    }
    return true;
}

void usart_putchar(Usart *usart, const uint8_t data) {
    if (usart) {
        while (usart->txBufferCount == USART_BUFFER_SIZE);
        if (!usart->txBufferEmpty || !dataRegisterEmpty(usart->index)) {
            usart->txBuffer[usart->txBufferIndexWrite++] = data;
            if (USART_BUFFER_SIZE == usart->txBufferIndexWrite) {
                usart->txBufferIndexWrite = 0;
            }
            while (usart->txBufferLocked);
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                usart->txBufferCount++;
            }
            usart->txBufferEmpty = false;
        }
        else {
            *usart->dataReg = data;
        }
    }
}

void usart_putstr(Usart *usart, const char *str) {
    if (usart) {
        while (*str != '\0') {
            usart_putchar(usart, *str++);
        }
    }
}

const uint8_t usart_getchar(Usart *usart) {
    uint8_t data = UCHAR_MAX;

    if (usart) {
        while (usart->rxBufferEmpty);
        data = usart->rxBuffer[usart->rxBufferIndexRead++];
        if (USART_BUFFER_SIZE == usart->rxBufferIndexRead) {
            usart->rxBufferIndexRead = 0;
        }
        while (usart->rxBufferLocked);
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            usart->rxBufferCount--;
        }
        if (0 == usart->rxBufferCount) {
            usart->rxBufferEmpty = true;
        }
    }
    return data;
}

void usart_flush(Usart *usart) {
    if (usart) {
        while (usart->txBufferCount != 0);
    }
}
