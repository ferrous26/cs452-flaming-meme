
#include <irq.h>
#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>
#include <debug.h>

#include <train.h>
#include <ts7200.h>

#include <parse.h>

#include <tasks/idle.h>
#include <tasks/stress.h>
#include <tasks/bench_msg.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/term_server.h>
#include <tasks/train_server.h>
#include <tasks/task_launcher.h>

#define TERM_ROW (LOG_END + 1) // command prompt starts after logging region
#define TERM_COL 6

inline static void print_help() {
    log("\t"
	"TODO\n"
	"h ~ Print this Help Message\n\t"
	"q ~ Quit\n");
}


static void __attribute__ ((unused)) tl_action(char input) {
    switch(input) {
    case 'o':
        Putc(TRAIN, SENSOR_POLL);
        for(int i = 0; i < 10; i++) {
            char c = (char)Getc(TRAIN);
            log("%d - %d", i, c);
        }
        break;
    case 'n':
        Putc(TRAIN, TRAIN_ALL_STOP);
        break;
    case 'y':
        Putc(TRAIN, TRAIN_ALL_START);
        break;
    case 'h':
    default:
        print_help();
        break;
    }
}

static void echo_test() {

    char buffer[32];
    char* ptr = buffer;
    ptr = vt_reset_scroll_region(ptr);
    ptr = vt_clear_screen(ptr);
    ptr = vt_goto(ptr, 1, 1);
    Puts(buffer, ptr - buffer);

    FOREVER {
	char c = (char)Getc(TERMINAL);
	if (c == '`') break;
	Putc(TERMINAL, c);
    }
}

static void action(command cmd, int args[]) {
    switch(cmd) {
    case NONE:
        break;
    case SPEED:
        log("setting train %d to %d", args[0], args[1]);
        put_train_cmd((char)args[0], (char)args[1]);
	Create(TASK_PRIORITY_MEDIUM_HI, echo_test);
	Exit(); // no more Terminal!
        break;
    case GATE:
        if(args[1] == 's' || args[1] == 'S') {
            put_train_turnout(TURNOUT_STRAIGHT,(char)args[0]);
        } else if (args[1] == 'c' || args[1] == 'C') {
            put_train_turnout(TURNOUT_CURVED, (char)args[0]);
        } else {
            log("invalid command");
        }
        break;
    case REVERSE:
	Create(TASK_PRIORITY_MEDIUM, bench_msg);
	Delay(2000); // :)
	Create(10, stress_root);
        break;
    case QUIT:
        Shutdown();
    case ERROR:
        log("invalid command");
	print_help();
        break;
    }
}

void task_launcher() {

    log("Welcome to ferOS build %u", __BUILD__);
    log("Built %s %s", __DATE__, __TIME__);
    log("Enter h for help");

    char  buffer[128];
    char* ptr = buffer;
    int   insert;
    char  line[80];
    const char* const line_mark = "TERM> ";

    FOREVER {
        insert = 1;

        ptr = vt_goto(ptr, TERM_ROW, 1);
        ptr = sprintf_string(ptr, line_mark);
        int result = Puts(buffer, ptr - buffer);
	assert(result != 0, "Failed to write prompt");

        char c = 0;
        for(; c != '\r';) {
            c = (char)Getc(TERMINAL);

            switch (c) {
            case '\b':
                if (insert == 1) continue;
		insert--;
                ptr = vt_goto(buffer, TERM_ROW, insert + TERM_COL);
                ptr = vt_kill_line(ptr);
                break;

            case 0x1B: // swallow escape characters
                c = '^';

            default:
                if (insert < 80) insert++;

                ptr = vt_goto(buffer, TERM_ROW, insert + TERM_COL);
                *(ptr++) = c;
                line[insert - 1] = c;
            }
            Puts(buffer, ptr - buffer);
        }

        line[insert - 1] = '\0';

        int args[5];
        action(parse_command(line, args), args);
    }
}
