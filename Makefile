#
# Makefile for cs452
#
XCC = gcc
AS  = as
LD  = ld

# source files
SOURCES_ASM := $(wildcard src/*.asm)
SOURCES_C   := $(wildcard src/*.c) $(wildcard src/tasks/*.c)

# object files
OBJS        := $(patsubst %.asm,%.o,$(SOURCES_ASM))
OBJS        += $(patsubst %.c,%.o,$(SOURCES_C))

ifeq ($(RELEASE), 1)
CFLAGS = -O2 -fpeel-loops -ftracer
#-ffast-math -ftree-vectorize -floop-optimize2 -ftree-loop-linear -ftracer
else
# -g: include hooks for gdb
CFLAGS = -g
endif

CFLAGS += -D __BUILD__=$(shell cat VERSION)
CFLAGS += -c -fPIC -I. -Iinclude -mcpu=arm920t -msoft-float --freestanding
CFLAGS += -Wall -Wextra -Werror -Wshadow -Wcast-align -Wredundant-decls
CFLAGS += -Wno-div-by-zero -Wno-multichar -Wpadded -Wunreachable-code
CFLAGS += -Wswitch-enum


ASFLAGS	= -mcpu=arm920t -mapcs-32
# -mapcs: always generate a complete stack frame

ARFLAGS = rcs

LDFLAGS  = -init main -Map kernel.map -N -T orex.ld
LDFLAGS += -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -Llib

KERN_CODE = src/main.o src/context.o src/syscall.o
TASKS     = src/tasks/bootstrap.o

all: kernel.elf

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc

# C.
%.o: %.c Makefile
	$(XCC) -S $(CFLAGS) $< -o $(<:.c=.s)
	$(AS) $(ASFLAGS) $(<:.c=.s) -o $@

# AS.
%.o: %.asm Makefile
	$(AS) $(ASFLAGS) $< -o $@


.PHONY: local remote clean

remote:
	rsync -trulip --exclude '.git/' --exclude './measurement' ./ uw:$(UW_HOME)/trains
	ssh uw "cd trains/ && make clean && RELEASE=$(RELEASE) make -s -j16"
	ssh uw "cd trains/ && cp kernel.elf /u/cs452/tftp/ARM/$(UW_USER)/micro.elf"

clean:
	-rm -f src/kernel.elf src/kernel.map
	-rm -f src/*.s
	-rm -f src/*.a
	-rm -f src/*.o
	-rm -f lib/*.a
