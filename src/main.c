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
#include <rand.h>

static void* exit_point = NULL;

static inline void _init_hardware() {
    // Invalidate the I/D-Cache
    asm volatile ("mov r0, #0                \n"
                  "mcr p15, 0, r0, c7, c7, 0 \n"
		  "mcr p15, 0, r0, c8, c7, 0 \n"
	         :
	         :
	         : "r0");
    // Turn on the I-Cache
    asm volatile ("mrc p15, 0, r0, c1, c0, 0 \n" //get cache control
                  "orr r0, r0, #0x1000       \n" //turn I-Cache
		  "orr r0, r0, #5            \n" //turn on mmu and dcache #b0101
	          "mcr p15, 0, r0, c1, c0, 0 \n"
	         :
	         :
	         : "r0");
}

static inline void _init(void* dp) {
    _init_hardware();

    clock_t4enable();
    uart_init();
    vt_init();
    scheduler_init();

    vt_goto(2, 40);
    kprintf("Welcome to ferOS build %u", __BUILD__);
    vt_goto(3, 40);
    kprintf("Built %s %s", __DATE__, __TIME__);
    vt_flush();

// Create a branch instruction
#define B(addr) (0xea000000 | (((int)addr >> 2) - 4))
    *SWI_HANDLER = B(kernel_enter);
    exit_point   = dp;

    srand(__BUILD__);
}

void shutdown(void) {
    vt_log("Shutting Down");
    vt_deinit();
    exit_to_redboot(exit_point);
}

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    void* dp;
    asm volatile ("mov %0, lr" : "=r" (dp));
    _init(dp);

    if (!scheduler_get_next())
        scheduler_activate();

    assert(false, "failed to launch first task");

    return 0;
}
