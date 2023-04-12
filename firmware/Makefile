# Makefile
#
# targets:
#   all:          compile release version
#   debug:        compile debug version
#   test:         test the connection to the MCU
#   prog:         write compiled hex file to the MCU's flash memory
#   fuse:         write the fuse bytes to the MCU
#   eeprom_read:  read eeprom content
#   eeprom_write: write eeprom content
#   disasm:       disassemble the code for debugging
#   clean:        remove all build files

TARGET = ignitor
MCU = atmega48
CLK = 8000000

RM = rm -rf
MKDIR = mkdir -p

CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
SIZE = avr-size --format=avr --mcu=$(MCU)

DUDE_PRG = usbasp
DUDE_MCU = m48
AVRDUDE = avrdude -c $(DUDE_PRG) -p $(DUDE_MCU)

# Lookup http://www.engbedded.com/fusecalc/ for fuse values
LFU = 0xE2
HFU = 0xDF
EFU = 0x01

$(info $(TARGET) firmware)

PWD := $(strip $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))

SUBDIR = $(PWD)/drv
BUILD = $(PWD)/../build-$(TARGET)-firmware

HEX = $(BUILD)/$(TARGET).hex
ELF = $(BUILD)/$(TARGET).elf
MAP = $(BUILD)/$(TARGET).map
EEP = $(BUILD)/$(TARGET).eep

INCS = $(wildcard $(PWD)/*.h $(foreach fd, $(SUBDIR), $(fd)/*.h))
SRCS = $(wildcard $(PWD)/*.c $(foreach fd, $(SUBDIR), $(fd)/*.c))
OBJS = $(patsubst $(PWD)/%.c, $(BUILD)/%.o, $(SRCS))

INCLUDES = -I$(PWD) $(addprefix -I, $(SUBDIR))
DEFINES = -DF_CPU=$(CLK) -DTIMER_SIMPLE_0= -DTIMER_SIMPLE_2= -DTIMER_METER_1=
OPTIONS = -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -funsigned-char -funsigned-bitfields -fomit-frame-pointer

LDFLAGS = -Wl,--gc-sections -Wl,-Map,$(MAP)
CFLAGS = -Wall -mmcu=$(MCU) -std=gnu99 $(DEFINES) $(INCLUDES) $(OPTIONS)

.PHONY: all

all: CFLAGS += -Os
all:  $(HEX) $(EEP)

debug: CFLAGS += -g -O0
debug: $(HEX) $(EEP)

$(BUILD)/%.o: $(PWD)/%.c $(INCS)
	$(if $(wildcard $(@D)), , ${MKDIR} $(@D))
	$(CC) $< -c $(CFLAGS) -o $@

$(ELF): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LDFLAGS) -o $@

$(HEX): $(ELF)
	rm -f $(HEX)
	$(OBJCOPY) -j .text -j .data -R .eeprom -R .fuse -R .lock -R .signature -O ihex $(ELF) $(HEX)
	$(SIZE) $(ELF)

$(EEP): $(ELF)
	$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0 --no-change-warnings -O ihex $< $@

test:
	$(AVRDUDE) -v

prog: $(HEX)
	$(AVRDUDE) -U flash:w:$(HEX)

fuse:
	$(AVRDUDE) -U lfuse:w:$(LFU):m -U hfuse:w:$(HFU):m -U efuse:w:$(EFU):m

eeprom_read:
	$(AVRDUDE) -U eeprom:r:$(EEP):i

eeprom_write: $(EEP)
	$(AVRDUDE) -U eeprom:w:$(EEP)

disasm: $(ELF)
	$(OBJDUMP) -d $(ELF)

clean:
	$(RM) $(HEX) $(ELF) $(EEP) $(MAP) $(OBJS)
	$(RM) $(BUILD)
