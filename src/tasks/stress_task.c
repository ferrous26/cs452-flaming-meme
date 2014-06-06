
#include <io.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>
#include <scheduler.h>


#include <tasks/stress.h>

static void stress_sub() {
    int tid  = myTid();
    int ptid = myParentTid();

    int seed  = tid;
    int delay = (rand(seed) % 70) + 10;
    int iter  = (rand(seed) % 10) + 2;
    
    for (int i = 1; i < iter; i++) {
        Delay(delay);
        vt_log("STR_T(%d): Delay:%d Iter:%d/%d", tid, delay, i, iter-1);
        vt_flush();
    }

    int ret = Send(ptid, NULL, 0, NULL, 0);
    assert(ret == 0, "Parent Died Before Task Could Return");
    UNUSED(ret);
}

void stress_root() {
    int tid;
    int nchildren = 0;
    int my_tid    = myTid();

    vt_log("STR_R(%d): Start", my_tid);

    int seed = my_tid;
    for (int i = 1+(rand(seed) % 6); i > 0; i--) {
        tid = Create((rand(seed) % TASK_PRIORITY_LEVELS-6) + 3, stress_sub);
        if ( tid >= 0 ) {
            vt_log("STR_R(%d): Created %d", my_tid, tid);
            nchildren++;
        }
    }
    
    vt_log("STR_R(%d): Waiting", my_tid);
    for (int i = 0; i < nchildren; i++) {
        Receive(&tid, NULL, 0);
        int ret = Reply(tid, NULL, 0);

        assert(ret == 0, "Child Died before parent could send back message");
        UNUSED(ret);
    }

    if (tid < 10000) {
        Create(16, stress_root);
        Create(16, stress_root);
    }
    
    vt_log("STR_R(%d): Ending", my_tid);
    vt_flush();
}

