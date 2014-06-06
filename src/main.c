/*
 * main.c - where it all begins
 */

#include <std.h>
#include <debug.h>
#include <vt100.h>
#include <clock.h>
#include <syscall.h>
#include <memory.h>
#include <scheduler.h>
#include <kernel.h>
#include <irq.h>

extern void* _BssStart;
extern void* _BssEnd;

static void* exit_point = NULL;
static void* exit_sp    = NULL;

static inline void _flush_caches() {
    // Invalidate the I/D-Cache
    asm volatile ("mov r0, #0                \n"
                  "mcr p15, 0, r0, c7, c7, 0 \n"
		  "mcr p15, 0, r0, c8, c7, 0 \n"
		  :
		  :
		  : "r0");
}

static inline void _enable_caches() {
    // Turn on the I-Cache
    asm volatile ("mrc p15, 0, r0, c1, c0, 0 \n" //get cache control
                  "orr r0, r0, #0x1000       \n" //turn I-Cache
		  "orr r0, r0, #5            \n" //turn on mmu and dcache #b0101
	          "mcr p15, 0, r0, c1, c0, 0 \n"
		  :
		  :
		  : "r0");
}

static inline void _init() {
    _flush_caches(); // we want to flush caches immediately

    clock_t4enable();
    clock_enable();

    uart_init();
    vt_init();
    scheduler_init();
    syscall_init();
    irq_init();

    _enable_caches();

    vt_goto(2, 40);
    kprintf("Welcome to ferOS build %u", __BUILD__);
    vt_goto(3, 40);
    kprintf("Built %s %s", __DATE__, __TIME__);

    vt_flush();
}

void shutdown(void) {

    assert(debug_processor_mode() == SUPERVISOR,
	   "Trying to shutdown from non-supervisor mode");

    vt_log("Shutting Down");
    vt_deinit();
    _flush_caches();

    syscall_deinit();
    irq_deinit();

    asm volatile ("mov  sp, %0\n"
		  "mov	pc, %1\n"
		  :
		  : "r" (exit_sp),
		    "r" (exit_point));
}

void main(void);
void main() {

    uint* volatile ep;
    uint* volatile sp;

    asm volatile ("mov %0, sp\n"
		  "mov %1, lr\n"
		  : "=r" (sp),
		    "=r" (ep));

    // zero out our base space
    memset(&_BssStart, 0, (uint)&_BssEnd - (uint)&_BssStart);

    exit_point = ep;
    exit_sp    = sp - 1;

    _init();

    scheduler_get_next();
    scheduler_activate();

    assert(false, "failed to launch first task");

    return;
}
