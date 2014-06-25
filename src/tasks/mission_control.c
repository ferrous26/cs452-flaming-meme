
#include <std.h>
#include <debug.h>
#include <train.h>
#include <syscall.h>
#include <track_data.h>

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

typedef struct {
    short num;
    short speed;
} train_speed;

typedef enum {
    TRACK_LOAD,

    SENSOR_UPDATE,
    TURNOUT_UPDATE,
    TRAIN_SPEED_UPDATE,
    TRAIN_TOGGLE_LIGHT,
    TRAIN_TOGGLE_HORN,
    RESET_STATE,

    SENSOR_DELAY,
    SENSOR_DELAY_ALL,

    MC_TYPE_COUNT
} mc_type;

typedef union {
    sensor_name   sensor;
    turnout_state turnout;
    train_speed   train_speed;
    int           int_value;
} mc_payload;

typedef struct {
    mc_type    type;
    mc_payload payload;
} mc_req;

#define TRAIN_ROW           6
#define TRAIN_COL           1
#define TRAIN_SPEED_COL     (TRAIN_COL + 8)
#define TRAIN_LIGHT_COL     (TRAIN_SPEED_COL + 6)
#define TRAIN_HORN_COL      (TRAIN_LIGHT_COL + 4)

#define TURNOUT_ROW TRAIN_ROW
#define TURNOUT_COL 30

#define SENSOR_ROW (TRAIN_ROW - 1)
#define SENSOR_COL 58

static void train_ui() {
    char buffer[1024];
    char* ptr = buffer;

    ptr = vt_goto(ptr, TRAIN_ROW - 2, TRAIN_COL);
    ptr = sprintf_string(ptr,
"Train  Speed  Special     Turnouts/Gates/Switches     __Sensor__\n"
"---------------------    +-----------------------+    |        | Newest\n"
" 43                      | 1   | 2   | 3   | 4   |    |        |\n"
" 45                      | 5   | 6   | 7   | 8   |    |        |\n"
" 47                      | 9   |10   |11   |12   |    |        |\n"
" 48                      |13   |14   |15   |16   |    |        |\n"
" 49                      |17   |18   |-----------|    |        |\n"
" 50                      |153   154  |155   156  |    |        |\n"
" 51                      +-----------------------+    |        | Oldest");
    Puts(buffer, ptr - buffer);
}

static int __attribute__ ((const)) train_to_pos(const int train) {
    switch (train) {
    case 43: return 0;
    case 45: return 1;
    }

    if (train >= 47 && train <=51) {
        return train - 47 + 2;
    }

    assert(false, "invalid train number %d", train);
    return -1;
}

static int __attribute__ ((const, unused)) pos_to_train(const int pos) {
    switch (pos) {
    case 0: return 43;
    case 1: return 45;
    }

    if (pos >= 2  && pos <= 6) {
        return pos + 47 - 2;
    }
    return -1;
}

static int __attribute__ ((const)) turnout_to_pos(const int turnout) {
    if (turnout <  1)   return -1;
    if (turnout <= 18)  return turnout - 1;
    if (turnout <  153) return -1;
    if (turnout <= 156) return turnout - (153-18);
    return -1;
}

static int __attribute__ ((const, unused)) pos_to_turnout(const int pos) {
    if (pos < 0)  return -1;
    if (pos < 18) return pos + 1;
    if (pos < 22) return pos + (153-18);
    return -1;
}

static int __attribute__ ((const, always_inline))
sensorname_to_pos(const int bank, const int num) {
    return ((bank-'A') << 4) + (num-1);
}

