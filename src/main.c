/*
 * main.c - where it all begins
 */

#include <std.h>
#include <vt100.h>
#include <kernel.h>
#include <kernel/arch/arm920/irq.h>
#include <kernel/arch/arm920/clock.h>
#include <kernel/arch/arm920/io.h>
#include <kernel/arch/arm920/cache.h>
#include <kernel/arch/arm920/cpu.h>

extern const void* const _DataStart;
extern const void* const _BssEnd;

static void* exit_point = NULL;
static void* exit_sp    = NULL;

static TEXT_COLD void _init() {
    _flush_caches(); // we want to flush caches immediately
    _lockdown_icache();
    _lockdown_dcache();
    _enable_caches();

    clock_t4enable();
    uart_init();
    vt_init();
    kernel_init();
    irq_init();
    clock_init();

    scheduler_first_run(); // go go go
}

void TEXT_COLD shutdown() {
    assert(processor_mode() == SUPERVISOR,
	   "Trying to shutdown from non-supervisor mode");

    vt_deinit();
    clock_deinit();
    kernel_deinit();
    irq_deinit();
    _flush_caches();

    asm volatile (
                  // unlock cache lines for Dididier/Kelsy
                  "mcr      p15, 0, r2, c9, c0,  1      \n\t"
                  "mcr      p15, 0, r2, c9, c0,  0      \n\t"

                  "mov  sp, %0\n"
                  "mov	pc, %1\n"
                  :
                  : "r" (exit_sp),
                    "r" (exit_point));

    FOREVER;
}

int main(int argc, char** argv) TEXT_COLD;
int main(int argc, char** argv) {
    uint* volatile ep;
    uint* volatile sp;

    asm volatile ("mov %0, sp\n"
		  "mov %1, lr\n"
		  : "=r" (sp),
		    "=r" (ep));

    // zero out our base space
    memset((void*)&_DataStart, 0, (uint)&_BssEnd - (uint)&_DataStart);

    exit_point = ep;
// So...none of this works completely...it will get the stack pointer
// close to where it should be, but will leak a varying amount of
// memory depending on the build type...release builds currently do
// not leak.
#ifdef COWAN
    exit_sp    = sp - 1;
#else
    exit_sp    = sp - 2;
#endif

    _init();

    assert(false, "failed to launch first task");

    UNUSED(argc);
    UNUSED(argv);
    return 0;
}
