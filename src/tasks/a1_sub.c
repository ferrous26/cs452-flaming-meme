
#include <io.h>
#include <vt100.h>
#include <syscall.h>

inline static void print_info(int tid, int pid) {
    vt_log("Id: %d Parent: %d", tid, pid);
    vt_flush();
}

void a1_sub() {
    int tid = myTid();
    int pid = myParentTid();

    print_info(tid, pid);
    Pass();
    print_info(tid, pid);
}
