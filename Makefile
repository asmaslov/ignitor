PRJ=ignitor
MCU=atmega48
F_CPU=20000000

RM=rm -f

SRCS_C = main.c \
         meter.c \
         debug.c \
         drv/usart.c \
         drv/timer.c

OBJS=$(SRCS_C:.c=.o )

CC=avr-gcc
OBJCOPY=avr-objcopy
SIZE=avr-size

DEFINES = -DF_CPU=${F_CPU}
INCLUDES = -I. -I./drv
OPTIONS = -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -funsigned-char -funsigned-bitfields
CFLAGS = -mmcu=${MCU} ${DEFINES} ${INCLUDES} -std=gnu99 -Wall -g2 -gstabs -Os ${OPTIONS}
LDFLAGS = -Wl,--gc-sections -Wl,-Map,${PRJ}.map

all: ${PRJ}.hex
	pyuic5 ${PRJ}.ui -o ui_${PRJ}.py
	pyrcc5 ${PRJ}.qrc -o ${PRJ}_rc.py
	python h2py.py debug.h
	${SIZE} --format=avr --mcu=${MCU} ${PRJ}.elf

app:
	pyinstaller --hidden-import ui_ignitor --onefile --windowed --icon=${PRJ}.ico ${PRJ}.py

%.o: %.c
	${CC} -c $(CFLAGS) $< -o $@

${PRJ}.hex: ${PRJ}.elf
	${OBJCOPY} -R .eeprom -R .fuse -R .lock -R .signature -O ihex $< $@

${PRJ}.elf: ${OBJS} 
	${CC} -mmcu=${MCU} ${LDFLAGS} -o $@ $^

clean:
	${RM}	${OBJS}
	${RM}	${PRJ}.elf
	${RM}	${PRJ}.hex
	${RM}	${PRJ}.map
	${RM}	ui_${PRJ}.py
	${RM}	${PRJ}_rc.py
	${RM}	DEBUG.py
	${RM}	dist/*
