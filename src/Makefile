DEVICE		= stm32l100rc
OPENCM3_DIR	= ../libopencm3

OBJS		+= main.o debug.o led.o button.o interrupt.o wifi.o ring_buffer.o conf.o data.o

CFLAGS		+= -mcpu=cortex-m3 -mthumb -Os -ggdb3 -std=gnu99
CFLAGS		+= -fno-common -fno-builtin
CFLAGS		+= -Wall -Wextra -Wshadow
CFLAGS		+= -Wall -Wextra -Wimplicit-function-declaration -Wredundant-decls -Wstrict-prototypes -Wundef -Wshadow
CFLAGS		+= -ffunction-sections -fdata-sections
CPPFLAGS	+= -MD
LDFLAGS		+= -Wl,--gc-sections
LDFLAGS		+= -static -nostartfiles
LDFLAGS		+= -specs=nosys.specs -specs=nano.specs
LDLIBS		+= -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

.PHONY: clean all console debug openocd watch download

all: main.elf

bin: main.bin
elf: main.elf
hex: main.hex
download: bin

clean:
	$(Q)$(RM) -rf *.{o,d,elf,bin,hex,srec,list}

console:
	$(Q)stty -f /dev/cu.SLAB_USBtoUART 115200 2>/dev/null | cat /dev/cu.SLAB_USBtoUART

debug:
	$(Q)$(PREFIX)-gdb -tui --eval-command="target remote :3333" main.elf

openocd:
	$(Q)openocd -f ../openocd.cfg

watch:
	$(Q)ls -1 *.{c,h} | entr make

download:
	$(Q)dfu-util -d 0483:df11 -s 0x08000000 -a 0 -D main.bin

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk
