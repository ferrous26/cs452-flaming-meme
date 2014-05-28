#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>

#include <tasks/a1_task.h>

#include <tasks/pass_test.h>
#include <tasks/memcpy_bench.h>
#include <tasks/papers.h>
#include <tasks/stress_msg.h>
#include <tasks/echo_server.h>
#include <tasks/name_server.h>

#include <tasks/task_launcher.h>
#include <benchmark.h>

BENCH(msg);

extern task_id name_server_tid;

inline static void print_help() {
    vt_log("\t1 ~ Kernel 1 Test Task");
    vt_log("\t2 ~ Kernel 2 Rock/Paper/Scissors");
    vt_log("\tb ~ Benchmark Self-Measure");
    vt_log("\te ~ Perform WhoIs Lookup on echo server");
    vt_log("\tm ~ Memcpy Benchmark Task");
    vt_log("\tn ~ Msg Test");
    vt_log("\tp ~ Pass Test Benchmark Task");
    vt_log("\th ~ Print this Help Message");
    vt_log("\ts - Stress the Message System");
    vt_log("\tq ~ Quit");
    vt_flush();
}

static void tl_action(char input) {
    switch(input) {
    case 'q':
        Exit();
        break;
    case '1':
        Create(4, a1_task);
        break;
    case '2':
        break;
    case 'b':
	Create(TASK_PRIORITY_MAX, papers);
	break;
    case 'p':
        Create(4, pass_test);
        break;
    case 'm':
        Create(4, memcpy_bench);
	break;
    case 'n': {
	int tid = WhoIs("echo");
	const char* send = "hey";
	char recv[4];
	BENCH_START(msg);
	for(size i = 1000000; i; i--) {
	    Send( tid, (char*)send, 4, recv, 4);
	    BENCH_LAP(msg);
	}
	BENCH_PRINT_WORST(msg);
	BENCH_PRINT_AVERAGE(msg);
	vt_flush();
	break;
    }
    case 'e':
        debug_log("ECHO_SERVER %d", WhoIs("echo"));
        break;
    case 's':
        Create(31, stress_msg);
        break;
    case 'h':
    default:
        print_help();
        break;
    }
}

static void tl_startup() {
    name_server_tid = Create(TASK_PRIORITY_MAX, name_server);
    if( name_server_tid < 0 ) {
        vt_log("failed starting name server! goodbye cruel world");
	vt_flush();
	Exit();
    }

    // Create(TASK_PRIORITY_MAX-3, k2_server);
    Create(TASK_PRIORITY_MAX-3, echo_server);
}

void task_launcher() {
    tl_startup();

    for(;;) {
        vt_log( "Welcome to Task Launcher (h for help)" );
        vt_flush();

	tl_action(vt_waitget());
    }
}
