#include <tasks/idle.h>
#include <std.h>
#include <vt100.h>
#include <syscall.h>

void idle() {

    vt_log("Starting idle task at %d", myTid());

    volatile int count = 0;
    FOREVER { count++; }
}
