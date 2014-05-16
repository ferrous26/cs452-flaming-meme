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

ifeq ($(RELEASE), 1)
# -g: include hooks for gdb
CFLAGS = -O2
else
CFLAGS = -g
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

kernel.elf: $(OBJS) src/main.o src/context.o libdebug libscheduler libmemory libcircular libio libvt100 libclock
	$(LD) $(LDFLAGS) -o $@ src/main.o src/context.o -lscheduler -ldebug -lvt100 -lio -lcircular -lclock -lmemory -lgcc

libio: src/io.o
	$(AR) $(ARFLAGS) lib/libio.a src/io.o

libcircular: src/circular_buffer.o
	$(AR) $(ARFLAGS) lib/libcircular.a src/circular_buffer.o

libmemory: src/memory.o
	$(AR) $(ARFLAGS) lib/libmemory.a src/memory.o

libvt100: src/vt100.o
	$(AR) $(ARFLAGS) lib/libvt100.a src/vt100.o

libclock: src/clock.o
	$(AR) $(ARFLAGS) lib/libclock.a src/clock.o

libscheduler: src/scheduler.o
	$(AR) $(ARFLAGS) lib/libscheduler.a src/scheduler.o

libdebug: src/debug.o
	$(AR) $(ARFLAGS) lib/libdebug.a src/debug.o

libclock: src/clock.o
	$(AR) $(ARFLAGS) lib/libclock.a src/clock.o

# C.
%.o: %.c Makefile
	$(XCC) -S $(CFLAGS) $< -o $(<:.c=.s)
	$(AS) $(ASFLAGS) $(<:.c=.s) -o $@

# AS.
%.o: %.asm Makefile
	$(AS) $(ASFLAGS) $< -o $@


.PHONY: local remote clean

local:
	make clean
	make
	cp kernel.elf /u/cs452/tftp/ARM/marada/kernel.elf

remote:
	rsync -truliph --stats --exclude '.git/' ./ uw:$(UW_HOME)/trains
	ssh uw "cd trains/ && make clean && make"
	ssh uw "cd trains/ && cp kernel.elf /u/cs452/tftp/ARM/$(UW_USER)/micro.elf"

clean:
	-rm -f src/kernel.elf src/kernel.map
	-rm -f src/*.s
	-rm -f src/*.a
	-rm -f src/*.o
	-rm -f lib/*.a
