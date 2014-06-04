
#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>
#include <debug.h>
#include <irq.h>

#include <tasks/idle.h>
#include <tasks/k3_demo.h>
#include <tasks/bench_msg.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/task_launcher.h>

extern task_id name_server_tid;
extern task_id clock_server_tid;

inline static void print_help() {
    vt_log("\n\t"
	   "3 ~ Benchmark message passing\n\t"
           "1 ~ K3 assignent demo\n\t"

	   "i ~ Print the interrupt table status\n\t"
	   "a ~ Trigger software interrupt 0\n\t"
	   "z ~ Trigger software interrupt 1\n\t"
           "s ~ Trigger software interrupt 63\n\n\t"

	   "p ~ Delay until the moon (+ 50 ticks)\n\t"

	   "h ~ Print this Help Message\n\t"
	   "q ~ Quit\n");

    vt_flush();
}

static void tl_action(char input) {
    switch(input) {
    case '1':
        Create(14, k3_root);
        break;
    case '3':
        Create(TASK_PRIORITY_MAX, bench_msg);
	break;
    case 'i':
	debug_interrupt_table();
	break;
    case 'a':
	irq_simulate_interrupt(0);
	break;
    case 'z':
	irq_simulate_interrupt(1);
	break;
    case 's':
	irq_simulate_interrupt(63);
	break;

    case 'p':
	Delay(50);
	break;

    case 'q':
        Shutdown();
        break;
    case 'h':
    default:
        print_help();
        break;
    }
}

static int _create(int priority, void (*code) (void), const char* const name) {

    int tid = Create(priority, code);
    if (tid < 0) {
        vt_log("Failed starting %s! Goodbye cruel world", name);
	vt_flush();
	Exit();
    }

    return tid;
}

static void tl_startup() {
    _create(TASK_PRIORITY_MIN, idle, "idle task");

    name_server_tid = _create(TASK_PRIORITY_MAX-2,
			      name_server,
			      "name server");

    clock_server_tid = _create(TASK_PRIORITY_MAX-1,
			       clock_server,
			       "clock server");
}

void task_launcher() {
    tl_startup();

    for(;;) {
        vt_log("Welcome to Task Launcher (h for help)");
        vt_flush();
	tl_action(vt_waitget());
    }
}
