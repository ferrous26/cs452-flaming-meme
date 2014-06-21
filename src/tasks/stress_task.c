
#include <io.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>
#include <tasks/stress.h>
#include <tasks/term_server.h>
#include <tasks/clock_server.h>

static void stress_sub() {
    int tid  = myTid();
    int ptid = myParentTid();

    int seed  = tid;
    int delay = (rand(seed) % 70) + 10;
    int iter  = (rand(seed) % 10) + 2;

    for (int i = 1; i < iter; i++) {
        Delay(delay);
        log("STR_T(%d): Delay:%d Iter:%d/%d", tid, delay, i, iter-1);
    }

    int ret = Send(ptid, NULL, 0, NULL, 0);
    UNUSED(ret);
    assert(ret == 0, "Parent Died Before Task Could Return");
}

void stress_root() {
    int tid = 0;
    int nchildren = 0;
    int my_tid    = myTid();

    log("STR_R(%d): Start", my_tid);

    int seed = my_tid;
    for (int i = 1+(rand(seed) % 6); i > 0; i--) {
        tid = Create((rand(seed) % 6) + 3, stress_sub);
        if ( tid >= 0 ) {
            log("STR_R(%d): Created %d", my_tid, tid);
            nchildren++;
        }
    }

    log("STR_R(%d): Waiting", my_tid);
    for (int i = 0; i < nchildren; i++) {
        Receive(&tid, NULL, 0);
        int ret = Reply(tid, NULL, 0);

	UNUSED(ret);
        assert(ret == 0, "Child Died before parent could send back message");
    }

    if (my_tid < 10000) {
        Create(10, stress_root);
        Create(10, stress_root);
    }

    log("STR_R(%d): Ending", my_tid);
}
