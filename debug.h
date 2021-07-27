#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdint.h>

#define DEBUG_BAUDRATE  19200

#define DEBUG_HEADER  0xAA

#define DEBUG_PACKETS_BUFFER_SIZE  3

#define DEBUG_CONTROL_PACKET_LEN           7

#define DEBUG_CONTROL_PACKET_PART_HEADER   0
#define DEBUG_CONTROL_PACKET_PART_IDX      1
#define DEBUG_CONTROL_PACKET_PART_VALUE_0  2
#define DEBUG_CONTROL_PACKET_PART_VALUE_1  3
#define DEBUG_CONTROL_PACKET_PART_VALUE_2  4
#define DEBUG_CONTROL_PACKET_PART_VALUE_3  5
#define DEBUG_CONTROL_PACKET_PART_CRC      6

#define DEBUG_CONTROL_PACKET_IDX_UNDEFINED  0x00
#define DEBUG_CONTROL_PACKET_IDX_TYPE_SHFT  7
#define DEBUG_CONTROL_PACKET_IDX_TYPE_MASK  (1 << DEBUG_CONTROL_PACKET_IDX_TYPE_SHFT)
#define DEBUG_CONTROL_PACKET_IDX_TYPE_GET   0
#define DEBUG_CONTROL_PACKET_IDX_TYPE_SET   1
#define DEBUG_CONTROL_PACKET_IDX_GET_SPEED  0x01
#define DEBUG_CONTROL_PACKET_IDX_SET_ANGLE  0x81


typedef union {
    uint8_t bytes[DEBUG_CONTROL_PACKET_LEN];
    struct {
        uint8_t hdr;
        uint8_t idx;
        uint32_t value;
        uint8_t crc;
    };
} DebugControlPacket;

#define DEBUG_REPLY_PACKET_LEN            7

#define DEBUG_REPLY_PACKET_PART_HEADER    0
#define DEBUG_REPLY_PACKET_PART_IDX       1
#define DEBUG_REPLY_PACKET_PART_VALUE_0   2
#define DEBUG_REPLY_PACKET_PART_VALUE_1   3
#define DEBUG_REPLY_PACKET_PART_VALUE_2   4
#define DEBUG_REPLY_PACKET_PART_VALUE_3   5
#define DEBUG_REPLY_PACKET_PART_CRC       6

typedef union {
    uint8_t bytes[DEBUG_REPLY_PACKET_LEN];
    struct {
        uint8_t hdr;
        uint8_t idx;
        uint32_t value;
        uint8_t crc;
    };
} DebugReplyPacket;

void debug_init(void);
void debug_work(void);

#endif /* DEBUG_H_ */