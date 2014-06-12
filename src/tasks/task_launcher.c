
#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>
#include <debug.h>
#include <irq.h>

#include <tasks/idle.h>
#include <tasks/stress.h>
#include <tasks/seppuku.h>
#include <tasks/k3_demo.h>
#include <tasks/bench_msg.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/term_server.h>

#include <tasks/task_launcher.h>

inline static void print_help() {
    log("\n\t"
	"1 ~ K3 assignent demo\n\t"
	"3 ~ Benchmark message passing\n\t"
	"4 ~ Get the current time\n\t");

    log("\t"
	"s ~ Run Stressing Task\n\t");

    log("\t"
	"h ~ Print this Help Message\n\t"
	"q ~ Quit\n");
}

static void tl_action(char input) {
    switch(input) {
    case '1':
        Create(16, k3_root);
        break;
    case '3':
        Create(TASK_PRIORITY_MAX, bench_msg);
	break;
    case '4':
	log("The time is %u ticks!", Time());
	break;
    case 's':
        Create(16, stress_root);
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
        log("Failed starting %s! Goodbye cruel world", name);
	Shutdown();
    }
    return tid;
}

void task_launcher() {
    // start idle task at highest level so that it can initialize
    // then it will set itself to the proper priority level before
    // entering its forever loop
    _create(TASK_PRIORITY_HIGH - 1, term_server,  "terminal server");
    _create(TASK_PRIORITY_HIGH - 1, name_server,  "name server");
    _create(TASK_PRIORITY_HIGH - 1, clock_server, "clock server");
    _create(TASK_PRIORITY_IDLE,     idle,         "idle task");

    char buffer[128];
    char* ptr = buffer;

    ptr = vt_goto(ptr, 2, 40);
    ptr = sprintf(ptr, "Welcome to ferOS build %u", __BUILD__);
    ptr = vt_goto(ptr, 3, 40);
    ptr = sprintf(ptr, "Built %s %s", __DATE__, __TIME__);

    Puts(buffer, (uint)(ptr - buffer));


    FOREVER {
        log("Welcome to Task Launcher (h for help)");
	tl_action(uart2_bw_waitget());
    }
}
