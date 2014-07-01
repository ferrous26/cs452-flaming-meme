
#include <std.h>
#include <debug.h>
#include <train.h>
#include <syscall.h>
#include <track_data.h>
#include <ui_constants.h>
#include <physics.h>

#include <tasks/term_server.h>
#include <tasks/train_server.h>
#include <tasks/train_station.h>

#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

#define NUM_TURNOUTS     22
#define NUM_SENSORS      (5*16)
#define SENSOR_LIST_SIZE 9

static int mission_control_tid;

typedef struct {
    sensor_name* insert;
    sensor_name  recent_sensors[SENSOR_LIST_SIZE];
    track_node   track[TRACK_MAX];

    int track_loaded;
    int wait_all;
    int sensor_delay[NUM_SENSORS];
    int turnouts[NUM_TURNOUTS];

    int  trains[NUM_TRAINS];
    bool horn[NUM_TRAINS];

    struct {
        int horn;
        int speed;
        int light;
        int reverse;
    }   pickup[NUM_TRAINS];
    int drivers[NUM_TRAINS];
} mc_context;

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

static inline int __attribute__ ((const))
train_to_pos(const int train) {
    switch (train) {
    case 43: return 0;
    case 45: return 1;
    }
    if (train >= 47 && train <=51) {
        return train - 47 + 2;
    }
    return -1;
}

static inline int __attribute__ ((const))
pos_to_train(const int pos) {
    switch (pos) {
    case 0: return 43;
    case 1: return 45;
    }

    if (pos >= 2  && pos <= 6) {
        return pos + 47 - 2;
    }
    return -1;
}

static inline int __attribute__ ((const))
turnout_to_pos(const int turnout) {
    if (turnout <  1)   return -1;
    if (turnout <= 18)  return turnout - 1;
    if (turnout <  153) return -1;
    if (turnout <= 156) return turnout - (153-18);
    return -1;
}

static inline int __attribute__ ((const, unused))
pos_to_turnout(const int pos) {
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
        .type = MC_U_SENSOR
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
                    req.payload.sensor.bank = (short)('A' + (bank>>1));
                    req.payload.sensor.num  = (short)(i + 8 * (bank & 1));
                    Send(ptid, (char*)&req, sizeof(req), NULL, 0);
                }
            }

            sensor_state[bank] = c;
        }
    }
}

static void mc_try_send_train(mc_context* const ctxt, const int train_index) {
    train_req req;

    if (-1 == ctxt->drivers[train_index]) return;
    if (ctxt->pickup[train_index].reverse) {
        req.type                          = TRAIN_REVERSE_DIRECTION;
        ctxt->pickup[train_index].reverse = 0;

    } else if (ctxt->pickup[train_index].speed >= 0) {
        req.type                        = TRAIN_CHANGE_SPEED;
        req.arg                         = ctxt->pickup[train_index].speed;
        ctxt->pickup[train_index].speed = -1;

    } else if (ctxt->pickup[train_index].light) {
        req.type                        = TRAIN_TOGGLE_LIGHT;
        ctxt->pickup[train_index].light = 0;

    } else if (ctxt->pickup[train_index].horn) {
        req.type                       = TRAIN_HORN_SOUND;
        ctxt->pickup[train_index].horn = 0;

    } else { return; }

    Reply(ctxt->drivers[train_index], (char*)&req, sizeof(req));
    int result = Reply(ctxt->drivers[train_index], (char*)&req, sizeof(req));
    if (!result) ABORT("Failed to send to train %d (%d)", train_index, result);
    ctxt->drivers[train_index] = -1;
}

static int get_next_sensor(const track_node* node,
                           const int turnouts[NUM_TURNOUTS],
                           const track_node** rtrn) {
    int dist = 0;

    do {
        const int index = node->type == NODE_BRANCH ?
                          turnouts[turnout_to_pos(node->num)] :
                          DIR_AHEAD;

        dist += node->edge[index].dist;
        node  = node->edge[index].dest;
        if (node->type == NODE_EXIT) {
        	dist = -dist;
        	break;
        }
    } while (node->type != NODE_SENSOR);

    *rtrn = node;
    return dist;
}

