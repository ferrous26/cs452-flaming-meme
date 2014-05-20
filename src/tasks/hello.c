
#include <io.h>
#include <syscall.h>
#include <debug.h>
#include <hello.h>
#include <debug.h>

static void print_info(int tid, int pid) {
    debug_log("Id: %d Parent: %d", tid, pid);
    vt_flush();
}

void hello() {
    int tid = myTid();
    int pid = myParentTid();

    print_info(tid, pid);
    Pass();
    print_info(tid, pid);
}
