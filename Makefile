#
# Makefile for cs452
#
XCC = gcc
AS  = as
LD  = ld

# source files
SOURCES_ASM := $(wildcard src/*.s)
SOURCES_C   := $(wildcard src/*.c)

# object files
OBJS        := $(patsubst %.S,%.o,$(SOURCES_ASM))
OBJS        += $(patsubst %.c,%.o,$(SOURCES_C))

ifeq ($(DEBUG), 1)
# -g: include hooks for gdb
CFLAGS = -g
else
# CFLAGS = -O2
endif

CFLAGS += -D __BUILD__=$(shell cat VERSION)
CFLAGS += -c -fPIC -I. -Iinclude -mcpu=arm920t -msoft-float --freestanding
CFLAGS += -Wall -Wextra -Werror -Wshadow -Wcast-align -Wredundant-decls
CFLAGS += -Winline -Wfloat-equal -Wno-multichar -Wno-div-by-zero


ASFLAGS	= -mcpu=arm920t -mapcs-32
# -mapcs: always generate a complete stack frame

ARFLAGS = rcs

LDFLAGS  = -init main -Map kernel.map -N -T orex.ld
LDFLAGS += -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -Llib


all: kernel.elf

kernel.elf: $(OBJS) src/main.o libmemory libcircular libio
	$(LD) $(LDFLAGS) -o $@ src/main.o -lio -lcircular -lmemory -lgcc

libio: src/io.o
	$(AR) $(ARFLAGS) lib/libio.a src/io.o

libcircular: src/circular_buffer.o
	$(AR) $(ARFLAGS) lib/libcircular.a src/circular_buffer.o

libmemory: src/memory.o
	$(AR) $(ARFLAGS) lib/libmemory.a src/memory.o

libclock: src/clock.o
	$(AR) $(ARFLAGS) lib/libclock.a src/clock.o

# C.
%.o: %.c Makefile
	$(XCC) $(CFLAGS) -c $< -o $@

%.s: %.c Makefile
	$(XCC) -S $(CFLAGS) $< -o $@

# AS.
%.o: %.S Makefile
	$(AS) $(ASFLAGS) -c $< -o $@


.PHONY: local remote clean

local:
	make clean
	make
	cp kernel.elf /u/cs452/tftp/ARM/marada/kernel.elf

remote:
	rsync -truliph --stats --exclude '.git/' ./ uw:/$(UW_HOME)/trains
	ssh uw 'whoami; pwd; echo $$PATH'
	ssh uw "cd trains/ && make clean && make"
	ssh uw "cd trains/ && cp kernel.elf /u/cs452/tftp/ARM/$(UW_USER)/micro.elf"

clean:
	-rm -f src/kernel.elf src/kernel.map
	-rm -f src/*.s
	-rm -f src/*.a
	-rm -f src/*.o
