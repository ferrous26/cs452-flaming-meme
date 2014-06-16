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


extern const void* const _DataStart;
extern const void* const _DataKernEnd;
extern const void* const _BssEnd;

extern const void* const _TextStart;
extern const void* const _TextKernEnd;

static void* exit_point = NULL;
static void* exit_sp    = NULL;

static inline void _flush_caches() {
    // Invalidate the I/D-Cache
    asm volatile ("mov r0, #0                \n\t"
                  "mcr p15, 0, r0, c7, c7, 0 \n\t"
		  "mcr p15, 0, r0, c8, c7, 0 \n\t"
		  :
		  :
		  : "r0");
}

#define FILL_SIZE 0x1F
static void _lockdown_icache() {
    register uint r0 asm ("r0") = (uint)&_TextStart & FILL_SIZE;
    register uint r1 asm ("r1") = (uint)&_TextKernEnd;

    // Makes sure we lock up to the lowest point including the end defined here
    asm volatile ("mov      r2, #0                      \n\t"
                  "mcr      p15, 0, r2, c9, c0,  1      \n"
                  "ICACHE_LOOP:                         \n\t"
                  "mcr      p15, 0, r0, c7, c13, 1      \n\t"
                  "add      r0, r0, #32                 \n\t"
                  "and      r3, r0, #0xE0               \n\t"
                  "cmp      r3, #0                      \n\t"
                  "addeq    r2, r2, #1 << 26            \n\t"
                  "mcreq    p15, 0, r2, c9, c0,  1      \n\t"
                  "cmp      r0, r1                      \n\t"
                  "ble      ICACHE_LOOP                 \n\t"
                  "cmp      r3, #0                      \n\t"
                  "addne    r2, r2, #1 << 26            \n\t"
                  "mcrne    p15, 0, r2, c9, c0,  1      \n\t"
                  :
                  : "r" (r0), "r" (r1)
                  : "r2", "r3" );
}

static void _lockdown_dcache() {
    register uint r0 asm ("r0") = (uint)&_DataStart & FILL_SIZE;
    register uint r1 asm ("r1") = (uint)&_DataKernEnd;

    // Makes sure we lock up to the lowest point including the end defined here
    asm volatile ("mov      r2, #0                      \n\t"
                  "mcr      p15, 0, r2, c9, c0,  0      \n"
                  "DCACHE_LOOP:                         \n\t"
                  "ldr      r3, [r0], #32               \n\t"
                  "and      r3, r0, #0xE0               \n\t"
                  "cmp      r3, #0                      \n\t"
                  "addeq    r2, r2, #1 << 26            \n\t"
                  "mcreq    p15, 0, r2, c9, c0,  0      \n\t"
                  "cmp      r0, r1                      \n\t"
                  "ble      DCACHE_LOOP                 \n\t"
                  "cmp      r3, #0                      \n\t"
                  "addne    r2, r2, #1 << 26            \n\t"
                  "mcrne    p15, 0, r2, c9, c0,  0      \n\t"
                  :
                  : "r" (r0), "r" (r1)
                  : "r2", "r3" );
}

static inline void _enable_caches() {
    // Turn on the I-Cache
    asm volatile ("mrc p15, 0, r0, c1, c0, 0 \n\t" //get cache control
                  "orr r0, r0, #0x1000       \n\t" //turn I-Cache
		  "orr r0, r0, #5            \n\t" //turn on mmu and dcache #b0101
	          "mcr p15, 0, r0, c1, c0, 0 \n\t"
		  :
		  :
		  : "r0");
}

static inline void _init() {
    _flush_caches(); // we want to flush caches immediately
    _lockdown_icache();
    _lockdown_dcache();
    _enable_caches();

    clock_t4enable();
    clock_enable();

    uart_init();
    vt_init();
    scheduler_init();
    syscall_init();
    irq_init();
}

void shutdown(void) {
    assert(debug_processor_mode() == SUPERVISOR,
	   "Trying to shutdown from non-supervisor mode");

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

int main() {
    uint* volatile ep;
    uint* volatile sp;

    asm volatile ("mov %0, sp\n"
		  "mov %1, lr\n"
		  : "=r" (sp),
		    "=r" (ep));

    // zero out our base space
    memset((void*)&_DataStart, 0, (uint)&_BssEnd - (uint)&_DataStart);

    exit_point = ep;
#ifdef COWAN
    exit_sp    = sp - 1;
#else
    exit_sp    = sp - 2;
#endif

    _init();

    scheduler_get_next();
    scheduler_activate();

    assert(false, "failed to launch first task");

    return 0;
}
