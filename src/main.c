/*
 * main.c - where it all begins
 */

#include <std.h>
#include <io.h>
#include <vt100.h>
#include <clock.h>

#define SWI_HANDLER ((int*)0x28)

void function( ) {
    debug_message( "Interrupt!" );
    kprintf_cpsr();

    debug_message( "Exiting Interrupt!" );
    asm ( "movs pc, lr" );
}

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    uart_init();
    vt_blank();
    vt_hide();

    debug_message("Welcome to ferOS v%u", __BUILD__);
    debug_message("Built %s %s", __DATE__, __TIME__);

    *SWI_HANDLER = (int)function;

    kprintf_cpsr();
    kprintf_char('\n');
    
    asm ("swi 1\n");

    kprintf_string("\nRETURNED");
    kprintf_cpsr();     

    vt_flush();
    return 0;
}

