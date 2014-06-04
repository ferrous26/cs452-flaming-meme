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

ifdef RELEASE
# Flags to look into trying:
CFLAGS   = -O$(RELEASE) -unswitch-loops -fpeel-loops -floop-optimize2 -fomit-frame-pointer
#CFLAGS += --param max-gcse-memory=1073741824 -fmerge-all-constants -fmodulo-sched -ffast-math
#CFLAGS += -ftree-loop-linear
#CFLAGS += -fgcse-after-reload -fgcse-sm -fgcse-las -ftree-loop-im
else
CFLAGS  = -g -D ASSERT -D DEBUG -fverbose-asm
# -mapcs: always generate a complete stack frame
ASFLAGS = -mapcs-32
endif

ifdef BENCHMARK
CFLAGS += -D BENCHMARK
endif

CFLAGS += -D __BUILD__=$(shell cat VERSION) -std=gnu99
CFLAGS += -c -I. -Iinclude -mcpu=arm920t -msoft-float --freestanding
CFLAGS += -Wall -Wextra -Wshadow -Wcast-align -Wredundant-decls
CFLAGS += -Wno-div-by-zero -Wno-multichar -Wpadded -Wunreachable-code
CFLAGS += -Wswitch-enum -Wdisabled-optimization

ifdef STRICT
CFLAGS += -Werror
endif

ASFLAGS	+= -mcpu=arm920t

ARFLAGS = rcs

LDFLAGS  = -init main -Map kernel.map -N -T orex.ld
LDFLAGS += -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2

# TODO: proper separation of kernel and user code
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
	ssh uw "cd trains/ && make clean && touch Makefile && STRICT=$(STRICT) RELEASE=$(RELEASE) BENCHMARK=$(BENCHMARK) make -s -j16"
	ssh uw "cd trains/ && cp kernel.elf /u/cs452/tftp/ARM/$(UW_USER)/k3.elf"

clean:
	-rm -f src/kernel.elf src/kernel.map
	-rm -f src/**/*.s
	-rm -f src/**/*.a
	-rm -f src/**/*.o
	-rm -f lib/**/*.a
