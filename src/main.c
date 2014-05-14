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

  while (1) {
    tick();
    vt_read();
    vt_write();
  }

  vt_bwblank();

  return 0;
}