static void __attribute__ ((noreturn)) sensor_poll() {
    RegisterAs((char*)SENSOR_POLL_NAME);
    Putc(TRAIN, SENSOR_RESET);

    int       sensor_state[10];
    const int ptid = myParentTid();
    mc_req    req = {
        .type = SENSOR_UPDATE
    };

    Putc(TRAIN, SENSOR_POLL);
    for (int i = 0; i < 10; i++) {
        sensor_state[i] = Getc(TRAIN);
    }

    FOREVER {
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

#define NUM_TURNOUTS     22
#define NUM_TRAINS       7
#define SENSOR_LIST_SIZE 9

typedef struct {
    sensor_name* insert;
    sensor_name  recent_sensors[SENSOR_LIST_SIZE];
    track_node   track[TRACK_MAX];

    int          sensor_delay[5*16];
    int          wait_all;

    int   turnouts[NUM_TURNOUTS];
    int   trains[NUM_TRAINS];
    bool  horn[NUM_TRAINS];
} mc_context;

inline static void mc_update_sensors(mc_context* const ctxt,
                                     const sensor_name* const sensor) {

    sensor_name* next = ctxt->insert++;
    memcpy(next, sensor, sizeof(sensor));
    if(ctxt->insert == ctxt->recent_sensors + SENSOR_LIST_SIZE) {
        ctxt->insert = ctxt->recent_sensors;
    }
    memset(ctxt->insert, 0, sizeof(ctxt->insert));

    const int sensor_pos = sensorname_to_pos(sensor->bank, sensor->num);
    const int waiter = ctxt->sensor_delay[sensor_pos];
    
    if (-1 != waiter) {
        Reply(waiter, NULL, 0);
        ctxt->sensor_delay[sensor_pos] = -1;
    }

    if (-1 != ctxt->wait_all) {
        Reply(ctxt->wait_all, NULL, 0);
        ctxt->wait_all = -1;
    }


    char buffer[64];
    for(int i =0; next->bank != 0; i++) {
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

static void mc_sensor_delay(mc_context* const ctxt,
                            const int tid,
                            const sensor_name* const sensor) {
    const int sensor_pos = sensorname_to_pos(sensor->bank, sensor->num);
    assert(-1 == ctxt->sensor_delay[sensor_pos],
           "Task Already waiting at %c %d", sensor->bank, sensor->num);

    ctxt->sensor_delay[sensor_pos] = tid;
}


static void mc_update_turnout(mc_context* const ctxt,
                              const int         turn_num,
                              int               turn_state) {
    char state;
    char* ptr;
    char buffer[32];
    const int pos = turnout_to_pos(turn_num);
    assert(pos >= 0, "%d", turn_num)

    if (pos < 18) {
        ptr = vt_goto(buffer,
		      TURNOUT_ROW + (pos>>2),
		      TURNOUT_COL + (mod2(pos, 4) * 6));
    } else {
        ptr = vt_goto(buffer,
		      TURNOUT_ROW + 5,
		      TURNOUT_COL + (mod2(pos - 18, 4) * 6));
    }

    switch(turn_state) {
    case 'c': case 'C': 
        state = TURNOUT_CURVED;
        ptr   = sprintf_string(ptr, COLOUR(MAGENTA) "C" COLOUR_RESET);
        break;
    case 's': case 'S':
        state = TURNOUT_STRAIGHT;
        ptr   = sprintf_string(ptr, COLOUR(CYAN) "S" COLOUR_RESET);
        break;
    default:
        log("Invalid Turnout State %d", turn_state);
        return;
    }

    ctxt->turnouts[pos] = turn_state;

    int result = put_train_turnout((char)turn_num, (char)state);
    assert(result == 0, "Failed setting turnout %d to %c", turn_num, turn_state);
    UNUSED(result);

    Puts(buffer, ptr - buffer);
}

static void mc_update_train_speed(mc_context* ctxt,
                                  const int   tr_num,
                                  const int   tr_speed) {
    int pos = train_to_pos(tr_num);

    ctxt->trains[pos] = (tr_speed & 0xf) | (ctxt->trains[pos] & 0x10);
    put_train_cmd((char)tr_num, (char)ctxt->trains[pos]);

    char  buffer[16];
    char* ptr = vt_goto(buffer, TRAIN_ROW + pos, TRAIN_SPEED_COL);
    if (tr_speed == 0) {
        ptr = sprintf_string(ptr, "- ");
    } else {
        ptr = sprintf(ptr, "%d ", tr_speed);
    }

    Puts(buffer, ptr-buffer);
}

static void mc_toggle_light(mc_context* ctxt,
                            const int   tr_num) {
    int pos = train_to_pos(tr_num);

    ctxt->trains[pos] ^= TRAIN_LIGHT;
    put_train_cmd((char)tr_num, (char)ctxt->trains[pos]);

    char buffer[16];
    char* ptr = vt_goto(buffer, TRAIN_ROW + pos, TRAIN_LIGHT_COL);
    if (ctxt->trains[pos] & TRAIN_LIGHT)
	ptr = sprintf_string(ptr, COLOUR(YELLOW) "L" COLOUR_RESET);
    else
	*(ptr++) = ' ';

    Puts(buffer, ptr-buffer);
}

static void mc_toggle_horn(mc_context* ctxt,
			   const int   tr_num) {
    int pos = train_to_pos(tr_num);

    ctxt->horn[pos] ^= INT_MAX;
    put_train_cmd((char)tr_num,
                  ctxt->horn[pos] ? TRAIN_HORN : TRAIN_FUNCTION_OFF);

    char buffer[16];
    char* ptr = vt_goto(buffer, TRAIN_ROW + pos, TRAIN_HORN_COL);

    if (ctxt->horn[pos]) {
	ptr   = sprintf_string(ptr, COLOUR(RED) "H" COLOUR_RESET);
    } else {
	*(ptr++) = ' ';
    }
    Puts(buffer, ptr - buffer);
}

static void mc_reset_train_state(mc_context* context) {
    // Kill any existing command so we're in a good state
    Putc(TRAIN, TRAIN_ALL_STOP);
    Putc(TRAIN, TRAIN_ALL_STOP);

    // Want to make sure the contoller is on
    Putc(TRAIN, TRAIN_ALL_START);

    for (int i = 1; i < 19; i++) {
        mc_update_turnout(context, i, i == 14 ? 'S' : 'C');
    }
    mc_update_turnout(context, 153, 'S');
    mc_update_turnout(context, 154, 'C');
    mc_update_turnout(context, 155, 'S');
    mc_update_turnout(context, 156, 'C');
}

static int mission_control_tid;

void mission_control() {
    mc_context context;
    memset(&context, 0, sizeof(context));
    memset(context.sensor_delay, -1, sizeof(context.sensor_delay));
    context.wait_all = -1;

    mission_control_tid = myTid();

    int tid = RegisterAs((char*)MISSION_CONTROL_NAME);
    assert(tid == 0, "Mission Control has failed to register (%d)", tid);

    log("Initalizing Train (%d)", Time());
    train_ui();
    mc_reset_train_state(&context); // this MUST run before the following
    Putc(TRAIN, SENSOR_POLL);
    for (int i = 0; i < 10; i++) {
        Getc(TRAIN);
    }
    log("Train Has Initalized (%d)", Time());

    tid = Create(15, sensor_poll);
    if (tid < 0) ABORT("Mission Control failed creating sensor poll (%d)", tid);

    mc_req req;
    context.insert = context.recent_sensors;

    FOREVER {
        int result = Receive(&tid, (char*)&req, sizeof(req));
        assert(req.type < MC_TYPE_COUNT, "Invalid MC Request %d", req.type);

        switch (req.type) {
        case SENSOR_UPDATE:
            mc_update_sensors(&context, &req.payload.sensor);
	    break;
        case TURNOUT_UPDATE:
            mc_update_turnout(&context,
                              req.payload.turnout.num,
                              req.payload.turnout.state);
            break;
        case TRAIN_SPEED_UPDATE:
            mc_update_train_speed(&context,
                                  req.payload.train_speed.num,
                                  req.payload.train_speed.speed);
            break;
        case RESET_STATE:
            mc_reset_train_state(&context);
            break;
        case TRAIN_TOGGLE_LIGHT:
            mc_toggle_light(&context, req.payload.int_value);
            break;
        case TRAIN_TOGGLE_HORN:
            mc_toggle_horn(&context,  req.payload.int_value);
            break;
        case TRACK_LOAD:
            if(req.payload.int_value == 'a') {
                init_tracka(context.track);
                log("loading track a ...");
            } else if (req.payload.int_value == 'b') {
                init_trackb(context.track);
                log("loading track b ...");
            }
            break;
        
        case MC_TYPE_COUNT:
            break;
        case SENSOR_DELAY:
            mc_sensor_delay(&context, tid, &req.payload.sensor);
            continue;
        case SENSOR_DELAY_ALL:
            context.wait_all = tid;
            continue;
        }

        result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("Failed to reply to mission control req %d (%d)",
                  req.type, result);
    }
}

int update_turnout(int num, int state) {
    // TODO: do not wastefully do translation here and also in server
    int pos = turnout_to_pos(num);
    if (pos < 0) {
        log("Invalid Turnout Number %d", num);
        return 0;
    }

    switch (state) {
    case 'C':
    case 'S':
	break;
    case 'c':
	state = 'C';
        break;
    case 's':
	state = 'S';
        break;
    default:
        log("Invalid turnout direction (%c)", state);
        return 0;
    }

    mc_req req = {
        .type = TURNOUT_UPDATE,
        .payload.turnout = {
            .num   = (short)num,
            .state = (short)state
        }
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int update_train_speed(int train, int speed) {
    if (speed < 0 || speed > TRAIN_REVERSE) {
        log("can't set train number %d to invalid speed %d", train, speed);
        return 0;
    }

    mc_req req = {
        .type = TRAIN_SPEED_UPDATE,
        .payload.train_speed = {
            .num   = (short)train,
            .speed = (short)speed
        }
    };
    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int reset_train_state() {
    mc_req req = {
        .type = RESET_STATE,
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int toggle_train_light(int train) {
    mc_req req = {
        .type              = TRAIN_TOGGLE_LIGHT,
        .payload.int_value = train
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int toggle_train_horn(int train) {
    mc_req req = {
        .type              = TRAIN_TOGGLE_HORN,
        .payload.int_value = train
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int delay_sensor(int sensor_bank, int sensor_num) {

    switch (sensor_bank) {
    case 'a': sensor_bank = 'A';
    case 'A': break;

    case 'b': sensor_bank = 'B';
    case 'B': break;

    case 'c': sensor_bank = 'C';
    case 'C': break;

    case 'd': sensor_bank = 'D';
    case 'D': break;

    case 'e': sensor_bank = 'E';
    case 'E': break;
    default:
        log("Invalid sensor bank %c", sensor_bank);
        return -1;
    }

    if( sensor_num < 1 || sensor_num > 16 ) {
        log("Invalid sensor num %d", sensor_num);
        return -1;
    }

    mc_req req = {
        .type           = SENSOR_DELAY,
        .payload.sensor = {
            .bank   = (short)sensor_bank,
            .num    = (short)sensor_num
        }
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int delay_all_sensor() {
    mc_req req = {
        .type           = SENSOR_DELAY_ALL,
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int load_track(int track_value) {
    mc_req req = {
        .type              = TRACK_LOAD,
        .payload.int_value = track_value
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}


