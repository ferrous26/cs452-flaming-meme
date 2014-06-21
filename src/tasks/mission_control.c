
#include <std.h>
#include <debug.h>
#include <train.h>
#include <syscall.h>

#include <tasks/train_server.h>
#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/mission_control.h>

typedef struct {
    short bank;
    short num;
} sensor_name;

typedef struct {
    short num;
    short state;
} turnout_state;

typedef enum {
    TURNOUT_UPDATE,
    SENSOR_UPDATE
} mc_type;

typedef union {
    sensor_name   sensor;
    turnout_state turnout;
} mc_payload;

typedef struct {
    mc_type    type;
    mc_payload payload;
} mc_req;

#define TRAIN_ROW           6
#define TRAIN_COL           1
#define TRAIN_SPEED_COL     (TRAIN_COLUMN + 7)
#define TRAIN_DIRECTION_COL (TRAIN_SPEED_COLUMN + 7)
#define TRAIN_LIGHT_COL     (TRAIN_DIRECTION_COLUMN + 6)
#define TRAIN_HORN_COL      (TRAIN_LIGHT_COLUMN + 2)
#define TRAIN_SFX_COL       (TRAIN_HORN_COLUMN + 2)

#define TURNOUT_ROW TRAIN_ROW
#define TURNOUT_COL 36

#define SENSOR_ROW (TRAIN_ROW - 1)
#define SENSOR_COL 64

static void train_ui() {
    char buffer[1024];
    char* ptr = buffer;

    ptr = vt_goto(ptr, TRAIN_ROW - 2, TRAIN_COL);
    ptr = sprintf_string(ptr,
"Train  Speed  Dir.  Special            Turnouts             __Sensor__\n"
"---------------------------    +-----------------------+    |        | Newest\n"
" 43                            | 1   | 2   | 3   | 4   |    |        |\n"
" 45                            | 5   | 6   | 7   | 8   |    |        |\n"
" 47                            | 9   |10   |11   |12   |    |        |\n"
" 48                            |13   |14   |15   |16   |    |        |\n"
" 49                            |17   |18   |-----------|    |        |\n"
" 50                            |19    20   |21    22   |    |        |\n"
" 51                            +-----------------------+    |        | Oldest");

    Puts(buffer, ptr - buffer);
}

static int __attribute__((const, unused)) train_to_pos(const int train) {
    if (train >= 43 && train <=51) {
        return train - 43;
    }
    return -1;
}

static int __attribute__((const, unused)) pos_to_train(const int pos) {
    if (pos >= 0 && pos <=8) {
        return pos + 43;
    }
    return -1;
}

static int __attribute__((const, unused)) turnout_to_pos(const int turnout) {
    if (turnout <  1)   return -1;
    if (turnout <= 22)  return turnout - 1;
    if (turnout <  153) return -1;
    if (turnout <= 156) return turnout - (153-18);
    return -1;
}

static int __attribute__((const, unused)) pos_to_turnout(const int pos) {
    if (pos < 0)  return -1;
    if (pos < 18) return pos + 1;
    if (pos < 22) return pos + (153-18);
    return -1;
}

static void __attribute__ ((noreturn)) sensor_poll() {
    RegisterAs((char*)SENSOR_POLL_NAME);
    Putc(TRAIN, SENSOR_RESET);
    Putc(TRAIN, SENSOR_POLL);

    int       sensor_state[10];
    const int ptid = myParentTid();
    mc_req    req = {
        .type = SENSOR_UPDATE
    };

    for (int i = 0; i < 10; i++) {
        sensor_state[i] = Getc(TRAIN);
    }

    FOREVER {
	Delay(10);

        Putc(TRAIN, SENSOR_POLL);
        for (int bank = 0; bank < 10; bank++) {
            int c = Getc(TRAIN);
            assert(c >= 0, "sensor_poll got bad return (%d)", c);

            for (int mask = 0x80, i = 1; mask > 0; mask = mask >> 1, i++) {
                if((c & mask) > (sensor_state[bank] & mask)) {
                    req.payload.sensor.bank = 'A' + (bank>>1);
                    req.payload.sensor.num  =  i + 8 * (bank & 1);
                    Send(ptid, (char*)&req, sizeof(req), NULL, 0);
                }
            }

            sensor_state[bank] = c;
        }
    }
}

#define SENSOR_LIST_SIZE 9
#define NUM_TURNOUTS 22

typedef struct {
    sensor_name* insert;
    sensor_name  recent_sensors[SENSOR_LIST_SIZE];

    int turnouts[NUM_TURNOUTS];
} mc_context;

