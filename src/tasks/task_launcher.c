
#include <irq.h>
#include <vt100.h>
#include <syscall.h>
#include <scheduler.h>
#include <debug.h>

#include <train.h>
#include <ts7200.h>

#include <parse.h>
#include <char_buffer.h>

#include <tasks/idle.h>
#include <tasks/stress.h>
#include <tasks/bench_msg.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/term_server.h>
#include <tasks/train_server.h>
#include <tasks/task_launcher.h>

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

CHAR_BUFFER(32);

static void __attribute__ ((noreturn)) echo_test() {

    char buffer[32];
    char* ptr = buffer;
    ptr = vt_reset_scroll_region(ptr);
    ptr = vt_clear_screen(ptr);
    ptr = vt_goto(ptr, 1, 1);
    Puts(buffer, ptr - buffer);

    // start the party
    char_buffer buf;
    cbuf_init(&buf);

    FOREVER {
	int ret = Getc(TERMINAL);
	cbuf_produce(&buf, (char)ret);

	if (cbuf_count(&buf) == 32)
	    Puts(buf.buffer, 32);
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
	Delay(200); // :)
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
    char* line_mark = "TERM> ";

    FOREVER {
        insert = 0;

        ptr = vt_goto(ptr, 80, 0);
        ptr = sprintf_string(ptr, line_mark);
        Puts(buffer, ptr - buffer);
	// TODO: check Puts result code

        char c = 0;
        for(;c != '\r';) {
            c = (char)Getc(TERMINAL);

            switch (c) {
            case '\b':
                if(insert == 0) continue;
                ptr = vt_goto(buffer, 80, (--insert) + (int)sizeof(line_mark));
                *(ptr++) = ' ';
                line[insert] = '\0';
                break;
            case 0x1B:
                c = '^';
            default:
                if(insert == 79) { insert--; }

                ptr = vt_goto(buffer, 80, insert + (int)sizeof(line_mark));
                *(ptr++) = c;
                line[insert++] = c;
            }
            Puts(buffer, (int)(ptr-buffer));
        }
        line[insert] = '\0';

        int args[5];
        action(parse_command(line, args), args);
    }
}
