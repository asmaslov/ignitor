#ifndef REMOTE_H_
#define REMOTE_H_

#include <stdint.h>
#include <stdbool.h>

#define REMOTE_BAUDRATE  19200

#define REMOTE_HEADER  0xAA

#define REMOTE_PACKETS_BUFFER_SIZE  3

#define REMOTE_CONTROL_PACKET_LEN           7

#define REMOTE_CONTROL_PACKET_PART_HEADER   0
#define REMOTE_CONTROL_PACKET_PART_CMD      1
#define REMOTE_CONTROL_PACKET_PART_VALUE_0  2
#define REMOTE_CONTROL_PACKET_PART_VALUE_1  3
#define REMOTE_CONTROL_PACKET_PART_VALUE_2  4
#define REMOTE_CONTROL_PACKET_PART_VALUE_3  5
#define REMOTE_CONTROL_PACKET_PART_CRC      6

typedef union {
    uint8_t bytes[REMOTE_CONTROL_PACKET_LEN];
    struct {
        uint8_t hdr;
        uint8_t idx;
        union {
            uint32_t value32;
            struct {
                uint16_t value16_0;
                uint16_t value16_1;
            };
            struct {
                uint8_t value8_0;
                uint8_t value8_1;
                uint8_t value8_2;
                uint8_t value8_3;
            };
        };
        uint8_t crc;
    };
} RemoteControlPacket;

#define REMOTE_REPLY_PACKET_LEN           7

#define REMOTE_REPLY_PACKET_PART_HEADER   0
#define REMOTE_REPLY_PACKET_PART_CMD      1
#define REMOTE_REPLY_PACKET_PART_VALUE_0  2
#define REMOTE_REPLY_PACKET_PART_VALUE_1  3
#define REMOTE_REPLY_PACKET_PART_VALUE_2  4
#define REMOTE_REPLY_PACKET_PART_VALUE_3  5
#define REMOTE_REPLY_PACKET_PART_CRC      6

typedef union {
    uint8_t bytes[REMOTE_REPLY_PACKET_LEN];
    struct {
        uint8_t hdr;
        uint8_t idx;
        union {
            uint32_t value32;
            struct {
                uint16_t value16_0;
                uint16_t value16_1;
            };
            struct {
                uint8_t value8_0;
                uint8_t value8_1;
                uint8_t value8_2;
                uint8_t value8_3;
            };
        };
        uint8_t crc;
    };
} RemoteReplyPacket;

#define REMOTE_PACKET_CMD_TYPE_SHFT   7
#define REMOTE_PACKET_CMD_TYPE_MASK   (1 << REMOTE_PACKET_CMD_TYPE_SHFT)
#define REMOTE_PACKET_CMD_TYPE_GET    0
#define REMOTE_PACKET_CMD_TYPE_SET    1
#define REMOTE_PACKET_CMD_GET_RPM     0x01
#define REMOTE_PACKET_CMD_GET_RECORD  0x21
#define REMOTE_PACKET_CMD_SET_RECORD  0xA1

void remote_init(void);
void remote_work(void);
void remote_led(bool on);
void remote_toggle(void);

#endif /* REMOTE_H_ */