inline static void mc_update_sensors(mc_context* const ctxt,
                                     const sensor_name* const sensor) {

    sensor_name* next = ctxt->insert++;
    memcpy(next, sensor, sizeof(sensor_name));
    if(ctxt->insert == ctxt->recent_sensors + SENSOR_LIST_SIZE) {
        ctxt->insert = ctxt->recent_sensors;
    }
    memset(ctxt->insert, 0, sizeof(sensor_name));

    const int sensor_pos = sensorname_to_pos(sensor->bank, sensor->num);
    const int waiter     = ctxt->sensor_delay[sensor_pos];

    if (-1 != waiter) {
        Reply(waiter, (char*)sensor, sizeof(sensor_name));
        ctxt->sensor_delay[sensor_pos] = -1;
    }
    if (-1 != ctxt->wait_all) {
    	const int time = Time();
        Reply(ctxt->wait_all, (char*)&time, sizeof(time));
        ctxt->wait_all = -1;
    }

    track_node* node = &ctxt->track[sensor_pos];

    if (ctxt->track_loaded) {
        const track_node* node_next;
        int dist = get_next_sensor(node, ctxt->turnouts, &node_next);
        log("%d %s\t--(%dmm)-->\t%d %s",
            node->num, node->name, dist,
            node_next->num, node_next->name);
    }

    char buffer[64];
    for(int i =0; next->bank != 0; i++) {
        const char* const output = next->num < 10 ? "%c 0%d": "%c %d";
        char*                pos = vt_goto(buffer, SENSOR_ROW+i, SENSOR_COL);

        pos = sprintf(pos, output, next->bank, next->num);
        Puts(buffer, pos-buffer);

        if (next == ctxt->recent_sensors) {
            next = ctxt->recent_sensors + SENSOR_LIST_SIZE-1;
        } else {
            next--;
        }
    }
}

static inline void mc_sensor_delay(mc_context* const ctxt,
                                   const int tid,
                                   const sensor_name* const sensor) {

    const int sensor_pos  = sensorname_to_pos(sensor->bank, sensor->num);
    const int task_waiter = ctxt->sensor_delay[sensor_pos];

    if (-1 != task_waiter) {
        log("%d kicked out task %d from sensor %c%d",
            task_waiter, tid, sensor->bank, sensor->num);
        Reply(task_waiter, NULL, 0);
    }
    ctxt->sensor_delay[sensor_pos] = tid;
}

static inline void mc_sensor_delay_any(mc_context* const ctxt, const int tid) {
    if (-1 != ctxt->wait_all) {
        int result = Reply(ctxt->wait_all, NULL, 0);
        if (!result) {
            log("task %d kicked out %d from the wait all queue",
                tid, ctxt->wait_all);
        }
    }

    ctxt->wait_all = tid;
}

static inline void mc_update_turnout(mc_context* const ctxt,
                                     const int turn_num,
                                     const int turn_state) {
    char state;
    char* ptr;
    char buffer[32];
    const int pos = turnout_to_pos(turn_num);

    assert(pos >= 0, "%d", turn_num)

    if (pos < 18) {
        ptr = vt_goto(buffer,
		      TURNOUT_ROW + (pos>>2),
		      TURNOUT_COL + (mod2_int(pos, 4) * 6));
    } else {
        ptr = vt_goto(buffer,
		      TURNOUT_ROW + 5,
		      TURNOUT_COL + (mod2_int(pos - 18, 4) * 6));
    }

    switch(turn_state) {
    case 'c': case 'C':
        state = TURNOUT_CURVED;
        ptr   = sprintf_string(ptr, COLOUR(MAGENTA) "C" COLOUR_RESET);
        ctxt->turnouts[pos] = DIR_CURVED;
        break;
    case 's': case 'S':
        state = TURNOUT_STRAIGHT;
        ptr   = sprintf_string(ptr, COLOUR(CYAN) "S" COLOUR_RESET);
        ctxt->turnouts[pos] = DIR_STRAIGHT;
        break;
    default:
        log("Invalid Turnout State %d", turn_state);
        return;
    }

    int result = put_train_turnout((char)turn_num, (char)state);
    assert(result == 0,
           "Failed setting turnout %d to %c", turn_num, turn_state);
    UNUSED(result);

    Puts(buffer, ptr - buffer);
}

