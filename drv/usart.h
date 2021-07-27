#ifndef USART_H_
#define USART_H_

#include <stdint.h>
#include <stdbool.h>

#define USART_BUFFER_SIZE   8

typedef enum USART {
  USART_0 = 0,
  USART_1 = 1
} UsartIndex;

typedef struct {
    UsartIndex index;
    volatile uint8_t *regData;
    uint8_t rxBuffer[USART_BUFFER_SIZE];
    uint8_t rxBufferIndexRead, rxBufferIndexWrite;
    volatile uint8_t rxBufferCount;
    volatile bool rxBufferLocked, rxBufferEmpty, rxBufferOverflow;
    uint8_t txBuffer[USART_BUFFER_SIZE];
    uint8_t txBufferIndexRead, txBufferIndexWrite;
    volatile uint8_t txBufferCount;
    volatile bool txBufferLocked, txBufferEmpty;
} Usart;

bool usart_init(Usart *usart, const UsartIndex index, const uint32_t baudrate);
void usart_putchar(Usart *usart, const uint8_t data);
void usart_putstr(Usart *usart, const char *str);
const uint8_t usart_getchar(Usart *usart);
void usart_flush(Usart *usart);

#endif /* USART_H_ */
