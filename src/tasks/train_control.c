#include <std.h>
#include <debug.h>
#include <vt100.h>
#include <train.h>
#include <syscall.h>

#include <tasks/priority.h>
#include <tasks/name_server.h>
#include <tasks/train_server.h>
#include <tasks/mission_control.h>
#include <tasks/train_master.h>
#include <tasks/train_blaster.h>
#include <tasks/train_control.h>

static int train_control_tid;

#define NORMALIZE_TRAIN(NAME, TRAIN)                            \
    const int NAME = train_to_pos(TRAIN);                       \
    if (NAME == INVALID_TRAIN) {                                \
        log("[Control] Train %d does not exist!", TRAIN);       \
        return 0;                                               \
    }

#define NORMALIZE_SENSOR(NAME, BANK, NUM)                                 \
    const int NAME = sensor_to_pos( bank >= 'a' ? bank - 32 : bank, num); \
    if (NAME == -1) {                                                     \
        log("[Control] Sensor %c%d does not exist!", BANK, NUM);          \
        return INVALID_SENSOR;                                            \
    }

typedef struct {
    struct {
        int  courier;            // tid of train_blaster courier
        int  speed;              // next speed to send to the train

        bool reverse;            // should send a reverse command
        int  whereis;            // should send send whereis command

        bool dump_velocity;      // should send a dump command
        int  feedback_threshold;
        int  feedback_alpha;
        int  stop_offset;
        int  clearance_offset;
        int  fudge_factor;
        int  short_move;

        int  horn;               // horn state
    } blaster[NUM_TRAINS];

    struct {
        int             courier;
        track_location location;
        int            stop_sensor; // sensor to stop after hitting
    } master[NUM_TRAINS];

    const track_node* const track;
} control_context;

static inline void control_init_context(control_context* const ctxt) {
    memset(ctxt, 0, sizeof(control_context));

    for(int i = 0; i < NUM_TRAINS; i++) {
        ctxt->blaster[i].courier            = -1;
        ctxt->blaster[i].speed              = -1;
        ctxt->blaster[i].feedback_alpha     = -1;
        ctxt->blaster[i].whereis            = -1;
        ctxt->blaster[i].stop_offset        = INT_MAX;
        ctxt->blaster[i].clearance_offset   = INT_MAX;
        ctxt->blaster[i].fudge_factor       = INT_MAX;

        ctxt->master[i].courier             = -1;
        ctxt->master[i].location.sensor     = -1;
        ctxt->master[i].stop_sensor         = -1;
    }

    int tid;
    int result = Receive(&tid, (char*)&ctxt->track, sizeof(ctxt->track));
    assert(myParentTid() == tid,
           "CONTROL received init message from invalid process(%d)", tid);
    assert(sizeof(ctxt->track) == result,
           "CONTROL received invalid message (%d)", result);

    result = Reply(tid, NULL, 0);
    assert(0 == result, "Failed releasing parent (%d)", result);
}

static inline void control_spawn_thunderdome(const track_node* const track) {

    for (int i = 0; i < NUM_TRAINS; i++) {
        const int train_num = pos_to_train(i);
        UNUSED(train_num);

        const int blaster_data[2] = { i, (int)track };
        const int      blaster    = Create(TRAIN_BLASTER_PRIORITY, train_blaster);
        assert(blaster > 0,
               "[Control] Failed to create train blaster (%d)",
               blaster);

        int result = Send(blaster,
                          (char*)blaster_data, sizeof(blaster_data),
                          NULL, 0);

        assert(result == 0, "[Control] Failed to send to blaster %d (%d)",
               train_num, result);

        const int master_data[2] = { i, blaster };
        const int master = Create(TRAIN_MASTER_PRIORITY, train_master);
        assert(master > 0,
               "[Control] Failed to create train master (%d)",
               master);

        result = Send(master,
                      (char*)master_data, sizeof(master_data),
                      NULL, 0);
        assert(result == 0,
               "[Control] Failed to send to aunty %d (%d)",
               train_num, result);
    }
}

