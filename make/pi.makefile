PREFIX ?= /usr
CC      = $(PREFIX)/bin/arm-none-eabi-gcc
AS      = $(PREFIX)/bin/arm-none-eabi-as
LD      = $(PREFIX)/bin/arm-none-eabi-ld
OBJCOPY = $(PREFIX)/bin/arm-none-eabi-objcopy

ifdef RELEASE
CFLAGS += -fpeel-loops -unswitch-loops -floop-optimize2
else
CFLAGS += -Og
endif

CFLAGS += -Wsuggest-attribute=format -Wmissing-format-attribute
CFLAGS += -Wsuggest-attribute=pure -Wsuggest-attribute=const
CFLAGS += -Wsuggest-attribute=noreturn -Wunreachable-code

CFLAGS  += -march=armv6t2 -mtune=arm1176jzf-s -D PI
ASFLAGS += -mcpu=arm1176jzf-s
LDFLAGS += -T ld/pi.ld

# TODO: task for autolading the pi elf onto the SD card
