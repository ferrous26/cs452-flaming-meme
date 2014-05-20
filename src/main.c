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

static inline void _init(void* dp) {
    debug_init(dp);
    clock_t4enable();
    uart_init();
    vt_init();
    task_init();
    scheduler_init();

    debug_log("Welcome to ferOS build %u", __BUILD__);
    debug_log("Built %s %s", __DATE__, __TIME__);
    vt_flush();

    *SWI_HANDLER = (void*)kernel_enter + 0x217000;
}

// static inline void _shutdown(void) {
//     debug_log("Shutting Down");
//     vt_flush();
// }

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    void* dp;
    asm("mov %0, lr" : "=r" (dp));
    _init(dp);

    for (;;) {
	debug_task(0);
	vt_flush();

	scheduler_get_next();
	scheduler_activate();
    }

    // shutdown various systems
    // _shutdown();

    // return 0;
}
