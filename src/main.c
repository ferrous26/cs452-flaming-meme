/*
 * main.c - where it all begins
 */

#include <std.h>
#include <io.h>
#include <vt100.h>
#include <clock.h>
#include <syscall.h>

#define SWI_HANDLER ((void**)0x28)


int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    uart_init();
    vt_blank();
    vt_hide();

    debug_message("Welcome to ferOS build %u", __BUILD__);
    debug_message("Built %s %s", __DATE__, __TIME__);

    *SWI_HANDLER = (void*)syscall_enter;
    debug_cpsr();

    vt_flush();
    unsigned int ret = myTid();

    kprintf_char('\n');
    kprintf_ptr((void*)ret);

    debug_message("\nRETURNED");
    debug_cpsr();
    vt_flush();

    return 0;
}