static inline void
control_try_send_blaster(control_context* const ctxt, const int index) {

    blaster_req req;

    if (ctxt->blaster[index].courier == -1) return; // no way to send work

    if (ctxt->blaster[index].speed >= 0) {
        req.type                  = BLASTER_CHANGE_SPEED;
        req.arg1                  = ctxt->blaster[index].speed;
        ctxt->blaster[index].speed = -1;

    } else if (ctxt->blaster[index].reverse) {
        req.type                    = BLASTER_REVERSE;
        ctxt->blaster[index].reverse = false;

    } else if (ctxt->blaster[index].whereis != -1) {
        req.type                    = BLASTER_WHERE_ARE_YOU;
        req.arg1                    = ctxt->blaster[index].whereis;
        ctxt->blaster[index].whereis = -1;

    } else if (ctxt->blaster[index].short_move) {
        req.type = BLASTER_SHORT_MOVE;
        req.arg1 = ctxt->blaster[index].short_move;
        ctxt->blaster[index].short_move = 0;

    } else if (ctxt->blaster[index].dump_velocity) {
        req.type                           = BLASTER_DUMP_VELOCITY_TABLE;
        ctxt->blaster[index].dump_velocity = false;

    } else if (ctxt->blaster[index].feedback_threshold) {
        req.type = BLASTER_UPDATE_FEEDBACK_THRESHOLD;
        req.arg1 = ctxt->blaster[index].feedback_threshold;
        ctxt->blaster[index].feedback_threshold = 0;

    } else if (ctxt->blaster[index].feedback_alpha != -1) {
        req.type = BLASTER_UPDATE_FEEDBACK_ALPHA;
        req.arg1 = ctxt->blaster[index].feedback_alpha;
        ctxt->blaster[index].feedback_alpha = -1;

    } else if (ctxt->blaster[index].stop_offset != INT_MAX) {
        req.type = BLASTER_UPDATE_STOP_OFFSET;
        req.arg1 = ctxt->blaster[index].stop_offset;
        ctxt->blaster[index].stop_offset = INT_MAX;

    } else if (ctxt->blaster[index].clearance_offset != INT_MAX) {
        req.type = BLASTER_UPDATE_CLEARANCE_OFFSET;
        req.arg1 = ctxt->blaster[index].clearance_offset;
        ctxt->blaster[index].clearance_offset = INT_MAX;

    } else if (ctxt->blaster[index].fudge_factor != INT_MAX) {
        req.type = BLASTER_UPDATE_FUDGE_FACTOR;
        req.arg1 = ctxt->blaster[index].fudge_factor;
        ctxt->blaster[index].fudge_factor = INT_MAX;

    } else { return; }

    if (ctxt->master[index].courier == -1) return; // no way to send work

    const int result = Reply(ctxt->blaster[index].courier,
                             (char*)&req,
                             sizeof(req));
    if (result) ABORT("Failed to send to train %d (%d)", index, result);
    ctxt->blaster[index].courier = -1;
}

static inline void
control_try_send_master(control_context* const ctxt, const int index) {

    master_req req;

    if (ctxt->master[index].location.sensor != -1) {
        req.type = MASTER_GOTO_LOCATION;
        req.arg1 = ctxt->master[index].location.sensor;
        req.arg2 = ctxt->master[index].location.offset;
        ctxt->master[index].location.sensor = -1;

    } else if (ctxt->master[index].stop_sensor != -1) {
        req.type = MASTER_STOP_AT_SENSOR;
        req.arg1 = ctxt->master[index].stop_sensor;
        ctxt->master[index].stop_sensor = -1;

    }
    else { return; }

    const int result = Reply(ctxt->master[index].courier,
                             (char*)&req,
                             sizeof(req));
    if (result) ABORT("Failed to send to train %d (%d)", index, result);
    ctxt->master[index].courier = -1;
}

