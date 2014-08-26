// select the correct hardware specific shits
#include <kernel/arch/arm920/exit.c>

#include <kernel/task.c>
#include <kernel/scheduler.c>
#include <kernel/syscall.c>
#include <kernel/abort.c>

#include <kernel/arch/arm920/irq.c>    // generic IRQ stuff
#include <kernel/arch/arm920/clock.c>
#include <kernel/arch/arm920/io.c>
#include <kernel/arch/arm920/cache.c>
#include <kernel/arch/arm920/cpu.c>
