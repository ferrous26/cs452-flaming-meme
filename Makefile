#
# Makefile for cs452
#
ifdef CLANG
CC = clang
AS = arm-eabi-as
LD = arm-eabi-ld
else ifdef FUTURE
CC = gcc
AS = arm-eabi-as
LD = arm-eabi-ld
else
CC = gcc
AS = as
LD = ld
endif

# source files
SOURCES_ASM := $(wildcard src/*.asm)
SOURCES_C   := $(wildcard src/*.c) $(wildcard src/tasks/*.c)

# object files
OBJS        := $(patsubst %.asm,%.o,$(SOURCES_ASM))
OBJS        += $(patsubst %.c,%.o,$(SOURCES_C))

CFLAGS  = # reset
ASFLAGS = # reset
LDFLAGS = # reset

ifdef RELEASE
CFLAGS += -O$(RELEASE)

ifdef CLANG
CFLAGS +=
else ifdef FUTURE
CFLAGS += -fpeel-loops
else
CFLAGS += -unswitch-loops -fpeel-loops -floop-optimize2
endif
endif

ifdef ASSERT
CFLAGS += -D ASSERT
endif

ifdef DEBUG
ifdef COWAN
CFLAGS  += -Og
else
CFLAGS  += -g
endif
CFLAGS  += -D DEBUG -fverbose-asm
ASFLAGS += -mapcs-32 # -mapcs: always generate a complete stack frame
endif

ifdef BENCHMARK
CFLAGS += -D BENCHMARK
endif

ifdef STRICT
CFLAGS += -Werror
endif

CFLAGS += -D __BUILD__=$(shell cat VERSION) -std=gnu99 -fomit-frame-pointer
CFLAGS += -c -I. -Iinclude -mcpu=arm920t

LDFLAGS = -init main -Map kernel.map -N -T orex.ld --warn-unresolved-symbols


ifdef CLANG
CFLAGS += -target armv4--eabi -ffreestanding -D CLANG
CFLAGS += -Weverything -Wno-language-extension-token -Wno-gnu-zero-variadic-macro-arguments

LDFLAGS += -L/u3/marada/arm-eabi-gcc/lib/gcc/arm-eabi/4.9.0

else

ifdef FUTURE
CFLAGS += -D FUTURE
else
CFLAGS += -D COWAN
endif

CFLAGS += --freestanding -msoft-float
CFLAGS += -Wall -Wextra -Wshadow -Wcast-align -Wredundant-decls
CFLAGS += -Wno-div-by-zero -Wno-multichar -Wpadded
CFLAGS += -Wswitch-enum -Wdisabled-optimization

ASFLAGS	+= -mcpu=arm920t

ifdef FUTURE
LDFLAGS += -L/u3/marada/arm-eabi-gcc/lib/gcc/arm-eabi/4.9.0
else
LDFLAGS += -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2
endif
endif


all: kernel.elf

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc

# C.
%.o: %.c Makefile
	$(CC) -S $(CFLAGS) $< -o $(<:.c=.s)
	$(AS) $(ASFLAGS) $(<:.c=.s) -o $@

# AS.
%.o: %.asm Makefile
	$(AS) $(ASFLAGS) $< -o $@


.PHONY: clean

clean:
	-rm -f src/kernel.elf src/kernel.map
	-rm -f src/*.s src/tasks/*.s
	-rm -f src/*.o src/tasks/*.o
