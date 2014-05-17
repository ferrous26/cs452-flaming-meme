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

static void print_startup_message(void) {
    debug_log("Welcome to ferOS build %u", __BUILD__);
    debug_log("Built %s %s", __DATE__, __TIME__);
}

static void _init() {
    print_startup_message();
    vt_flush();

    *SWI_HANDLER = (void*)syscall_enter;

    clock_t4enable();
    debug_init();
    uart_init();
    vt_init();
}

static void _shutdown() {
    kprintf_string( "Shutting Down\n" );
    vt_flush();
}

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    // startup various systems
    _init();

    debug_cpsr();
    vt_flush();

    /*
    unsigned int ret = myTid();
    kprintf_char('\n');
    kprintf_ptr((void*)ret);
    */

    scheduler_init();
    
    task* tsk = scheduler_schedule();
    scheduler_activate(tsk);

    // shutdown various systems
    _shutdown();

    return 0;
}
