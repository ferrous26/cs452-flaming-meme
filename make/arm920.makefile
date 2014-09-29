CC = gcc
AS = as
LD = ld

SOURCES_ASM += $(wildcard src/kernel/arch/arm920/*.asm)

ifdef RELEASE
KCFLAGS = -O3
endif

ifdef DEBUG
ASFLAGS += -mapcs-32 # -mapcs: always generate a complete stack frame
endif

CFLAGS  += -mcpu=arm920t -D ARM920
ASFLAGS	+= -mcpu=arm920t
LDFLAGS += -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -T ld/arm920.ld
