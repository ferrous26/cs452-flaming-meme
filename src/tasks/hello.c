
#include <io.h>
#include <syscall.h>

#include <hello.h>

static void print_info(int tid, int pid) {
    kprintf("Id: %d Parent: %d\n", tid, pid);
    vt_flush();
}

void hello() {
    int tid = myTid();
    int pid = myParentTid();

    print_info(tid, pid);
    Pass();
    print_info(tid, pid);
    Exit(); // Doesn't need to be called linked implicitly
}

