PRJ=ignitor
MCU=atmega48
F_CPU=20000000

ifeq ($(OS),Windows_NT)
$(info ************ WINDOWS VERSION ************)
RM=del
else
$(info ************* LINUX VERSION *************)
RM=rm -f
endif

TRACE_LEVEL=TRACE_LEVEL_DEBUG

CFLAGS = -mmcu=${MCU} -DF_CPU=${F_CPU}UL -DTRACE_LEVEL=${TRACE_LEVEL} -std=c11 -g -Os -Wall -I. -I./drv -ffunction-sections
LDFLAGS = -Wl,--gc-sections -Wl,-Map,${PRJ}.map

SRCS_C = main.c \
	meter.c \
	trace.c \
	drv/usart.c \
	drv/timer.c \
	drv/adc.c \

OBJS=$(SRCS_C:.c=.o )

CC=avr-gcc
OBJCOPY=avr-objcopy

all: ${PRJ}.bin ${PRJ}.hex

%.o: %.c
	${CC} -c $(CFLAGS) $< -o $@

${PRJ}.hex: ${PRJ}.elf
	${OBJCOPY} -O ihex $< $@

${PRJ}.bin: ${PRJ}.elf
	${OBJCOPY} -O binary $< $@

${PRJ}.elf: ${OBJS} 
	${CC} -mmcu=${MCU} ${LDFLAGS} -o $@ $^

clean:
	${RM}	${OBJS}
	${RM}	${PRJ}.elf
	${RM}	${PRJ}.bin
	${RM}	${PRJ}.hex
	${RM}	${PRJ}.map
