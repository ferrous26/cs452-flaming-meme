#
# Makefile for cs452
#

# source files
SOURCES_ASM := $(wildcard src/*.asm)
SOURCES_C   := $(wildcard src/*.c) $(wildcard src/tasks/*.c)

# object files
OBJS        := $(patsubst %.asm,%.o,$(SOURCES_ASM))
OBJS        += $(patsubst %.c,%.o,$(SOURCES_C))

ifdef ASSERT
CFLAGS += -D ASSERT
endif

ifdef DEBUG
CFLAGS += -D DEBUG -fverbose-asm
endif

ifdef BENCHMARK
CFLAGS += -D BENCHMARK
endif

ifdef STRICT
CFLAGS += -Werror
endif

ifdef RELEASE
CFLAGS += -O$(RELEASE) -Wuninitialized
endif

CFLAGS += -D __BUILD__=$(shell cat VERSION)
CFLAGS += -D __CODE_NAME__=$(shell cat CODE_NAME)
CFLAGS += -std=gnu99 -fomit-frame-pointer -c -Isrc -I. -Iinclude

CFLAGS += --freestanding -msoft-float
CFLAGS += -Wall -Wextra -Wshadow -Wcast-align -Wredundant-decls
CFLAGS += -Wno-div-by-zero -Wno-multichar -Wpadded
CFLAGS += -Wswitch-enum -Wdisabled-optimization

LDFLAGS += -init main -Map kernel.map -N  --warn-unresolved-symbols

# load platform specific makefile
ifdef PI
include make/pi.makefile
else
include make/arm920.makefile
endif

all: kernel.elf

kernel.elf: $(OBJS)
	$(CC) -S $(CFLAGS) $(KCFLAGS) src/kernel.c -o src/kernel.s
	$(AS) $(ASFLAGS) src/kernel.s -o src/kernel.o
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
	-rm -f kernel.elf kernel.map
	-rm -f src/*.s src/tasks/*.s
	-rm -f src/*.o src/tasks/*.o src/kernel/*.o
