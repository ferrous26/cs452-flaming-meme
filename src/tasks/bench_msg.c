#include <tasks/bench_msg.h>
#include <vt100.h>
#include <benchmark.h>
#include <syscall.h>
#include <scheduler.h>

BENCH(bench);

static void baseline() {

    BENCH_START(bench);
    for (size i = 100000; i; i--)
	BENCH_LAP(bench);

    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);
    vt_flush();
}

static void send_it(int child) {

    vt_log("4 bytes (1 word)");

    const char* msg = "hey";
    char rply[4];

    BENCH_START(bench);
    for (size i = 100000; i; i--) {
	int siz = Send(child, (char*)msg, 4, rply, 4);
	if (siz != 4) {
	    vt_log("Send Failure %d", siz);
	    vt_flush();
	    return;
	}
	BENCH_LAP(bench);
    }

    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);
    vt_flush();


    vt_log("64 bytes (16 words)");

    const char* big_msg =
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"0123456789 ";
    char big_rply[64];

    BENCH_START(bench);
    for (size i = 100000; i; i--) {
	int siz = Send(child, (char*)big_msg, 64, big_rply, 64);
	if (siz != 64) {
	    vt_log("Send Failure %d", siz);
	    vt_flush();
	    return;
	}
	BENCH_LAP(bench);
    }

    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);
    vt_flush();
}

void bench_msg() {

    // this guy sets it up
    if (myPriority() == TASK_PRIORITY_MAX) {

	int child = Create(TASK_PRIORITY_MAX / 2, bench_msg);
	if (child < 0) {
	    vt_log("Failed to create child task for benchmarking");
	    vt_flush();
	    return;
	}

	vt_log("Starting message passing benchmark!");


	vt_log("Baseline measurement!");
	baseline();

	vt_log("Send before receive");
	send_it(child);

	ChangePriority(2);

	vt_log("Receive before send");
	send_it(child);
    }

    int tid = 99;
    char buffer[64];
    for(;;) {
        int siz = Receive(&tid, buffer, 64);
	siz = Reply(tid, buffer, siz);
	if (siz < 0) {
	    vt_log("Reply Failure %d", siz);
	    vt_flush();
	    return;
	}
    }
}
