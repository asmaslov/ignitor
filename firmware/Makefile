PRJ = ignitor
MCU = atmega48
F_CPU = 8000000

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

DEFINES = -DF_CPU=${F_CPU} -DREMOTE=
OPTIONS = -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -funsigned-char -funsigned-bitfields
CFLAGS = -mmcu=${MCU} ${DEFINES} ${INCS_DIRS} -std=gnu99 -Wall -g2 -gstabs -Os ${OPTIONS}
LDFLAGS = -Wl,--gc-sections -Wl,-Map,${BUILD_DIR}/${PRJ}.map

AVRDUDE = avrdude
AVRDUDE_PROGRAMMER = usbasp
AVRDUDE_FLAGS = -p$(MCU) -c$(AVRDUDE_PROGRAMMER) -v
AVRDUDE_WRITE_FLASH = -U flash:w:${BUILD_DIR}/${PRJ}.hex
AVRDUDE_WRITE_EEPROM = -U eeprom:w:${BUILD_DIR}/${PRJ}.eep

$(foreach S, $(SRCS_C), \
	$(foreach O, $(filter %$(basename $(notdir $S)).o, $(OBJS)), \
		$(eval $O: $S ${INCS_C}) \
	) \
)

%.o:
	$(if $(wildcard $(@D)), , ${MKDIR} $(@D) &&) ${CC} ${CFLAGS} -c $< -o $@

${BUILD_DIR}/${PRJ}.elf: ${OBJS}
	${CC} -mmcu=${MCU} ${LDFLAGS} -o $@ ${OBJS}

${BUILD_DIR}/${PRJ}.hex: ${BUILD_DIR}/${PRJ}.elf
	${OBJCOPY} -R .eeprom -R .fuse -R .lock -R .signature -O ihex $< $@

${BUILD_DIR}/${PRJ}.eep: ${BUILD_DIR}/${PRJ}.elf
	$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" \
	--change-section-lma .eeprom=0 --no-change-warnings -O ihex $< $@

all: ${BUILD_DIR}/${PRJ}.hex ${BUILD_DIR}/${PRJ}.eep
	${SIZE} --format=avr --mcu=${MCU} ${BUILD_DIR}/${PRJ}.elf

program: ${BUILD_DIR}/${PRJ}.hex ${BUILD_DIR}/${PRJ}.eep
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_WRITE_FLASH) $(AVRDUDE_WRITE_EEPROM)

clean:
	${RM}	${OBJS}
	${RM}	${OBJS_DIRS}
	${RM}	${BUILD_DIR}/${PRJ}.elf
	${RM}	${BUILD_DIR}/${PRJ}.map
	${RM}	${BUILD_DIR}/${PRJ}.hex
	${RM}	${BUILD_DIR}/${PRJ}.eep
	${RM}	${BUILD_DIR}