static inline void mc_spawn_train_drivers() {
    for (int i=0; i < NUM_TRAINS; i++) {
        int train_num = pos_to_train(i);

        log("Spawning Train %d", train_num);

        int setup[2] = {train_num, i};
        int tid      = Create(5, train_driver);
        assert(tid > 0, "Failed to create train driver (%d)", tid);

        tid = Send(tid, (char*)&setup, sizeof(setup), NULL, 0);
        assert(tid == 0, "Failed to send to train %d (%d)", train_num, tid);
    }
}

static inline void mc_update_train_speed(mc_context* const ctxt,
                                         const int index,
                                         const int speed) {
     ctxt->pickup[index].speed = speed;
     mc_try_send_train(ctxt, index);
}

static inline void mc_toggle_light(mc_context* const ctxt, const int index) {
    ctxt->pickup[index].light ^= 1;
    mc_try_send_train(ctxt, index);
}

static inline void mc_toggle_horn(mc_context* const ctxt, const int index) {
    ctxt->pickup[index].horn ^= 1;
    mc_try_send_train(ctxt, index);
}

static inline void mc_reverse_train(mc_context* const ctxt, const int index) {
    ctxt->pickup[index].reverse ^= 1;
    mc_try_send_train(ctxt, index);
}

static void mc_reset_track_state(mc_context* const context) {
    for (int i = 1; i < 19; i++) {
        mc_update_turnout(context, i, 'C');
    }
    mc_update_turnout(context, 153, 'S');
    mc_update_turnout(context, 154, 'C');
    mc_update_turnout(context, 155, 'S');
    mc_update_turnout(context, 156, 'C');
}

static void mc_load_track(mc_context* const ctxt, int track_num) {
    switch (track_num) {
    case 'a': case 'A':
        init_tracka(ctxt->track);
        break;
    case 'b': case 'B':
        init_trackb(ctxt->track);
        break;
    default:
        log("Invalid track name `%c'", track_num);
        return;
    }

    ctxt->track_loaded = 1;
    log("loading track %c ...", track_num);
}

static inline void __attribute__((always_inline)) mc_flush_sensors() {
    Putc(TRAIN, SENSOR_POLL);
    for (int i = 0; i < 10; i++) { Getc(TRAIN); }
}

static inline void mc_init_context(mc_context* const ctxt) {
    memset(ctxt,                0, sizeof(*ctxt));
    memset(ctxt->drivers,      -1, sizeof(ctxt->drivers));
    memset(ctxt->sensor_delay, -1, sizeof(ctxt->sensor_delay));

    ctxt->wait_all = -1;
    ctxt->insert   = ctxt->recent_sensors;
}

static inline void mc_initalize(mc_context* const ctxt) {
    mc_init_context(ctxt);
    train_ui();

    mc_reset_track_state(ctxt);
    mc_spawn_train_drivers();
    mc_flush_sensors();

    int tid = Create(15, sensor_poll);
    if (tid < 0)
        ABORT("Mission Control failed creating sensor poll (%d)", tid);
}

