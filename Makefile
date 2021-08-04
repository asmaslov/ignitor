PRJ = ignitor
MCU = atmega48
F_CPU = 20000000

RM = rm -rf
MKDIR = mkdir -p
SRC_DIR = src
BUILD_DIR = build
DIST_DIR = dist

SRCS_C = $(shell find ${SRC_DIR} -type f -name *.c)
INCS_C = $(shell find ${SRC_DIR} -type f -name *.h)
H2PY_PY = $(patsubst %.h, h2py_%.py, $(notdir ${INCS_C}))
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

${PRJ}.hex: ${BUILD_DIR}/${PRJ}.elf
	${OBJCOPY} -R .eeprom -R .fuse -R .lock -R .signature -O ihex $< $@

ui_${PRJ}.py: ${PRJ}.ui
	pyuic5 $< -o $@

${PRJ}_rc.py: ${PRJ}.qrc
	pyrcc5 $< -o $@

$(foreach H, $(INCS_C), \
	$(foreach P, $(filter %h2py_$(basename $(notdir $H)).py, $(H2PY_PY)), \
		$(eval $P: $H) \
	) \
)

h2py_%.py:
	python h2py.py $<

all: ${PRJ}.hex ui_${PRJ}.py ${PRJ}_rc.py ${H2PY_PY}
	${SIZE} --format=avr --mcu=${MCU} ${BUILD_DIR}/${PRJ}.elf

app: ${PRJ}.py ${PRJ}_rc.py ${H2PY_PY}
	pyinstaller --hidden-import ui_ignitor --onefile --windowed --icon=${PRJ}.ico ${PRJ}.py

clean:
	${RM}	${OBJS}
	${RM}	${BUILD_DIR}/${PRJ}.elf
	${RM}	${OBJS_DIRS}
	${RM}	${BUILD_DIR}
	${RM}	${PRJ}.hex
	${RM}	${PRJ}.map
	${RM}	ui_${PRJ}.py
	${RM}	${PRJ}_rc.py
	${RM}	${H2PY_PY}
	${RM}	${DIST_DIR}
