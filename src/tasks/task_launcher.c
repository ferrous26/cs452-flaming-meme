#include <syscall.h>
#include <debug.h>
#include <train.h>
#include <parse.h>

#include <tasks/idle.h>
#include <tasks/stress.h>
#include <tasks/bench_msg.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/term_server.h>
#include <tasks/train_server.h>
#include <tasks/mission_control.h>
#include <tasks/train_station.h>
#include <tasks/calibrate.h>

#include <tasks/task_launcher.h>

#define TERM_ROW (LOG_HOME - 2) // command prompt starts after logging region
#define TERM_COL 6

inline static void print_help() {
    log("  tr <train_num> [0-14]  ~ Set <train_num> to speed 0-14");
    log("  rv <train_num>         ~ Reverse the direction of <train_num>");
    log("  lt <train_num>         ~ Toggle the lights of <train_num>");
    log("  hr <train_num>         ~ Toggle the horn function of <train_num>");
    log("  sw <switch_num> [C|S]  ~ Set <train_num> to speed <speed>");
    log("  re                     ~ Reset the track layout");
    log("  h                      ~ Print this Help Message");
    log("  q                      ~ Quit");
}

static void __attribute__((noreturn)) echo_test() {
    char buffer[32];
    char* ptr = buffer;
    ptr = vt_reset_scroll_region(ptr);
    ptr = vt_clear_screen(ptr);
    ptr = vt_goto(ptr, 1, 1);
    Puts(buffer, ptr - buffer);

    FOREVER {
        Putc(TERMINAL, (char)Getc(TERMINAL));
    }
}

static void action(command cmd, int args[]) {
    switch(cmd) {
    case NONE:
        print_help();
        break;
    case QUIT:
        Shutdown();

    case LOC_SPEED:
        train_set_speed(args[0], args[1]);
        break;
    case LOC_REVERSE:
        train_reverse(args[0]);
        break;
    case LOC_LIGHT:
        train_toggle_light(args[0]);
        break;
    case LOC_HORN:
	train_sound_horn(args[0]);
	break;
    case TRACK_RESET:
       reset_train_state();
       break;
    case TRACK_TURNOUT:
        update_turnout(args[0], args[1]);
        break;

    case SWITCH_STOP:
        delay_sensor(args[0], args[1]);
        train_set_speed(args[2], 0);
        break;
    case SWITCH_TIME: {
        delay_all_sensor();
        int time = Time();
        delay_all_sensor();
        time = Time() - time;
        log("time interval took %d", time);
        break;
    }
    case CALIBRATE:
        Create(5, calibrate);
        break;

    case CMD_BENCHMARK:
	Create(TASK_PRIORITY_MEDIUM_LOW, bench_msg);
        break;
    case CMD_STRESS:
	Create(10, stress_root);
        break;
    case CMD_ECHO:
	Create(TASK_PRIORITY_MEDIUM, echo_test);
	Exit();
	break;
    case REVERSE_LOOKUP: {
        char name[8];
        if (WhoTid(args[0], name) < 0) {
            log("tid %d is not registered", args[0]);
        } else {
            log("tid %d is %s", args[0], name);
        }
        break;
    }
    case ERROR:
        log("invalid command");
	print_help();
        break;
    }
}

void task_launcher() {
    log("Enter an empty command for help");

    char  buffer[128];
    char* ptr = buffer;
    int   insert;

    const char* const line_mark = "TERM> ";
    char  line[80 - 6];

    FOREVER {
        ptr = vt_goto(ptr, TERM_ROW, 1);
        ptr = sprintf_string(ptr, line_mark);
        ptr = sprintf_string(ptr, COLOUR(BG_WHITE) " " COLOUR_RESET);
	ptr = vt_kill_line(ptr);
        Puts(buffer, ptr - buffer);

        char c = 0;
        insert = 0;

        for(; c != '\r';) {
        retry_term:
            c = (char)Getc(TERMINAL);

            switch (c) {
            case '\b':
                if (insert == 0) goto retry_term;
                insert--;
                ptr = vt_goto(buffer, TERM_ROW, insert + TERM_COL+1);
                ptr = vt_kill_line(ptr);
                ptr = sprintf_string(ptr, COLOUR(BG_WHITE) " " COLOUR_RESET);
                break;

            case 0x1B: // swallow escape characters
                c = '^';

            default:
                if (insert == sizeof(line)-2) { insert--; }

                ptr = vt_goto(buffer, TERM_ROW, insert + TERM_COL+1);
                *(ptr++) = c;
                line[insert++] = c;

                if (c != '\r')
                    ptr = sprintf_string(ptr, COLOUR(BG_WHITE) " " COLOUR_RESET);
            }

            Puts(buffer, ptr - buffer);
        }

        line[insert] = '\0';
	log(line);

        int args[5];
        action(parse_command(line, args), args);
    }
}