void mission_control() {
    mc_req     req;
    mc_context context;
    mission_control_tid = myTid();

    log("Initalizing Mission Control (%d)", Time());
    Putc(TRAIN, TRAIN_ALL_STOP);
    Putc(TRAIN, TRAIN_ALL_STOP);
    Putc(TRAIN, TRAIN_ALL_START);

    int tid = RegisterAs((char*)MISSION_CONTROL_NAME);
    assert(tid == 0, "Mission Control has failed to register (%d)", tid);

    physics_init();

    mc_initalize(&context);
    log("Mission Control Has Initalized (%d)", Time());

    FOREVER {
        int result = Receive(&tid, (char*)&req, sizeof(req));
        assert(req.type < MC_TYPE_COUNT, "Invalid MC Request %d", req.type);

        switch (req.type) {
        case MC_U_SENSOR:
            mc_update_sensors(&context, &req.payload.sensor);
	    break;
        case MC_D_SENSOR:
            mc_sensor_delay(&context, tid, &req.payload.sensor);
            continue;
        case MC_D_SENSOR_ANY:
            mc_sensor_delay_any(&context, tid);
            continue;

        case MC_U_TURNOUT:
            mc_update_turnout(&context,
                              req.payload.turnout.num,
                              req.payload.turnout.state);
            break;
        case MC_R_TRACK:
            mc_reset_track_state(&context);
            break;

        case MC_U_TRAIN_SPEED:
            mc_update_train_speed(&context,
                                  req.payload.train_speed.num,
                                  req.payload.train_speed.speed);
            break;
        case MC_U_TRAIN_REVERSE:
            mc_reverse_train(&context, req.payload.int_value);
            break;
        case MC_T_TRAIN_LIGHT:
            mc_toggle_light(&context, req.payload.int_value);
            break;
        case MC_T_TRAIN_HORN:
            mc_toggle_horn(&context, req.payload.int_value);
            break;

        case MC_L_TRACK:
            mc_load_track(&context, req.payload.int_value);
            break;

        case MC_TD_CALL:
            context.drivers[req.payload.int_value] = tid;
            mc_try_send_train(&context, req.payload.int_value);
            continue;
        case MC_TYPE_COUNT:
            break;
        }

        result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("Failed to reply to mission control req %d (%d)",
                  req.type, result);
    }
}

int update_turnout(int num, int state) {
    int pos = turnout_to_pos(num);
    if (pos < 0) {
        log("Invalid Turnout Number %d", num);
        return 0;
    }

    switch (state) {
    case 'c': state = 'C';
    case 'C': break;
    case 's': state = 'S';
    case 'S': break;
    default:
        log("Invalid turnout direction (%c)", state);
        return 0;
    }

    mc_req req = {
        .type = MC_U_TURNOUT,
        .payload.turnout = {
            .num   = (short)num,
            .state = (short)state
        }
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_set_speed(int train, int speed) {
    int train_index = train_to_pos(train);

    if (train_index < 0) {
        log("Can't toggle horn of invalid train %d", train);
        return 0;
    }
    if (speed < 0 || speed >= TRAIN_REVERSE) {
        log("can't set train number %d to invalid speed %d", train, speed);
        return 0;
    }

    mc_req req = {
        .type = MC_U_TRAIN_SPEED,
        .payload.train_speed = {
            .num   = (short)train_index,
            .speed = (short)speed
        }
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int reset_train_state() {
    mc_req req = {
        .type = MC_R_TRACK,
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_toggle_light(int train) {
    int train_index = train_to_pos(train);
    if (train_index < 0) {
        log("Can't toggle light of invalid train %d", train);
        return 0;
    }

    mc_req req = {
        .type              = MC_T_TRAIN_LIGHT,
        .payload.int_value = train_index
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_toggle_horn(int train) {
    int train_index = train_to_pos(train);
    if (train_index < 0) {
        log("Can't toggle horn of invalid train %d", train);
        return 0;
    }

    mc_req req = {
        .type              = MC_T_TRAIN_HORN,
        .payload.int_value = train_index
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
        .type           = MC_D_SENSOR,
        .payload.sensor = {
            .bank = (short)sensor_bank,
            .num  = (short)sensor_num
        }
    };

    int time;
    int res = Send(mission_control_tid,
                   (char*)&req, sizeof(req),
                   (char*)&time, sizeof(time));

   if (res < 0) return res;
   return time;
}

int delay_all_sensor(sensor_name* const sensor) {
    mc_req req = {
        .type           = MC_D_SENSOR_ANY,
    };

    return Send(mission_control_tid,
                (char*)&req, sizeof(req),
                (char*)&sensor, sizeof(sensor_name));
}

int train_reverse(int train) {
    int train_index = train_to_pos(train);
    if (train_index < 0) {
        log("Can't Reverse Invalid Train %d", train);
    	return 0;
    }

    mc_req req = {
        .type              = MC_U_TRAIN_REVERSE,
        .payload.int_value = train_index
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int load_track(int track_value) {
    mc_req req = {
        .type              = MC_L_TRACK,
        .payload.int_value = track_value
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}
