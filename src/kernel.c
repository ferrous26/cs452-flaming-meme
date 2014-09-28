// select the correct hardware specific things
#ifdef ARM920
#include <kernel/arch/arm920/exit.c>
#endif
#ifdef PI
#include <kernel/arch/pi/exit.c>
#endif

// generic kernel stuff
#include <kernel/task.c>
#include <kernel/scheduler.c>
#include <kernel/syscall.c>
#include <kernel/abort.c>

#ifdef ARM920
#include <kernel/arch/arm920/irq.c>
#include <kernel/arch/arm920/clock.c>
#include <kernel/arch/arm920/io.c>
#include <kernel/arch/arm920/cache.c>
#include <kernel/arch/arm920/cpu.c>
#endif
#ifdef PI
#include <kernel/arch/pi/irq.c>
#include <kernel/arch/pi/clock.c>
#include <kernel/arch/pi/io.c>
#include <kernel/arch/pi/cache.c>
#include <kernel/arch/pi/cpu.c>
#endif
