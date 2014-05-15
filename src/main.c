/*
 * main.c - where it all begins
 */

#include <std.h>
#include <io.h>
#include <clock.h>

void function( ) {
    kprintf_bwstring( "Interrupt!\n" );
    __asm__ ( "movs pc, lr" );
}

int main(int argc, char* argv[]) {

    UNUSED(argc);
    UNUSED(argv);

    uart_init();
    vt_blank();
    vt_hide();

    debug_message("Welcome to ferOS v%u", __BUILD__);
    debug_message("Built %s %s", __DATE__, __TIME__);

    // tell the CPU where to handle software interrupts
    void** irq_handler = (void**)0x28;
    *irq_handler = (void*)function;

    /*
      int i;
      for( i = 8; i < 255; i += 4 ) {
      irq_handler[i/4] = (void*)function;
      }
    */

    clock_t4enable();
    uint clock_prev = clock_t4tick();
    uint clock_curr = clock_t4tick();

    __asm__ ("swi 1");
    kprintf_bwstring( "RETURNED" );

    while (1) {
	vt_read();
	vt_write();

	if (clock_curr - clock_prev > 100000) {
	    clock_prev = clock_curr;
	    debug_message("Clock is %u", clock_curr);
	}
    }

    vt_bwblank();

    return 0;
}
