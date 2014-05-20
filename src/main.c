/*
 * main.c - where it all begins
 */

#include <std.h>
#include <io.h>
#include <debug.h>
#include <vt100.h>
#include <clock.h>
#include <syscall.h>
#include <memory.h>
#include <scheduler.h>
#include <task.h>
#include <kernel.h>

static void* exit_point = NULL;

static inline void _init(void* dp) {
    debug_init();
    clock_t4enable();
    uart_init();
    vt_init();
    task_init();
    scheduler_init();

    debug_log("Welcome to ferOS build %u", __BUILD__);
    debug_log("Built %s %s", __DATE__, __TIME__);
    vt_flush();

    *SWI_HANDLER = (void*)kernel_enter + OS_OFFSET;
    exit_point   = dp;
}

// forces gcc to do the right thing (tm)
static void __attribute__ ((noinline)) exit_to_redboot(void* ep) {
    UNUSED(ep);
    asm("mov pc, r0");
}

void _shutdown(void) {
    debug_log("Shutting Down");
    vt_flush();
    exit_to_redboot(exit_point);
}

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    void* dp;
    asm("mov %0, lr" : "=r" (dp));
    _init(dp);

    scheduler_get_next();
    kernel_exit(task_active->sp);

    _shutdown();
    return 0;
}
