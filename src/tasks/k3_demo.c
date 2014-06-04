
#include <io.h>
#include <vt100.h>
#include <syscall.h>

#include <tasks/k3_demo.h>


#define K3_TASK(name, delay, iterations)        \
void name() {                                   \
    int tid  = myTid();                         \
    int ptid = myParentTid();                   \
    for(int i = 1; i < iterations+1; i++) {     \
        Delay(delay);                           \
        vt_log("%d:\t%d,\t%d", tid, delay, i);  \
        vt_flush();                             \
    }                                           \
    Send(ptid, NULL, 0, NULL, 0);               \
}

K3_TASK(k3_1, 10, 20)
K3_TASK(k3_2, 23,  9)
K3_TASK(k3_3, 33,  6)
K3_TASK(k3_4, 71,  3)

void k3_root() {
    Create(10, k3_1);
    Create(11, k3_2);
    Create(12, k3_3);
    Create(13, k3_4);

    int tid;
    for(int i = 0; i < 4; i++){
        Receive(&tid, NULL, 0);
        Reply(tid, NULL, 0);
    }
}
