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

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    // startup various systems
    clock_t4enable();
    debug_init();
    uart_init();
    vt_init();

    print_startup_message();

    *SWI_HANDLER = (void*)syscall_enter;
    debug_cpsr();

    vt_flush();
    unsigned int ret = myTid();
    kprintf_char('\n');
    kprintf_ptr((void*)ret);

    // TODO: main loop goes here!
    // startup_idle_task()
    // go into scheduling loop

    // shutdown various systems
    vt_flush();

    return 0;
}