static void
control_toggle_horn(control_context* const ctxt,
                    const int tid,
                    const int index) {

    ctxt->blaster[index].horn ^= -1;
    put_train_cmd((char)pos_to_train(index),
                  ctxt->blaster[index].horn ? TRAIN_HORN : TRAIN_FUNCTION_OFF);

    const int result = Reply(tid, NULL, 0);
    if (result < 0)
        ABORT("[CONTROL] Failed to reply to horn toggler %d (%d)",
              index, result);
}

void train_control() {
    train_control_tid = myTid();

    int result = RegisterAs((char*)TRAIN_CONTROL_NAME);
    if (result)
        ABORT("[Control] Failed to register with name server (%d)");

    int             tid;
    control_req     req;
    control_context context;

    control_init_context(&context);
    control_spawn_thunderdome(context.track);

    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));

        assert(result >= (int)sizeof(int),
               "[CONTROL] Received invalid request size %d", result);

        assert(req.type <= CONTROL_TOGGLE_HORN,
               "[Control] Received invalid request %d from %d", req.type, tid);

        const int index = req.arg1; // index of the train

        switch (req.type) {
        case CONTROL_SET_SPEED:
            context.blaster[index].speed = req.arg2;
            break;
        case CONTROL_REVERSE:
            context.blaster[index].reverse = true;
            break;
        case CONTROL_WHERE_ARE_YOU:
            context.blaster[index].whereis = tid;
            control_try_send_blaster(&context, index);
            continue;
        case CONTROL_STOP_AT_SENSOR:
            context.master[index].stop_sensor = req.arg2;
            break;
        case CONTROL_SHORT_MOVE:
            context.blaster[index].short_move = req.arg2;
            break;
        case CONTROL_DUMP_VELOCITY_TABLE:
            context.blaster[index].dump_velocity = true;
            break;
        case CONTROL_UPDATE_FEEDBACK_THRESHOLD:
            context.blaster[index].feedback_threshold = req.arg2;
            break;
        case CONTROL_UPDATE_FEEDBACK_ALPHA:
            context.blaster[index].feedback_alpha = req.arg2;
            break;
        case CONTROL_UPDATE_STOP_OFFSET:
            context.blaster[index].stop_offset = req.arg2;
            break;
        case CONTROL_UPDATE_CLEARANCE_OFFSET:
            context.blaster[index].clearance_offset = req.arg2;
            break;
        case CONTROL_UPDATE_FUDGE_FACTOR:
            context.blaster[index].fudge_factor = req.arg2;
            break;
        case CONTROL_GOTO_LOCATION:
            context.master[index].location.sensor = req.arg2;
            context.master[index].location.offset = req.arg3;
            break;

        case CONTROL_TOGGLE_HORN:
            control_toggle_horn(&context, tid, index);
            continue;
        case BLASTER_CONTROL_REQUEST_COMMAND:
            context.blaster[index].courier = tid;
            control_try_send_blaster(&context, index);
            continue;

        case MASTER_CONTROL_REQUEST_COMMAND:
            context.master[index].courier = tid;
            control_try_send_master(&context, index);
            continue;
        }

        result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("[CONTROL] Failed to reply to %d (%d)", req.type, result);

        control_try_send_master(&context, index);
        control_try_send_blaster(&context, index);
    }
}



/* Client Code */

