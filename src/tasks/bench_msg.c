#include <tasks/bench_msg.h>
#include <vt100.h>
#include <benchmark.h>
#include <syscall.h>
#include <tasks/term_server.h>

#ifdef BENCHMARK
static
BENCH(bench);
#endif

#define ITERATIONS 100000

static void baseline() {

    BENCH_START(bench);
    for (uint i = ITERATIONS; i; i--)
	BENCH_LAP(bench);

    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);
}

static void send_it(int child) {

    log("4 bytes (1 word)");

    const char* msg = "hey";
    char rply[4];

    BENCH_START(bench);
    for (uint i = ITERATIONS; i; i--) {
	Send(child, (char*)msg, 4, rply, 4);
	BENCH_LAP(bench)
    }

    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);


    log("64 bytes (16 words)");

    const char* big_msg =
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"0123456789 ";
    char big_rply[64];

    BENCH_START(bench)
    for (uint i = ITERATIONS; i; i--) {
	Send(child, (char*)big_msg, 64, big_rply, 64);
	BENCH_LAP(bench)
    }

    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);
}

void bench_msg() {

    // this guy sets it up
    if (myPriority() == (TASK_PRIORITY_MEDIUM_LOW - 1)) {

	int child = Create(TASK_PRIORITY_MEDIUM_LOW - 2, bench_msg);
	if (child < 0) {
	    log("Failed to create child task for benchmarking");
	    return;
	}

	log("Child is %u", child);

	log("Starting message passing benchmark!");

	log("Baseline measurement!");
	baseline();
	log("");

	log("Send before receive");
	send_it(child);
	log("");

	log("Receive before send");
	ChangePriority(TASK_PRIORITY_MEDIUM_LOW - 3);
	Pass(); // ensure the child gets ahead
	send_it(child);
	log("");

	return;
    }

    int tid = 99;
    char buffer[64];

    for (;;) {
        int siz = Receive(&tid, buffer, 64);
	Reply(tid, buffer, siz);
    }
}
