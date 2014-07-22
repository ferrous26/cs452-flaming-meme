#include <syscall.h>
#include <debug.h>
#include <train.h>
#include <parse.h>

#include <track_node.h>
#include <normalize.h>
#include <track_data.h>

#include <tasks/idle.h>
#include <tasks/stress.h>
#include <tasks/bench_msg.h>

#include <tasks/term_server.h>
#include <tasks/train_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <tasks/mission_control.h>
#include <tasks/sensor_farm.h>
#include <tasks/track_reservation.h>

#include <tasks/courier.h>
#include <tasks/path_admin.h>

#include <tasks/train_control.h>
#include <tasks/task_launcher.h>

extern uint* _DataStart;
extern uint* _DataKernEnd;
extern uint* _DataKernWarmEnd;
extern uint* _DataEnd;
extern uint* _BssStart;
extern uint* _BssEnd;
extern uint* _RODataStart;
extern uint* _RODataEnd;
extern uint* _TextStart;
extern uint* _TextKernEnd;
extern uint* _TextColdStart;
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

static void seppuku() {}

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
        train_dump_velocity_table(args[0]);
        break;
    case QUIT:
        Shutdown();
    case WHEREIS: {
        const track_location l = train_where_are_you(args[0]);

        if (l.sensor == INVALID_TRAIN) {
            log("Train %d does not exist", args[0]);
        }
        else if (l.sensor < 0) {
            log("Failed to send message to train %d (%d)",
                args[0], l.sensor);
        }
        else if (l.sensor == 80) {
            log("Train %d does not know where it is", args[0]);
        }
        else {
            const sensor s = pos_to_sensor(l.sensor);
            log("Train %d is at %c%d %c %d cm",
                args[0], s.bank, s.num,
                l.offset >= 0 ? '+' : '-',
                abs(l.offset) / 10000);
        }
        break;
    }
    case LOC_SPEED:
        train_set_speed(args[0], args[1]);
        break;
    case LOC_REVERSE:
        train_reverse(args[0]);
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
        train_stop_at_sensor(args[2], args[0], args[1]);
        break;
    case SWITCH_TIME: {
        delay_sensor_any();
        int time = Time();
        delay_sensor_any();
        time = Time() - time;
        log("time interval took %d", time);
        break;
    }

    case GO:
        train_goto_location(args[0], args[1], args[2], args[3]);
        break;

    case SHORT_MOVE:
        train_short_move(args[0], args[1]);
        break;

    case CMD_BENCHMARK:
	Create(TASK_PRIORITY_MEDIUM_LOW - 1, bench_msg);
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

    case SIZES: {
        char buffer[512];
        char* ptr = log_start(buffer);
        ptr = sprintf(ptr,
                      "\n"
                      "Data:\n"
                      "    Kernel Hot: %u bytes\n"
                      "   Kernel Warm: %u bytes\n"
                      "          Task: %u bytes\n"
                      "         Total: %u bytes\n"
                      "Text:\r\n"
                      "    Kernel Hot: %u bytes\n"
                      "     Task Reg.: %u bytes\n"
                      "     Task Cold: %u bytes\n"
                      "         Total: %u bytes\n"
                      "BSS:            %u bytes\n"
                      "ROData:         %u bytes\n",
                      &_DataKernEnd     - &_DataStart,
                      &_DataKernWarmEnd - &_DataKernEnd,
                      &_DataEnd         - &_DataKernWarmEnd,
                      &_DataEnd         - &_DataStart,
                      &_TextKernEnd     - &_TextStart,
                      &_TextColdStart   - &_TextKernEnd,
                      &_TextEnd         - &_TextColdStart,
                      &_TextEnd         - &_TextStart,
                      &_BssEnd          - &_BssStart,
                      &_RODataEnd       - &_RODataStart);
        ptr = log_end(ptr);
        Puts(buffer, ptr - buffer);
        break;
    }
    case UPDATE_THRESHOLD:
        train_update_feedback_threshold(args[0], args[1]);
        break;

    case UPDATE_FEEDBACK:
        train_update_feedback_alpha(args[0], args[1]);
        break;

    case UPDATE_FUDGE_FACTOR:
        train_update_reverse_time_fudge(args[0], args[1]);
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

    case PATH_FIND: {
        int time = Time();

        pa_request req = {
            .type = PA_GET_PATH,
            .req  = {
                .requestor   = myTid(),
                .header      = 0,
                .sensor_to   = (short)sensor_to_pos(args[2], args[3]),
                .sensor_from = (short)sensor_to_pos(args[0], args[1]),
                .opts        = (short)PATH_NO_REVERSE_MASK | 0x3,
                .reserve     = 0
            }
        };

        int worker_tid;
        path_response res;

        Send(WhoIs((char*)PATH_ADMIN_NAME),
             (char*)&req, sizeof(req), (char*)&worker_tid, sizeof(worker_tid));
        Receive(&worker_tid, (char*)&res, sizeof(res));
        for (int i = res.size-1, j = 0; i >= 0; i--, j++) {
            switch(res.path[i].type) {
            case PATH_SENSOR: {
                const sensor print = pos_to_sensor(res.path[i].data.int_value);
                log("%d  \tS\t%c%d\t%d", j, print.bank, print.num, res.path[i].dist);
                break;
            }
            case PATH_TURNOUT:
                log("%d  \tT\t%d %c\t%d",
                    j, res.path[i].data.turnout.num, res.path[i].data.turnout.dir,
                    res.path[i].dist);
                break;
            case PATH_REVERSE:
                log("%d  \tR\t\t%d", j, res.path[i].dist);
                break;
            }
        }
        log("delta %d", Time() - time);
        Reply(worker_tid, NULL, 0);

        break;
    }

    case PATH_STEPS: {
        const int start_sensor = sensor_to_pos(args[0], args[1]);

        const track_location start_location = {
            .sensor = start_sensor,
            .offset = args[2] * 1000
        };

        track_location locs[NUM_SENSORS];

        // now find out our end position
        const int result = get_position_from(start_location,
                                             locs,
                                             args[3] * 1000);
        assert(result == 0, "Failed to scan ahead on the track (%d)", result);
        UNUSED(result);

        log("Trip distance %d mm", args[3]);

        // we need to walk the list and see what we have
        for (int i = 0; i < NUM_SENSORS; i++) {
            const sensor loc = pos_to_sensor(locs[i].sensor);
            log("At checkpoint %c%d there are %d mm left",
                loc.bank, loc.num, locs[i].offset / 1000);

            if (locs[i].offset <= 0 || locs[i].sensor == AN_EXIT) break;
        }

        break;
    }

    case TEST_TIME: {
        struct {
            tnotify_header head;
            int            number;
        } msg = {
            .head = {
                .type  = DELAY_RELATIVE,
                .ticks = args[0]
            },
            .number = 12
        };
        log ("%d", sizeof(msg));

        int ret = 4;
        int tid = Create(4, time_notifier);

        Send(tid, (char*)&msg, sizeof(msg), NULL, 0);
        Receive(&tid, (char*)&ret, sizeof(ret));

        log("%d", ret);
        Reply(tid, NULL, 0);
    }   break;

    case STOP_OFFSET:
        train_update_stop_offset(args[0], args[1]);
        break;

    case CMD_RESERVE_NODE: {
        int res = reserve_section_term(args[1], args[2], args[0]);
        log("RESERVE: %d", res);
    }   break;

    case CMD_RELEASE_NODE: {
        int res = reserve_section_term(args[1], args[2], args[0]);
        log("RELEASE: %d", res);
    }   break;

    case CMD_LOOKUP_RESERVATION: {
        int res = reserve_who_owns_term(args[0], args[1]);
        log("LOOKUP: %d", res);
    }   break;

    case CMD_SENSOR_KILL: {
        disable_sensor_name(args[0], args[1]);
    }   break;
    
    case CMD_SENSOR_REVIVE: {
        revive_sensor_name(args[0], args[1]);
    }   break;

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
        if (buffer[0] == 'q') {
            log("Aborting track load...be careful!");
            break;
        }
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
