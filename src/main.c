/*
 * main.c - where it all begins
 */

#include <std.h>
#include <io.h>
#include <clock.h>

int main(int argc, char* argv[]) {

    UNUSED(argc);
    UNUSED(argv);

    uart_init();
    vt_blank();
    vt_hide();

    debug_message("Welcome to ferOS v%u", __BUILD__);
    debug_message("Built %s %s", __DATE__, __TIME__);

    clock_t4enable();
    uint clock_prev = clock_t4tick();
    uint clock_curr = clock_t4tick();

    while (1) {
	vt_read();
	vt_write();
    }

    vt_bwblank();

    return 0;
}
