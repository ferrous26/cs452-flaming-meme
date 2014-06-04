#include <tasks/idle.h>
#include <std.h>

void idle() {
    volatile int count = 0;
    FOREVER { count++; }
}
