#include <tasks/bench_msg.h>
#include <tasks/term_server.h>
#include <vt100.h>
#include <syscall.h>
#include <benchmark.h>

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

    //    log("0 bytes (NULL message)");

    BENCH_START(bench);
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, NULL, 0, NULL, 0);
        if (result != 0)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);


    //    log("4 bytes (1 word)");

    const char* msg = "hey there, sexy beast from unicorn land....";
    char rply[32];

    BENCH_START(bench);
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, (char*)msg, 2, rply, 2);
        if (result != 2)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);

    BENCH_START(bench);
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, (char*)msg, 4, rply, 4);
        if (result != 4)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);

    BENCH_START(bench);
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, (char*)msg, 8, rply, 8);
        if (result != 8)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);

    BENCH_START(bench);
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, (char*)msg, 16, rply, 16);
        if (result != 16)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);

    BENCH_START(bench);
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, (char*)msg, 32, rply, 32);
        if (result != 32)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //    BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);


    //log("64 bytes (16 words)");

    const char* big_msg =
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRTSUVWXYZ"
	"0123456789 ";
    char big_rply[256];

    BENCH_START(bench)
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, (char*)big_msg, 64, big_rply, 64);
        if (result != 64)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);

    BENCH_START(bench)
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, (char*)big_msg, 128, big_rply, 128);
        if (result != 128)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //BENCH_PRINT_WORST(bench);
    BENCH_PRINT_AVERAGE(bench);

    BENCH_START(bench)
    for (uint i = ITERATIONS; i; i--) {
	const int result = Send(child, (char*)big_msg, 256, big_rply, 256);
        if (result != 256)
            ABORT("Message passing failure %d", result);
	BENCH_LAP(bench)
    }

    //BENCH_PRINT_WORST(bench);
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

        //	log("Receive before send");
	//ChangePriority(TASK_PRIORITY_MEDIUM_LOW - 3);
	//Pass(); // ensure the child gets ahead
	//send_it(child);
	//log("");

	return;
    }

    int tid = 99;
    char buffer[256];

    for (;;) {
        int siz = Receive(&tid, buffer, 256);
        if (siz >= 0) {
            siz = Reply(tid, buffer, (size_t)siz);
            if (siz != 0)
                ABORT("Message sending failure %d", siz);
        }
        else {
            ABORT("Message sending failure %d", siz);
        }
    }
}
