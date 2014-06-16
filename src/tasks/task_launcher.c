
#include <irq.h>
#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>
#include <debug.h>

#include <train.h>
#include <ts7200.h>

#include <tasks/idle.h>
#include <tasks/stress.h>
#include <tasks/seppuku.h>
#include <tasks/k3_demo.h>
#include <tasks/bench_msg.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/term_server.h>
#include <tasks/train_server.h>

#include <tasks/task_launcher.h>

inline static void print_help() {
    log("\n\t"
	"1 ~ K3 assignent demo\n\t"
	"3 ~ Benchmark message passing\n\t"
	"4 ~ Get the current time\n\t");

    log("\t"
	"s ~ Run Stressing Task\n\t"
        "o ~ Poll Sensors");

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

    case 'o':
        put_train_char(WhoIs(TRAIN_SEND), SENSOR_POLL);
        for(int i = 0; i < 10; i++) {
            uint c;
            get_train(WhoIs(TRAIN_RECV), (char*)&c, sizeof(c));
            log("%d - %d", i, c);
        }
        break;

    case 'l':
        put_train_turnout(WhoIs(TRAIN_SEND), TURNOUT_STRAIGHT, 14);
        break;

    case 'c':
        put_train_turnout(WhoIs(TRAIN_SEND), TURNOUT_CURVED, 14);
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
    _create(TASK_PRIORITY_MEDIUM, term_server,  "terminal server");
    _create(TASK_PRIORITY_MEDIUM, name_server,  "name server");
    _create(TASK_PRIORITY_MEDIUM, clock_server, "clock server");
    _create(TASK_PRIORITY_MEDIUM, train_server, "train server");
    _create(TASK_PRIORITY_IDLE,   idle,         "idle task");

    char buffer[128];
    char* ptr = buffer;

    ptr = vt_goto(ptr, 2, 40);
    ptr = sprintf(ptr, "Welcome to ferOS build %u", __BUILD__);
    ptr = vt_goto(ptr, 3, 40);
    ptr = sprintf(ptr, "Built %s %s", __DATE__, __TIME__);
    Puts(buffer, (uint)(ptr - buffer));


    char line_mark[] = "TERM> ";
    char command[80];
    int  insert;

    log("Welcome to Task Launcher (h for help)");
    FOREVER {
        insert = 0;

        ptr = vt_goto(buffer, 80, 0);
        ptr = sprintf(ptr, line_mark);
        Puts(buffer, (uint)(ptr-buffer));

        for(;;) {
            char c = (char)Getc(TERMINAL);

            switch (c) {
            case '\b':
                if(insert == 0) continue;
                ptr = vt_goto(buffer, 80, (--insert)+sizeof(line_mark));
                *(ptr++) = ' ';
                command[insert] = 0;
                break;
            case 0x1B:
                c = '^';
            default:
                if(insert == 80) { insert--; }

                ptr = vt_goto(buffer, 80, insert+sizeof(line_mark));
                *(ptr++) = c;
                command[insert++] = c;
            }
            Puts(buffer, (uint)(ptr-buffer));
            if(c == '\r') { break; }
        }

        tl_action(command[0]);
    }
}
