
#include <io.h>
#include <vt100.h>
#include <syscall.h>

#include <tasks/k3_demo.h>

#define K3_TASK(name, delay, iterations)                            \
static void name() {                                                \
    int tid  = myTid();                                             \
    int ptid = myParentTid();                                       \
    for(int i = 1; i < iterations+1; i++) {                         \
        Delay(delay);                                               \
        log("K3_TASK(%d): Delay:%d, Iter:%d", tid, delay, i);    \
    }                                                               \
    Send(ptid, NULL, 0, NULL, 0);                                   \
}                                                                   \

K3_TASK(k3_1, 10, 20)
K3_TASK(k3_2, 23,  9)
K3_TASK(k3_3, 33,  6)
K3_TASK(k3_4, 71,  3)

void k3_root() {
    int tid;
    int my_tid = myTid();
    int nchildren = 0;

    log("K3_Root(%d): Start", my_tid);

    tid = Create(9, k3_1);
    if ( tid > 0 ) {
        log("K3_Root(%d): Created %d", my_tid, tid);
        nchildren++;
    }

    tid = Create(8, k3_2);
    if ( tid > 0 ) {
        log("K3_Root(%d): Created %d", my_tid, tid);
        nchildren++;
    }

    tid = Create(7, k3_3);
    if ( tid > 0 ) {
        log("K3_Root(%d): Created %d", my_tid, tid);
        nchildren++;
    }

    tid = Create(6, k3_4);
    if (tid > 0) {
        log("K3_Root(%d): Created %d", my_tid, tid);
        nchildren++;
    }

    log("K3_Root(%d): Waiting", my_tid);
    for (int i = 0; i < nchildren; i++) {
        Receive(&tid, NULL, 0);
        Reply(tid, NULL, 0);
    }

    log("K3_Root(%d): Ending", my_tid);
}
