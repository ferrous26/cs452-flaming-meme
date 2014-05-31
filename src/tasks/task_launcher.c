#include <tasks/task_launcher.h>

#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>
#include <irq.h>

#include <tasks/name_server.h>
#include <tasks/k2_server.h>
#include <tasks/k2_client.h>
#include <tasks/bench_memcpy.h>
#include <tasks/bench_msg.h>
#include <tasks/test_msg.h>
#include <tasks/test_ns.h>

extern task_id name_server_tid;

inline static void print_help() {

    vt_log("\n\t"
	   "1 ~ Benchmark memcpy\n\t"
	   "2 ~ Kernel 2 Rock/Paper/Scissors\n\t"
	   "r ~ Kernel 2 Rock/Paper/Scissors (2 Computers)\n\t"
	   "3 ~ Benchmark message passing\n\t"
	   "4 ~ Test message passing\n\t"
	   "5 ~ Test name server\n\t"
	   "i ~ Print the interrupt table status\n\t"
	   "h ~ Print this Help Message\n\t"
	   "q ~ Quit\n");

    vt_flush();
}

static void tl_action(char input) {
    switch(input) {
    case '1':
        Create(TASK_PRIORITY_MAX, bench_memcpy);
	break;
    case '2':
        Create(4, k2_computer);
        Create(4, k2_human);
        break;
    case 'r':
        Create(4, k2_computer);
        Create(4, k2_computer);
        break;
    case '3':
        Create(TASK_PRIORITY_MAX, bench_msg);
	break;
    case '4':
        Create(TASK_PRIORITY_MAX, test_msg);
	break;
    case '5':
        Create(TASK_PRIORITY_MAX, test_ns);
	break;
    case 'i':
	debug_interrupt_table();
	break;
    case 'q':
        Exit();
        break;
    case 'h':
    default:
        print_help();
        break;
    }
}

static void tl_startup() {

    name_server_tid = Create(TASK_PRIORITY_MAX, name_server);
    if (name_server_tid < 0) {
        vt_log("Failed starting name server! Goodbye cruel world");
	vt_flush();
	Exit();
    }

    int rps = Create(TASK_PRIORITY_MAX-2, k2_server);
    if (rps < 0) {
        vt_log("Failed starting the RPS server! Goodbye cruel world");
	vt_flush();
	Exit();
    }
}

void task_launcher() {
    tl_startup();

    for(;;) {
        vt_log("Welcome to Task Launcher (h for help)");
        vt_flush();
	tl_action(vt_waitget());
    }
}
