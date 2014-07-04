#include <syscall.h>
#include <debug.h>
#include <train.h>
#include <parse.h>
#include <physics.h>

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

#include <tasks/train_control.h>

#include <tasks/task_launcher.h>

extern uint* _DataStart;
extern uint* _DataKernEnd;
extern uint* _DataEnd;
extern uint* _BssStart;
extern uint* _BssEnd;
extern uint* _RODataStart;
extern uint* _RODataEnd;
extern uint* _TextStart;
extern uint* _TextKernEnd;
extern uint* _TextEnd;

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

static void seppuku() {
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
    case DUMP:
        physics_dump();
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
	train_toggle_horn(args[0]);
	break;
    case TRACK_RESET:
       reset_train_state();
       break;
    case TRACK_TURNOUT:
        update_turnout(args[0], args[1]);
        break;
    case SWITCH_STOP:
        train_stop_at(args[2], args[0], args[1]);
        break;
    case SWITCH_TIME: {
        delay_all_sensor();
        int time = Time();
        delay_all_sensor();
        time = Time() - time;
        log("time interval took %d", time);
        break;
    }

    case CALIBRATE: {
        int tid = Create(5, calibrate);
        Send(tid, (char*)args, sizeof(int), NULL, 0);
        break;
    }

    case ACCELERATE: {
        int tid = Create(5, velocitate);
        Send(tid, (char*)args, sizeof(int), NULL, 0);
        break;
    }

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

    case SEPPUKU:
        Create(TASK_PRIORITY_EMERGENCY, seppuku);
        break;

    case SIZES:
        log("\n"
            "Data:\n"
            "    Kernel: %u bytes\n"
            "      Task: %u bytes\n"
            "     Total: %u bytes\n"
            "Text:\n"
            "    Kernel: %u bytes\n"
            "      Task: %u bytes\n"
            "     Total: %u bytes\n"
            "BSS:        %u bytes\n"
            "ROData:     %u bytes\n",
            &_DataKernEnd - &_DataStart,
            &_DataEnd     - &_DataKernEnd,
            &_DataEnd     - &_DataStart,
            &_TextKernEnd - &_TextStart,
            &_TextEnd     - &_TextKernEnd,
            &_TextEnd     - &_TextStart,
            &_BssEnd      - &_BssStart,
            &_RODataEnd   - &_RODataStart);
        break;

    case UPDATE_THRESHOLD:
        physics_change_feedback_threshold(args[0]);
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
    char* ptr;
    int   insert;

    const char* const line_mark = "TERM> ";
    char  line[80 - 6];

    ptr = vt_goto(buffer, TERM_ROW, TERM_COL);
    ptr = sprintf_string(ptr, "Please select track (a) or (b)... ");
    Puts(buffer, ptr-buffer);

    do {
        // quick hack to force track loading
        buffer[0] = (char)Getc(TERMINAL);
    } while (load_track(buffer[0]));
    ptr = buffer;

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
