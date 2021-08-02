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
    uint8_t crc = 0;

    //TODO: use crc16.h
    for (uint8_t i = 0; i < DEBUG_CONTROL_PACKET_PART_CRC; i++) {
        crc += controlPacket.bytes[i];
    }
    if (crc == controlPacket.crc) {
        replyPacket.hdr = DEBUG_HEADER;
        switch (controlPacket.idx) {
            case DEBUG_PACKET_IDX_GET_RPM:
                replyPacket.idx = controlPacket.idx;
                replyPacket.value32 = meter_getRpm();
                break;
            case DEBUG_PACKET_IDX_GET_TIMING:
                replyPacket.idx = controlPacket.idx;
                replyPacket.value8_0 = controlPacket.value8_0;
                replyPacket.value8_1 = getTimingRecord(controlPacket.value8_0).timing;
                replyPacket.value16_1 = getTimingRecord(controlPacket.value8_0).rpm;
                break;
            case DEBUG_PACKET_IDX_SET_TIMING:
                replyPacket.idx = controlPacket.idx;
                MeterTimingRecord record;
                record.rpm = controlPacket.value16_1;
                record.timing = controlPacket.value8_1;
                if (meter_setTimingRecord(controlPacket.value8_0, record)) {
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
        for (uint8_t i = DEBUG_REPLY_PACKET_PART_HEADER; i < DEBUG_REPLY_PACKET_PART_CRC; i++) {
            usart_putchar(&usart0, replyPacket.bytes[i]);
            replyPacket.crc += replyPacket.bytes[i];
        }
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
    if (usart0.rxBufferCount > 0) {
        uint8_t byte = usart_getchar(&usart0);
        controlPacket.bytes[receivedPartIndex] = byte;
        switch (receivedPartIndex) {
            case DEBUG_CONTROL_PACKET_PART_HEADER:
                if (DEBUG_HEADER == byte) {
                    receivedPartIndex++;
                }
                break;
            case DEBUG_CONTROL_PACKET_PART_IDX:
            case DEBUG_CONTROL_PACKET_PART_VALUE_0:
            case DEBUG_CONTROL_PACKET_PART_VALUE_1:
            case DEBUG_CONTROL_PACKET_PART_VALUE_2:
            case DEBUG_CONTROL_PACKET_PART_VALUE_3:
                receivedPartIndex++;
                break;
            case DEBUG_CONTROL_PACKET_PART_CRC:
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
