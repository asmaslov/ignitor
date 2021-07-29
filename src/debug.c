#include "debug.h"
#include "usart.h"
#include "meter.h"
#include <avr/io.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

static uint8_t receivedPartIndex;
static DebugControlPacket controlPacket;
static DebugReplyPacket replyPacket;
static Usart usart0;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static void proceed(void) {
    uint8_t i;
    uint8_t crc = 0;

    //TODO: use crc16.h
    for (i = 0; i < DEBUG_CONTROL_PACKET_PART_CRC; i++) {
        crc += controlPacket.bytes[i];
    }
    if (crc == controlPacket.crc) {
        replyPacket.hdr = DEBUG_HEADER;
        switch (controlPacket.idx) {
            case DEBUG_PACKET_IDX_GET_SPEED:
                replyPacket.idx = controlPacket.idx;
                replyPacket.value32 = meter_getSpeed();
                break;
            case DEBUG_PACKET_IDX_GET_ANGLE:
                replyPacket.idx = controlPacket.idx;
                replyPacket.value32 = meter_getAngle();
                break;
            case DEBUG_PACKET_IDX_SET_ANGLE:
                replyPacket.idx = controlPacket.idx;
                if (meter_setAngle(controlPacket.value32)) {
                    replyPacket.value32 = controlPacket.value32;
                } else {
                    replyPacket.value32 = UINT32_MAX;
                }
                break;
            case DEBUG_PACKET_IDX_SET_LED:
                replyPacket.idx = controlPacket.idx;
                debug_led(controlPacket.value32);
                replyPacket.value32 = controlPacket.value32;
                break;
            default:
                replyPacket.idx = DEBUG_PACKET_IDX_UNDEFINED;
                replyPacket.value32 = UINT32_MAX;
                break;
        }
        replyPacket.crc = 0;
        usart_putchar(&usart0, replyPacket.bytes[DEBUG_REPLY_PACKET_PART_HEADER]);
        replyPacket.crc += replyPacket.bytes[DEBUG_REPLY_PACKET_PART_HEADER];
        usart_putchar(&usart0, replyPacket.bytes[DEBUG_REPLY_PACKET_PART_IDX]);
        replyPacket.crc += replyPacket.bytes[DEBUG_REPLY_PACKET_PART_IDX];
        usart_putchar(&usart0, replyPacket.bytes[DEBUG_REPLY_PACKET_PART_VALUE_0]);
        replyPacket.crc += replyPacket.bytes[DEBUG_REPLY_PACKET_PART_VALUE_0];
        usart_putchar(&usart0, replyPacket.bytes[DEBUG_REPLY_PACKET_PART_VALUE_1]);
        replyPacket.crc += replyPacket.bytes[DEBUG_REPLY_PACKET_PART_VALUE_1];
        usart_putchar(&usart0, replyPacket.bytes[DEBUG_REPLY_PACKET_PART_VALUE_2]);
        replyPacket.crc += replyPacket.bytes[DEBUG_REPLY_PACKET_PART_VALUE_2];
        usart_putchar(&usart0, replyPacket.bytes[DEBUG_REPLY_PACKET_PART_VALUE_3]);
        replyPacket.crc += replyPacket.bytes[DEBUG_REPLY_PACKET_PART_VALUE_3];
        usart_putchar(&usart0, replyPacket.crc);
    }
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void debug_init(void) {
    DDRD |= (1 << DDD4);
    debug_led(false);
    receivedPartIndex = DEBUG_CONTROL_PACKET_PART_HEADER;
    usart_init(&usart0, USART_0, DEBUG_BAUDRATE);
}

void debug_work(void) {
    uint8_t byte;

    if (usart0.rxBufferCount > 0) {
        byte = usart_getchar(&usart0);
        switch (receivedPartIndex) {
            case DEBUG_CONTROL_PACKET_PART_HEADER:
                if (DEBUG_HEADER == byte) {
                    controlPacket.hdr = byte;
                    receivedPartIndex = DEBUG_CONTROL_PACKET_PART_IDX;
                }
                break;
            case DEBUG_CONTROL_PACKET_PART_IDX:
                controlPacket.idx = byte;
                if (((byte & DEBUG_PACKET_IDX_TYPE_MASK) >> DEBUG_PACKET_IDX_TYPE_SHFT) == DEBUG_PACKET_IDX_TYPE_SET) {
                    receivedPartIndex = DEBUG_CONTROL_PACKET_PART_VALUE_0;
                } else {
                    controlPacket.bytes[DEBUG_CONTROL_PACKET_PART_VALUE_0] = 0;
                    controlPacket.bytes[DEBUG_CONTROL_PACKET_PART_VALUE_1] = 0;
                    controlPacket.bytes[DEBUG_CONTROL_PACKET_PART_VALUE_2] = 0;
                    controlPacket.bytes[DEBUG_CONTROL_PACKET_PART_VALUE_3] = 0;
                    receivedPartIndex = DEBUG_CONTROL_PACKET_PART_CRC;
                }
                break;
            case DEBUG_CONTROL_PACKET_PART_VALUE_0:
                controlPacket.bytes[DEBUG_CONTROL_PACKET_PART_VALUE_0] = byte;
                receivedPartIndex = DEBUG_CONTROL_PACKET_PART_VALUE_1;
                break;
            case DEBUG_CONTROL_PACKET_PART_VALUE_1:
                controlPacket.bytes[DEBUG_CONTROL_PACKET_PART_VALUE_1] = byte;
                receivedPartIndex = DEBUG_CONTROL_PACKET_PART_VALUE_2;
                break;
            case DEBUG_CONTROL_PACKET_PART_VALUE_2:
                controlPacket.bytes[DEBUG_CONTROL_PACKET_PART_VALUE_2] = byte;
                receivedPartIndex = DEBUG_CONTROL_PACKET_PART_VALUE_3;
                break;
            case DEBUG_CONTROL_PACKET_PART_VALUE_3:
                controlPacket.bytes[DEBUG_CONTROL_PACKET_PART_VALUE_3] = byte;
                receivedPartIndex = DEBUG_CONTROL_PACKET_PART_CRC;
                break;
            case DEBUG_CONTROL_PACKET_PART_CRC:
                controlPacket.crc = byte;
                proceed();
                receivedPartIndex = DEBUG_CONTROL_PACKET_PART_HEADER;
                break;
            default:
                break;
        }
    }
}

void debug_led(bool on) {
    if (on) {
        PORTD &= ~(1 << PD4);
    } else {
        PORTD |= (1 << PD4);
    }
}

void debug_toggle(void) {
    if (PIND & (1 << PIND4)) {
        PORTD &= ~(1 << PD4);
    } else {
        PORTD |= (1 << PD4);
    }
}
