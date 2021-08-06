PRJ = ignitor
MCU = atmega48
F_CPU = 20000000

RM = rm -rf
MKDIR = mkdir -p
SRC_DIR = $(PWD)
BUILD_DIR = $(PWD)/../build-${PRJ}-firmware

SRCS_C = $(shell find ${SRC_DIR} -type f -name '*.c')
INCS_C = $(shell find ${SRC_DIR} -type f -name '*.h')
INCS_DIRS = $(addprefix -I, $(dir ${INCS_C}))
OBJS_DIRS = $(subst ${SRC_DIR}, ${BUILD_DIR}, $(dir ${SRCS_C}))
OBJS = $(subst ${SRC_DIR}, ${BUILD_DIR}, $(patsubst %.c, %.o, ${SRCS_C}))

CC = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size

DEFINES = -DF_CPU=${F_CPU}
OPTIONS = -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -funsigned-char -funsigned-bitfields
CFLAGS = -mmcu=${MCU} ${DEFINES} ${INCS_DIRS} -std=gnu99 -Wall -g2 -gstabs -Os ${OPTIONS}
LDFLAGS = -Wl,--gc-sections -Wl,-Map,${PRJ}.map

$(foreach S, $(SRCS_C), \
	$(foreach O, $(filter %$(basename $(notdir $S)).o, $(OBJS)), \
		$(eval $O: $S ${INCS_C}) \
	) \
)

%.o:
	$(if $(wildcard $(@D)), , ${MKDIR} $(@D) &&) ${CC} ${CFLAGS} -c $< -o $@

${BUILD_DIR}/${PRJ}.elf: ${OBJS}
	${CC} -mmcu=${MCU} ${LDFLAGS} -o $@ ${OBJS}

firmware.hex: ${BUILD_DIR}/${PRJ}.elf
	${OBJCOPY} -R .eeprom -R .fuse -R .lock -R .signature -O ihex $< $@

all: firmware.hex
	${SIZE} --format=avr --mcu=${MCU} ${BUILD_DIR}/${PRJ}.elf

clean:
	${RM}	${OBJS}
	${RM}	${BUILD_DIR}/${PRJ}.elf
	${RM}	${OBJS_DIRS}
	${RM}	${BUILD_DIR}
	${RM}	${BUILD_DIR}/${PRJ}.map
	${RM}	firmware.hex
