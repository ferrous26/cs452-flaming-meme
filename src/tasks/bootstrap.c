#include <io.h>
#include <debug.h>
#include <syscall.h>

#include <hello.h>
#include <bootstrap.h>

static void print_created(int tid) {
    kprintf("Created: %d\n", tid);
    vt_flush();
}

void bootstrap(void) {
    print_created(Create(1, hello));
    print_created(Create(1, hello));
    print_created(Create(8, hello));
    print_created(Create(8, hello));

    kprintf("First: exiting");
    vt_flush();
}
