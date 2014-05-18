/*
 * main.c - where it all begins
 */

#include <std.h>
#include <io.h>
#include <debug.h>
#include <vt100.h>
#include <clock.h>
#include <syscall.h>
#include <debug.h>
#include <memory.h>
#include <scheduler.h>



static void _init(void) {
    clock_t4enable();
    debug_init();
    uart_init();
    vt_init();

    debug_log("Welcome to ferOS build %u", __BUILD__);
    debug_log("Built %s %s", __DATE__, __TIME__);
    vt_flush();

    *SWI_HANDLER = (void*)kernel_enter;
}

static void _shutdown(void) {
    debug_log("Shutting Down");
    vt_flush();
}

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    _init();



    //    scheduler_init();
    //    task* tsk = scheduler_schedule();
    // scheduler_activate(tsk);

    // shutdown various systems
    _shutdown();

    return 0;
}
