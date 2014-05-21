
#include <io.h>
#include <debug.h>
#include <syscall.h>

inline static void print_info(int tid, int pid) {
    debug_log("Id: %d Parent: %d", tid, pid);
    vt_flush();
}

void a1_sub() {
    int tid = myTid();
    int pid = myParentTid();

    print_info(tid, pid);
    Pass();
    print_info(tid, pid);
}
