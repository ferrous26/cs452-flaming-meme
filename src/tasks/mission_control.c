
#include <ui.h>
#include <std.h>
#include <debug.h>
#include <train.h>
#include <syscall.h>
#include <track_data.h>

#include <tasks/term_server.h>
#include <tasks/train_server.h>
#include <tasks/train_driver.h>

#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

#define NUM_TURNOUTS         22
#define NUM_SENSOR_BANKS     5
#define NUM_SENSORS_PER_BANK 16
#define NUM_SENSORS          (NUM_SENSOR_BANKS * NUM_SENSORS_PER_BANK)
#define SENSOR_LIST_SIZE     9

static int mission_control_tid;

typedef struct {
    sensor_name* sensor_insert;
    sensor_name  recent_sensors[SENSOR_LIST_SIZE];
    track_node   track[TRACK_MAX];

    int wait_all;
    int track_loaded;
    int sensor_delay[NUM_SENSORS];
    int turnouts[NUM_TURNOUTS];
} mc_context;

static void train_ui() {
    char buffer[1024];
    char* ptr = buffer;

    ptr = vt_goto(ptr, TRAIN_ROW - 2, TRAIN_COL);
    ptr = sprintf_string(ptr,
"Train  Speed    Sensors          Turnouts/Gates/Switches     __Sensor__\n"
"----------------------------    +-----------------------+    |        | Newest\n"
" 43                             | 1   | 2   | 3   | 4   |    |        |\n"
" 45                             | 5   | 6   | 7   | 8   |    |        |\n"
" 47                             | 9   |10   |11   |12   |    |        |\n"
" 48                             |13   |14   |15   |16   |    |        |\n"
" 49                             |17   |18   |-----------|    |        |\n"
" 50                             |153   154  |155   156  |    |        |\n"
" 51                             +-----------------------+    |        | Oldest");
    Puts(buffer, ptr - buffer);
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

static inline int __attribute__ ((const, always_inline, unused))
sensorname_to_num(const int bank, const int num) {
    return ((bank-'A') << 4) + (num-1);
}

static inline sensor_name __attribute__ ((const, always_inline))
sensornum_to_name(const int num) {
    sensor_name name = {
        .bank = 'A' + (num >>4),
        .num  = (num & 15) + 1
    };
    return name;
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
    for (int i = 0; i < NUM_SENSOR_BANKS; i++) {
        // set up the inital state
        sensor_state[i] = get_train_bank();
    }

    FOREVER {
        for (int bank = 0; bank < NUM_SENSOR_BANKS; bank++) {
            Putc(TRAIN, SENSOR_RESET + 1 + bank);
            int c = get_train_bank();
            assert(c >= 0, "sensor_poll got bad return (%d)", c);

            for (int mask = 0x8000, i = 0; mask > 0; mask = mask >> 1, i++) {
                if ((c & mask) > (sensor_state[bank] & mask)) {
                    req.payload.int_value = (bank<<4) + i;
                    Send(ptid, (char*)&req, sizeof(req), NULL, 0);
                }
            }

            sensor_state[bank] = c;
        }
    }
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

static void mc_get_next_sensor(mc_context* const ctxt,
                               const int sensor_num,
                               const int tid) {
    assert(sensor_num >= 0 && sensor_num < NUM_SENSORS,
           "can't get next from invalid sensor %d", sensor_num);

    const track_node* const node_curr = &ctxt->track[sensor_num];
    const track_node* node_next;
    int               response[2];

    response[0] = get_next_sensor(node_curr, ctxt->turnouts, &node_next);
    response[1] = node_next->num;

    int result = Reply(tid, (char*)&response, sizeof(response));
    if (result) ABORT("Train driver died! (%d)", result);
}

inline static void mc_update_sensors(mc_context* const ctxt,
                                     const int sensor_num) {
    assert(sensor_num >= 0 && sensor_num < NUM_SENSORS,
           "Can't update invalid sensor num %d", sensor_num);

    sensor_name* next = ctxt->sensor_insert++;
    *next             = sensornum_to_name(sensor_num);
    const int waiter  = ctxt->sensor_delay[sensor_num];

    if(ctxt->sensor_insert == ctxt->recent_sensors + SENSOR_LIST_SIZE) {
        ctxt->sensor_insert = ctxt->recent_sensors;
    }
    memset(ctxt->sensor_insert, 0, sizeof(sensor_name));

    if (-1 != waiter) {
    	const int time                 = Time();
        ctxt->sensor_delay[sensor_num] = -1;

        Reply(waiter, (char*)&time, sizeof(time));
    } else if (-1 != ctxt->wait_all) {
        Reply(ctxt->wait_all, (char*)&sensor_num, sizeof(sensor_num));
        ctxt->wait_all = -1;
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
                                   const int sensor_num) {
    assert(sensor_num >= 0 && sensor_num < NUM_SENSORS,
           "[MISSION CONTROL] can't delay on invalid sensor %d", sensor_num);

    const int task_waiter = ctxt->sensor_delay[sensor_num];
    if (-1 != task_waiter) {
        log("[MISSION CONTROL] %d kicked out task %d from sensor %d",
            task_waiter, tid, sensor_num);
        Reply(task_waiter, NULL, 0);
    }
    ctxt->sensor_delay[sensor_num] = tid;
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
    memset(ctxt->sensor_delay, -1, sizeof(ctxt->sensor_delay));

    ctxt->wait_all      = -1;
    ctxt->sensor_insert = ctxt->recent_sensors;
}

static inline void mc_initalize(mc_context* const ctxt) {
    mc_init_context(ctxt);
    train_ui();

    mc_reset_track_state(ctxt);
    mc_flush_sensors();

    int tid = Create(15, sensor_poll);
    if (tid < 0)
        ABORT("Mission Control failed creating sensor poll (%d)", tid);
}

void mission_control() {
    mc_req     req;
    mc_context context;
    mission_control_tid = myTid();

    log("[MISSION CONTROL] %d - Initalizing", Time());
    Putc(TRAIN, TRAIN_ALL_STOP);
    Putc(TRAIN, TRAIN_ALL_STOP);
    Putc(TRAIN, TRAIN_ALL_START);

    int tid = RegisterAs((char*)MISSION_CONTROL_NAME);
    assert(tid == 0, "Mission Control has failed to register (%d)", tid);

    mc_initalize(&context);
    log("[Mission Control] %d - Ready!", Time());

    FOREVER {
        int result = Receive(&tid, (char*)&req, sizeof(req));
        assert(req.type < MC_TYPE_COUNT, "Invalid MC Request %d", req.type);

        switch (req.type) {
        case MC_U_SENSOR:
            mc_update_sensors(&context, req.payload.int_value);
	    break;
        case MC_D_SENSOR:
            mc_sensor_delay(&context, tid, req.payload.int_value);
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
        case MC_L_TRACK:
            mc_load_track(&context, req.payload.int_value);
            break;

        case MC_TD_GET_NEXT_SENSOR:
            mc_get_next_sensor(&context, req.payload.int_value, tid);
            continue;

        case MC_TYPE_COUNT:
            break;
        }

        result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("[Mission Control] failed replying to %d (%d)",
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

int reset_train_state() {
    mc_req req = {
        .type = MC_R_TRACK,
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
        .type              = MC_D_SENSOR,
        .payload.int_value = ((sensor_bank-'A') << 4) + (sensor_num - 1)
    };

    int time;
    int res = Send(mission_control_tid,
                   (char*)&req, sizeof(req),
                   (char*)&time, sizeof(time));

   if (res < 0) return res;
   return time;
}

int delay_all_sensor() {
    mc_req req = {
        .type           = MC_D_SENSOR_ANY,
    };

    int sensor;
    int result = Send(mission_control_tid,
                      (char*)&req, sizeof(req),
                      (char*)&sensor, sizeof(sensor));

    if (result < 0) return result;
    return sensor;
}

int load_track(int track_value) {
    switch (track_value) {
    case 'A': track_value = 'a';
    case 'a': break;
    case 'B': track_value = 'b';
    case 'b': break;
    default:
        log("failed loading track, no such track %c", track_value);
        return 1;
    }

    mc_req req = {
        .type              = MC_L_TRACK,
        .payload.int_value = track_value
    };

    return Send(mission_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int get_sensor_from(int from, int* const res_dist, int* const res_name) {
    int    values[2];
    mc_req req = {
        .type              = MC_TD_GET_NEXT_SENSOR,
        .payload.int_value = from
    };

    int result = Send(mission_control_tid,
                      (char*)&req, sizeof(req),
                      (char*)&values, sizeof(values));
    if (result < 0) return result;

    *res_dist = values[0];
    *res_name = values[1];

    return 0;
}
