#include "remote.h"
#include "usart.h"
#include <avr/io.h>
#include <cdi.h>
#include <util/crc16.h>

/****************************************************************************
 * Private types/enumerations/variables                                     *
 ****************************************************************************/

static uint8_t receivedPartIndex;
static RemoteControlPacket controlPacket;
static CdiTimingRecord *record;
static RemoteReplyPacket replyPacket;
static Usart usart0;

/****************************************************************************
 * Public types/enumerations/variables                                      *
 ****************************************************************************/

/****************************************************************************
 * Private functions                                                        *
 ****************************************************************************/

static void proceed(void) {
    uint8_t crc = 0;

    //TODO: Use crc16.h
    for (uint8_t i = 0; i < REMOTE_CONTROL_PACKET_PART_CRC; i++) {
        crc += controlPacket.bytes[i];
    }
    if (crc == controlPacket.crc) {
        replyPacket.cmd = controlPacket.cmd;
        switch (controlPacket.cmd) {
            case REMOTE_PACKET_CMD_GET_RPM:
                replyPacket.value16_0 = cdi_getRpm();
                break;
            case REMOTE_PACKET_CMD_GET_RECORD:
                if (controlPacket.value8_0 < CDI_TIMING_RECORD_SLOTS) {
                    replyPacket.value8_0 = controlPacket.value8_0;
                    record = getTimingRecord(controlPacket.value8_0);
                    replyPacket.value16_1 = record->rpm;
                    replyPacket.value8_1 = record->timing;
                } else {
                    return;
                }
                break;
            case REMOTE_PACKET_CMD_GET_SHIFT:
                replyPacket.value8_0 = cdi_getShift();
                break;
            case REMOTE_PACKET_CMD_SET_RECORD:
                if (controlPacket.value8_0 < CDI_TIMING_RECORD_SLOTS) {
                    replyPacket.value8_0 = controlPacket.value8_0;
                    cdi_setTimingRecord(controlPacket.value8_0, controlPacket.value16_1, controlPacket.value8_1);
                    replyPacket.value16_1 = controlPacket.value16_1;
                    replyPacket.value8_1 = controlPacket.value8_1;
                } else {
                    return;
                }
                break;
            case REMOTE_PACKET_CMD_SET_SHIFT:
                if (controlPacket.value8_0 < CDI_VALUE_MAX) {
                    cdi_setShift(controlPacket.value8_0);
                } else {
                    return;
                }
                break;
            default:
                return;
        }
        replyPacket.crc = 0;
        for (uint8_t i = REMOTE_REPLY_PACKET_PART_HEADER; i < REMOTE_REPLY_PACKET_PART_CRC; i++) {
            usart_putchar(&usart0, replyPacket.bytes[i]);
            replyPacket.crc += replyPacket.bytes[i];
        }
        usart_putchar(&usart0, replyPacket.crc);
    }
}

/****************************************************************************
 * Public functions                                                         *
 ****************************************************************************/

void remote_init(void) {
    DDRD |= (1 << DDD4);
    remote_led(false);
    receivedPartIndex = REMOTE_CONTROL_PACKET_PART_HEADER;
    replyPacket.hdr = REMOTE_HEADER;
    usart_init(&usart0, USART_0, REMOTE_BAUDRATE);
}

void remote_work(void) {
    if (usart0.rxBufferCount > 0) {
        uint8_t byte = usart_getchar(&usart0);
        controlPacket.bytes[receivedPartIndex] = byte;
        switch (receivedPartIndex) {
            case REMOTE_CONTROL_PACKET_PART_HEADER:
                if (REMOTE_HEADER == byte) {
                    receivedPartIndex++;
                }
                break;
            case REMOTE_CONTROL_PACKET_PART_CMD:
            case REMOTE_CONTROL_PACKET_PART_VALUE_0:
            case REMOTE_CONTROL_PACKET_PART_VALUE_1:
            case REMOTE_CONTROL_PACKET_PART_VALUE_2:
            case REMOTE_CONTROL_PACKET_PART_VALUE_3:
                receivedPartIndex++;
                break;
            case REMOTE_CONTROL_PACKET_PART_CRC:
                proceed();
                receivedPartIndex = REMOTE_CONTROL_PACKET_PART_HEADER;
                break;
        }
    }
}

void remote_led(bool on) {
    if (on) {
        PORTD &= ~(1 << PD4);
    } else {
        PORTD |= (1 << PD4);
    }
}
