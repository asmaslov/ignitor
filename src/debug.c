#include "debug.h"
#include "usart.h"
#include "meter.h"
#include <avr/io.h>
#include <util/crc16.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

static uint8_t receivedPartIndex;
static DebugControlPacket controlPacket;
static MeterTimingRecord *record;
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
        replyPacket.idx = controlPacket.idx;
        switch (controlPacket.idx) {
            case DEBUG_PACKET_IDX_GET_RPM:
                replyPacket.value16_0 = meter_getRpm();
                break;
            case DEBUG_PACKET_IDX_GET_RECORD:
                replyPacket.value8_0 = controlPacket.value8_0;
                record = getTimingRecord(controlPacket.value8_0);
                replyPacket.value16_1 = record->rpm;
                replyPacket.value8_1 = record->value;
                break;
            case DEBUG_PACKET_IDX_SET_RECORD:
                meter_setTimingRecord(controlPacket.value8_0, controlPacket.value8_2, controlPacket.value16_1);
                replyPacket.value32 = controlPacket.value32;
                break;
            default:
                return;
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
    replyPacket.hdr = DEBUG_HEADER;
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
