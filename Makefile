PRJ=ignitor
MCU=atmega48pa
F_CPU=12070000

ifeq ($(OS),Windows_NT)
$(info ************ WINDOWS VERSION ************)
RM=del
else
$(info ************* LINUX VERSION *************)
RM=rm
endif

CFLAGS = -mmcu=${MCU} -DF_CPU=${F_CPU}UL -g -Os -Wall -I. -I./drv -ffunction-sections
LDFLAGS = -Wl,--gc-sections -Wl,-Map,${PRJ}.map

SRCS_C = main.c \
	trace.c \
	drv/usart.c \
	drv/timer.c \
	drv/adc.c \

OBJS=$(SRCS_C:.c=.o )

CC=avr-gcc
OBJCOPY=avr-objcopy

all: ${PRJ}.hex

%.o: %.c
	${CC} -c $(CFLAGS) $< -o $@

${PRJ}.hex: ${PRJ}.elf
	${OBJCOPY} -O ihex $< $@

${PRJ}.elf: ${OBJS} 
	${CC} -mmcu=${MCU} ${LDFLAGS} -o $@ $^

clean:
	${RM}	${OBJS}
	${RM}	${PRJ}.elf
	${RM}	${PRJ}.hex
	${RM}	${PRJ}.map
