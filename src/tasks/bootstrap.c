#include <io.h>
#include <debug.h>
#include <syscall.h>

#include <hello.h>
#include <bootstrap.h>

static void print_created(int tid) {
    debug_log("Created: %d", tid);
    vt_flush();
}

void bootstrap(void) {
    print_created(Create(1, hello));
    print_created(Create(1, hello));
    print_created(Create(8, hello));
    print_created(Create(8, hello));

    debug_log("First: exiting");
    vt_flush();
}