inline static void mc_update_sensors(mc_context* const ctxt,
                                     const sensor_name* const sensor) {

    sensor_name* next = ctxt->insert++;
    memcpy(next, sensor, sizeof(sensor));
    if(ctxt->insert == ctxt->recent_sensors + SENSOR_LIST_SIZE) {
        ctxt->insert = ctxt->recent_sensors;
    }
    memset(ctxt->insert, 0, sizeof(ctxt->insert));

    char buffer[16];
    for(int i = 0; next->bank != 0; i++) {
        char* output = next->num < 10 ? "%c 0%d": "%c %d";
        char* pos = vt_goto(buffer, SENSOR_ROW+i, SENSOR_COL);

        pos = sprintf(pos, output, next->bank, next->num);
        Puts(buffer, pos-buffer);

        if (next == ctxt->recent_sensors) {
            next = ctxt->recent_sensors + SENSOR_LIST_SIZE-1;
        } else {
            next--;
        }
    }
}

static void mc_update_turnout(mc_context* const ctxt,
                              const int         turn_num,
                              int               turn_state) {

    assert(turn_num >= 0, "Bad turnout number (%d)", turn_num);

    int state;
    char buffer[32];
    char* ptr;

    if (turn_num < 18)
        ptr = vt_goto(buffer,
		      TURNOUT_ROW + (turn_num>>2),
		      TURNOUT_COL + (mod2(turn_num, 4) * 6));
    else
        ptr = vt_goto(buffer,
		      TURNOUT_ROW + 5,
		      TURNOUT_COL + (mod2(turn_num - 18, 4) * 6));


    switch(turn_state) {
    case 'c':
	turn_state = 'C';
	// fall through
    case 'C':
	state = TURNOUT_CURVED;
	ptr   = sprintf_string(ptr, COLOUR(MAGENTA) "C" COLOUR_RESET);
	break;
    case 's':
	turn_state = 'S';
	// fall through
    case 'S':
	state = TURNOUT_STRAIGHT;
	ptr   = sprintf_string(ptr, COLOUR(CYAN) "S" COLOUR_RESET);
	break;
    default:
        log("Invalid Turnout State %c", turn_state);
        return;
    }

    ctxt->turnouts[turn_num] = turn_state;

    int result = put_train_turnout(state, pos_to_turnout(turn_num));
    assert(result == 0, "Failed setting turnout %d to %c", turn_num, turn_state);
    UNUSED(result);

    Puts(buffer, ptr-buffer);
}

static void reset_train_state(mc_context* context) {
    // Kill any existing command so we're in a good state
    Putc(TRAIN, TRAIN_ALL_STOP);
    Putc(TRAIN, TRAIN_ALL_STOP);

    // Want to make sure the contoller is on
    Putc(TRAIN, TRAIN_ALL_START);

    for (int i = 0; i < 18; i++) {
        mc_update_turnout(context, i, 'C');
    }
    mc_update_turnout(context, 18, 'S');
    mc_update_turnout(context, 19, 'C');
    mc_update_turnout(context, 20, 'S');
    mc_update_turnout(context, 21, 'C');
    mc_update_turnout(context, 13, 'S');

    for (int i = 0;; i++) {
        int pos = pos_to_train(i);

        if(pos < 0) break;
        put_train_cmd(pos, 0);
    }
}


static int mission_control_tid;

void mission_control() {
    mc_context context;
    memset(&context, 0, sizeof(context));

    mission_control_tid = myTid();

    int tid = RegisterAs((char*)MISSION_CONTROL_NAME);
    assert(tid == 0, "Mission Control has failed to register (%d)", tid);

    train_ui();
    reset_train_state(&context); // this MUST run before the following

    tid = Create(15, sensor_poll);
    if (tid < 0) ABORT("Mission Control failed creating sensor poll (%d)", tid);

    mc_req req;
    context.insert = context.recent_sensors;

    FOREVER {
        int result = Receive(&tid, (char*)&req, sizeof(req));

        switch (req.type) {
        case SENSOR_UPDATE:
            mc_update_sensors(&context, &req.payload.sensor);
            result = Reply(tid, NULL, 0);
	    if (result < 0) ABORT("Failed to reply to sensor notifier (%d)",
				  result);
            break;

        case TURNOUT_UPDATE:
            mc_update_turnout(&context,
                              req.payload.turnout.num,
                              req.payload.turnout.state);
            Reply(tid, NULL, 0);
            break;
        }
    }
}

int update_turnout(int num, int state) {

    int pos = turnout_to_pos(num);

    if (pos < 0) {
        log("Invalid Turnout Number %d", num);
        return 0;
    }

    mc_req req = {
        .type = TURNOUT_UPDATE,
        .payload.turnout = {
            .num   = pos,
            .state = state
        }
    };


    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}
