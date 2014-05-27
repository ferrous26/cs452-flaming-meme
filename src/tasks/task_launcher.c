#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>

#include <tasks/a1_task.h>
#include <tasks/pass_test.h>
#include <tasks/memcpy_bench.h>
#include <tasks/papers.h>
#include <tasks/echo_server.h>
#include <tasks/name_server.h>


#include <tasks/task_launcher.h>

#include <benchmark.h>


BENCH(msg);



extern task_id name_server_tid;

inline static void print_help() {
    vt_log( "\t1 - Kernel 1 Test Task" );
    vt_log( "\tp - Pass Test Benchmark Task" );
    vt_log( "\tm - memcpy Benchmark Task" );
    vt_log( "\tn - msg test" );
    vt_log( "\tb - benchmark self-measure" );
    vt_log( "\th - Print this Help Message" );
    vt_log( "\tq - Quit" );
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
	const char* send = "hey";
	char recv[4];
	BENCH_START(msg);
	for(size i = 100000; i; i--) {
	    Send(1, (char*)send, 4, recv, 4);
	    BENCH_LAP(msg);
	    //	    UNUSED(resp);
	    //debug_log("Send returned %u with %s", resp, recv);
	}
	BENCH_PRINT_WORST(msg);
	BENCH_PRINT_AVERAGE(msg);
	vt_flush();
	break;
    }
    case 'h':
    default:
        print_help();
        break;
    }
}

static void tl_startup() {
    name_server_tid = Create(TASK_PRIORITY_MIN, echo_server);

    if( name_server_tid < 0 ) {
        vt_log("failed starting name server! goodbye cruel world");
	vt_flush();
	Exit();
    }

}

void task_launcher() {
    tl_startup();

    for(;;) {
        vt_log( "Welcome to Task Launcher (h for help)" );
        vt_flush();

        while( !vt_can_get() ) {
	    vt_read();
	    Pass();
	}

	tl_action(vt_getc());
    }
}
