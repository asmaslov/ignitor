PRJ=ignitor
MCU=atmega48
F_CPU=20000000

RM=rm -f

SRCS_C = main.c \
	meter.c \
	drv/usart.c \
	drv/timer.c \
	drv/adc.c \

OBJS=$(SRCS_C:.c=.o )

CC=avr-gcc
OBJCOPY=avr-objcopy
SIZE=avr-size

INCLUDES = -I. -I./drv
CFLAGS = -mmcu=${MCU} -DF_CPU=${F_CPU}UL ${INCLUDES} -Wall -g2 -gstabs -Os -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -std=gnu99 -funsigned-char -funsigned-bitfields
LDFLAGS = -Wl,--gc-sections -Wl,-Map,${PRJ}.map

all: ${PRJ}.hex

%.o: %.c
	${CC} -c $(CFLAGS) $< -o $@

${PRJ}.hex: ${PRJ}.elf
	${OBJCOPY} -R .eeprom -R .fuse -R .lock -R .signature -O ihex $< $@

${PRJ}.elf: ${OBJS} 
	${CC} -mmcu=${MCU} ${LDFLAGS} -o $@ $^
	${SIZE} --format=avr --mcu=${MCU} ${PRJ}.elf

clean:
	${RM}	${OBJS}
	${RM}	${PRJ}.elf
	${RM}	${PRJ}.hex
	${RM}	${PRJ}.map
