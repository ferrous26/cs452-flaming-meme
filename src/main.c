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
    clock_t4enable();
    uart_init();
    vt_init();
    task_init();
    scheduler_init();

    vt_goto(2, 40);
    kprintf("Welcome to ferOS build %u", __BUILD__);
    vt_goto(3, 40);
    kprintf("Built %s %s", __DATE__, __TIME__);
    vt_flush();

    *SWI_HANDLER = START_ADDRESS(kernel_enter);
    exit_point   = dp;
}

void shutdown(void) {
    vt_log("Shutting Down");
    vt_flush();

    exit_to_redboot(exit_point);
}

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    void* dp;
    asm("mov %0, lr" : "=r" (dp));
    _init(dp);

    while (!scheduler_get_next()) {
        scheduler_activate();
    }

    shutdown();

    return 0;
}
