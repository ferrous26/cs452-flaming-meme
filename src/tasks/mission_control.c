
#include <ui.h>
#include <std.h>
#include <debug.h>
#include <train.h>
#include <syscall.h>
#include <track_data.h>

#include <tasks/priority.h>
#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/train_server.h>

#include <tasks/path_admin.h>
#include <tasks/sensor_farm.h>
#include <tasks/train_blaster.h>
#include <tasks/track_reservation.h>

#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

#define LOG_HEAD               "[MISSION CONTROL] "
#define NUM_TURNOUTS           22
#define NUM_SENSOR_BANKS       5
#define NUM_SENSORS_PER_BANK   16
#define NUM_SENSORS           (NUM_SENSOR_BANKS * NUM_SENSORS_PER_BANK)
#define SENSOR_LIST_SIZE       9

#define CHECK_CREATE(tid, msg) if (tid < 0) ABORT(msg " (%d)", tid)


static int mission_control_tid;

typedef struct {
    track_node track[TRACK_MAX];
    int        turnouts[NUM_TURNOUTS];
} mc_context;

static void train_ui() {
    char buffer[1024];
    char* ptr = buffer;

    ptr = vt_goto(ptr, TRAIN_ROW - 2, TRAIN_COL);
    ptr = sprintf_string(ptr,
"Train  Speed    Sensors              Turnouts/Gates/Switches     __Sensor__\n"
"--------------------------------    +-----------------------+    |        | Newest\n"
"                                    | 1   | 2   | 3   | 4   |    |        |\n"
"                                    | 5   | 6   | 7   | 8   |    |        |\n"
"                                    | 9   |10   |11   |12   |    |        |\n"
"                                    |13   |14   |15   |16   |    |        |\n"
"                                    |17   |18   |-----------|    |        |\n"
"                                    |153   154  |155   156  |    |        |\n"
"                                    +-----------------------+    |        | Oldest");
    Puts(buffer, ptr - buffer);
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
    mc_update_turnout(context, 1, 'S');
    mc_update_turnout(context, 2, 'S');
    mc_update_turnout(context, 3, 'C');
    mc_update_turnout(context, 4, 'S');
    mc_update_turnout(context, 5, 'C');
    mc_update_turnout(context, 6, 'S'); // 6 frequently derails when curved

    for (int i = 7; i < 19; i++)
        mc_update_turnout(context, i, 'C');

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
        ABORT("Invalid track name `%c'", track_num);
        return;
    }
    log(LOG_HEAD "loading track %c ...", track_num);
}

static inline void __attribute__((always_inline)) mc_flush_sensors() {
    Putc(TRAIN, SENSOR_POLL);
    for (int i = 0; i < 10; i++) { Getc(TRAIN); }
}

static inline void mc_stall_for_track_load(mc_context* const ctxt) {
    int    tid, result;
    mc_req req;

    for(int track_loaded = 0; !track_loaded; ) {
        result = Receive(&tid, (char*)&req, sizeof(req));

        if (req.type ==  MC_L_TRACK) {
            mc_load_track(ctxt, req.payload.int_value);
            track_loaded = 1;
        } else {
            log(LOG_HEAD "can not perfrom request %d "
                "until a track has been loaded", req.type);
        }

        result = Reply(tid, NULL, 0);
    }
}

static inline void mc_initalize(mc_context* const ctxt) {
    int result, tid, pa_tid, sf_tid, tb_tid, tr_tid;

    log(LOG_HEAD "Initalizing");
    Putc(TRAIN, TRAIN_ALL_STOP);
    Putc(TRAIN, TRAIN_ALL_STOP);
    Putc(TRAIN, TRAIN_ALL_START);

    tid = RegisterAs((char*)MISSION_CONTROL_NAME);
    assert(tid == 0, "Mission Control has failed to register (%d)", tid);

    train_ui();
    memset(ctxt, 0, sizeof(*ctxt));

    pa_tid = Create(PATH_ADMIN_PRIORITY, path_admin);
    CHECK_CREATE(pa_tid, LOG_HEAD "path admin failed creation");

    tb_tid = Create(TRAIN_BLASTER_PRIORITY, train_blaster);
    CHECK_CREATE(tb_tid, LOG_HEAD "train blaser failed creation");

    sf_tid = Create(SENSOR_FARM_PRIORITY, sensor_farm); 
    CHECK_CREATE(sf_tid, LOG_HEAD "server farm failed creation");

    tr_tid = Create(TRACK_RESERVATION_PRIORITY, track_reservation);
    CHECK_CREATE(tr_tid, LOG_HEAD "track reservation failed creation");

    mc_reset_track_state(ctxt);
    mc_stall_for_track_load(ctxt);

    const track_node* const track = ctxt->track;
    result = Send(pa_tid, (char*)&track, sizeof(track), NULL, 0);
    if (result != 0) ABORT("Failed setting up PATH ADMIN %d", result);

    result = Send(tb_tid, (char*)&track, sizeof(track), NULL, 0);
    if (result != 0) ABORT("Failed setting up TRAIN BLASTER %d", result);

    result = Send(tr_tid, (char*)&track, sizeof(track), NULL, 0);
    if (result != 0) ABORT("Failed setting up TRACK RESERVE %d", result);
    
    result = Send(sf_tid, NULL, 0, NULL, 0);
    if (result != 0) ABORT("Failed setting up Server Farm %d", result);

    log(LOG_HEAD "Ready!");
}

void mission_control() {
    int        tid;
    mc_req     req;
    mc_context context;
    mission_control_tid = myTid();

    mc_initalize(&context);

    FOREVER {
        int result = Receive(&tid, (char*)&req, sizeof(req));
        assert(req.type < MC_TYPE_COUNT, "Invalid MC Request %d", req.type);

        switch (req.type) {
        case MC_U_TURNOUT:
            mc_update_turnout(&context,
                              req.payload.turn.num,
                              req.payload.turn.state);
            break;

        case MC_R_TRACK:
            mc_reset_track_state(&context);
            break;

        case MC_TD_GET_NEXT_SENSOR:
            mc_get_next_sensor(&context, req.payload.int_value, tid);
            continue;

        case MC_L_TRACK:
        case MC_TYPE_COUNT:
            break;
        }

        result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT(LOG_HEAD "failed replying to %d (%d)", req.type, result);
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
        .payload.turn = {
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
