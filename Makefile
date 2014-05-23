#
# Makefile for cs452
#

ifdef PI
PREFIX ?= /usr
XCC     = $(PREFIX)/bin/arm-none-eabi-gcc
AS      = $(PREFIX)/bin/arm-none-eabi-as
LD      = $(PREFIX)/bin/arm-none-eabi-ld
OBJCOPY = $(PREFIX)/bin/arm-none-eabi-objcopy
else
XCC = gcc
AS  = as
LD  = ld
endif

# source files
SOURCES_ASM := $(wildcard src/*.asm)
SOURCES_C   := $(wildcard src/*.c) $(wildcard src/tasks/*.c)

# object files
OBJS        := $(patsubst %.asm,%.o,$(SOURCES_ASM))
OBJS        += $(patsubst %.c,%.o,$(SOURCES_C))

ifdef RELEASE
# Flags to look into trying:
#-ffast-math -ftree-vectorize -fmerge-all-constants -ftree-loop-linear -ftracer
# -fmodulo-sched -fgcse-sm -fgcse-las -fgcse-after-reload -ftree-loop-linear
# -ftree-loop-im
CFLAGS  = -O$(RELEASE) -fpeel-loops -ftracer -floop-optimize2
else
CFLAGS  = -g -D ASSERT
# -mapcs: always generate a complete stack frame

endif

ifdef BENCHMARK
CFLAGS += -D BENCHMARK
endif

ifdef PI
CFLAGS  += -mcpu=arm1176jzf-s -D PI
ASFLAGS	 = -mcpu=arm1176jzf-s
else
CFLAGS  += -mcpu=arm920t
ASFLAGS	 = -mcpu=arm920t -mapcs-32
endif

CFLAGS += -D __BUILD__=$(shell cat VERSION) -std=gnu99
CFLAGS += -c -fPIC -I. -Iinclude -msoft-float --freestanding
CFLAGS += -Wall -Wextra -Werror -Wshadow -Wcast-align -Wredundant-decls
CFLAGS += -Wno-div-by-zero -Wno-multichar -Wpadded -Wunreachable-code
CFLAGS += -Wswitch-enum

ARFLAGS = rcs

LDFLAGS  = -init main -Map kernel.map -N -Llib

ifdef PI
LDFLAGS += -Tlink-arm-eabi.ld
else
LDFLAGS += -Torex.ld -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2
endif

# TODO: proper separation of kernel and user code
KERN_CODE = src/main.o src/context.o src/syscall.o
TASKS     = src/tasks/bootstrap.o

all: kernel.elf

kernel.img: kernel-pi.elf
	$(OBJCOPY) kernel-pi.elf -O binary kernel.img

kernel-pi.elf: $(OBJS) link-arm-eabi.ld
	$(LD) $(OBJS) -Tlink-arm-eabi.ld -o $@

kernel.elf: $(OBJS) orex.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc

# C.
%.o: %.c Makefile
	$(XCC) -S $(CFLAGS) $< -o $(<:.c=.s)
	$(AS) $(ASFLAGS) $(<:.c=.s) -o $@

# AS.
%.o: %.asm Makefile
	$(AS) $(ASFLAGS) $< -o $@


.PHONY: local remote clean pi

pi:
	ssh pi-builder "cd /mnt/hgfs/Documents/cs452 && make clean && PI=yup make kernel.img"
	cp kernel.img /Volumes/PIOS/kernel.img
	diskutil unmountDisk /Volumes/PIOS

remote:
	rsync -trulip --exclude '.git/' --exclude './measurement' ./ uw:$(UW_HOME)/trains
	ssh uw "cd trains/ && make clean && touch Makefile && RELEASE=$(RELEASE) BENCHMARK=$(BENCHMARK) make -s -j16"
	ssh uw "cd trains/ && cp kernel.elf /u/cs452/tftp/ARM/$(UW_USER)/micro.elf"

clean:
	-rm -f kernel.elf kernel.map kernel.img kernel-pi.elf
	-rm -f src/*.s
	-rm -f src/*.a
	-rm -f src/*.o
	-rm -f lib/*.a
