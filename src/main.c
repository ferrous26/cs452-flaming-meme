/*
 * main.c - where it all begins
 */

#include <std.h>
#include <io.h>

int main(int argc, char* argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  uart_init();
  vt_blank();
  vt_hide();

  debug_message("Welcome OS v%u", __BUILD__);
  debug_message("Built %s %s", __DATE__, __TIME__);

  while (1) {
    //    tick();
    vt_read();
    vt_write();
  }

  vt_bwblank();

  return 0;
}
