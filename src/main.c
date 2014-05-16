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

static void print_startup_message(void) {
    debug_message("Welcome to ferOS build %u", __BUILD__);
    debug_message("Built %s %s", __DATE__, __TIME__);
}


#define SWI_HANDLER ((void**)0x28)

DEBUG_TIME(test);

void function(void) {
    register unsigned int r0 asm ("r0");
    register unsigned int r1 asm ("r1");

    kprintf_char( '\n' );
    kprintf_ptr( (void*)r0 );
    kprintf_char( '\n' );
    kprintf_ptr( (void*)r1 );

    debug_message( "Interrupt!" );
    debug_cpsr();

    debug_message( "Exiting Interrupt!" );
}

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    DEBUG_TIME_INIT(test);

    // startup various systems
    clock_t4enable();
    debug_init();
    uart_init();
    vt_init();

    print_startup_message();

    *SWI_HANDLER = (void*)syscall_enter;
    debug_cpsr();

    vt_flush();

    asm ("swi 23");
    register unsigned int x asm("r0");

    unsigned int i = x;
    kprintf_char('\n');
    kprintf_ptr((void*)i);

    debug_message("\nRETURNED");
    debug_cpsr();

    // TODO: main loop goes here!

    uint count = 100000;
    for (; count; count--) {
	vt_read();
	vt_write();
	DEBUG_TIME_LAP(test, 0);
    }

    DEBUG_TIME_PRINT_WORST(test);
    DEBUG_TIME_PRINT_AVERAGE(test);

    // shutdown various systems
    vt_flush();

    return 0;
}