int train_set_speed(const int train, const int speed) {
    NORMALIZE_TRAIN(train_index, train);

    if (speed < 0 || speed >= TRAIN_REVERSE) {
        log("[Control] Invalid speed (%d) for train %d", speed, train);
        return INVALID_SPEED;
    }

    control_req req = {
        .type = CONTROL_SET_SPEED,
        .arg1 = train_index,
        .arg2 = speed * 10
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_reverse(const int train) {
    NORMALIZE_TRAIN(train_index, train);

    control_req req = {
        .type = CONTROL_REVERSE,
        .arg1 = train_index
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - (sizeof(int) * 2),
                NULL, 0);
}

track_location train_where_are_you(const int train) {

    track_location location = {
        .sensor = INVALID_TRAIN
    };

    const int train_index = train_to_pos(train);
    if (train_index == INVALID_TRAIN) {
        log("[Control] Train %d does not exist!", train);
        return location;
    }

    control_req req = {
        .type = CONTROL_WHERE_ARE_YOU,
        .arg1 = train_index
    };

    int result = Send(train_control_tid,
                      (char*)&req, sizeof(req) - (sizeof(int) * 2),
                      (char*)&location, sizeof(location));

    if (result < 0)
        location.sensor = result;

    return location;
}

int train_stop_at_sensor(const int train, const int bank, const int num) {
    NORMALIZE_TRAIN(train_index, train);
    NORMALIZE_SENSOR(sensor_index, bank, num);

    control_req req = {
        .type = CONTROL_STOP_AT_SENSOR,
        .arg1 = train_index,
        .arg2 = sensor_index
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_goto_location(const int train,
                        const int bank,
                        const int num,
                        const int off) {
    NORMALIZE_TRAIN(train_index, train);
    NORMALIZE_SENSOR(sensor_index, bank, num);

    control_req req = {
        .type = CONTROL_GOTO_LOCATION,
        .arg1 = train_index,
        .arg2 = sensor_index,
        .arg3 = off
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req),
                NULL, 0);
}

int train_short_move(const int train, const int off) {
    NORMALIZE_TRAIN(train_index, train);

    control_req req = {
        .type = CONTROL_SHORT_MOVE,
        .arg1 = train_index,
        .arg2 = off
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req),
                NULL, 0);
}

int train_dump_velocity_table(const int train) {
    NORMALIZE_TRAIN(train_index, train);

    control_req req = {
        .type = CONTROL_DUMP_VELOCITY_TABLE,
        .arg1 = train_index
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - (sizeof(int) * 2),
                NULL, 0);
}

int train_update_feedback_threshold(const int train, const int threshold) {
    NORMALIZE_TRAIN(train_index, train);

    control_req req = {
        .type = CONTROL_UPDATE_FEEDBACK_THRESHOLD,
        .arg1 = train_index,
        .arg2 = threshold // convert to micrometers
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_update_feedback_alpha(const int train, const int alpha) {
    NORMALIZE_TRAIN(train_index, train);

    if (alpha > NINTY_TEN) {
        log("[Control] Invalid alpha value %d", alpha);
        return INVALID_FEEDBACK_ALPHA;
    }

    control_req req = {
        .type = CONTROL_UPDATE_FEEDBACK_ALPHA,
        .arg1 = train_index,
        .arg2 = alpha
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_update_stop_offset(const int train, const int offset) {
    NORMALIZE_TRAIN(train_index, train);

    control_req req = {
        .type = CONTROL_UPDATE_STOP_OFFSET,
        .arg1 = train_index,
        .arg2 = offset * 1000
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_update_clearance_offset(const int train, const int offset) {
    NORMALIZE_TRAIN(train_index, train);

    control_req req = {
        .type = CONTROL_UPDATE_CLEARANCE_OFFSET,
        .arg1 = train_index,
        .arg2 = offset * 1000
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_update_reverse_time_fudge(const int train, const int fudge) {
    NORMALIZE_TRAIN(train_index, train);

    control_req req = {
        .type = CONTROL_UPDATE_FUDGE_FACTOR,
        .arg1 = train_index,
        .arg2 = fudge
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_toggle_horn(const int train) {
    NORMALIZE_TRAIN(train_index, train);

    control_req req = {
        .type = CONTROL_TOGGLE_HORN,
        .arg1 = train_index
    };

    return Send(train_control_tid,
                (char*)&req, sizeof(req) - (sizeof(int) * 2),
                NULL, 0);
}
