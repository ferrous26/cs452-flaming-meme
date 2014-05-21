#include <io.h>
#include <debug.h>
#include <syscall.h>
#include <benchmark.h>

#include <tasks/a1_sub.h>

static void print_created(int tid) {
    debug_log("Created: %d", tid);
    vt_flush();
}

void a1_task(void) {
    print_created(Create(1, a1_sub));
    print_created(Create(1, a1_sub));
    print_created(Create(8, a1_sub));
    print_created(Create(8, a1_sub));

    debug_log("First: exiting");
    vt_flush();
}
