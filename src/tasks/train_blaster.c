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


static int train_blaster_tid;

#define NORMALIZE_TRAIN(NAME, TRAIN)                            \
    const int NAME = train_to_pos(TRAIN);                       \
    if (NAME == INVALID_TRAIN) {                                \
        log("[Blaster] Train %d does not exist!", TRAIN);       \
        return 0;                                               \
    }

#define NORMALIZE_SENSOR(NAME, BANK, NUM)                                 \
    const int NAME = sensor_to_pos( bank >= 'a' ? bank - 32 : bank, num); \
    if (NAME == -1) {                                                     \
        log("[Blaster] Sensor %c%d does not exist!", BANK, NUM);          \
        return INVALID_SENSOR;                                            \
    }

typedef struct {
    struct {
        int  courier;            // tid of train_master courier
        int  speed;              // next speed to send to the train

        bool reverse;            // should send a reverse command
        bool whereis;            // should send send whereis command

        int  stop_sensor;        // sensor to stop after hitting

        struct {
            int index;
            int offset;
        } location;

        bool dump_velocity;      // should send a dump command
        int  feedback_threshold;
        int  feedback_alpha;
        int  stop_offset;
        int  clearance_offset;
        int  fudge_factor;
        int  short_move;

        int  horn;               // horn state
    } master[NUM_TRAINS];

    const track_node* const track;
} blaster_context;

static inline void blaster_init_context(blaster_context* const ctxt) {
    memset(ctxt, 0, sizeof(blaster_context));

    for(int i = 0; i < NUM_TRAINS; i++) {
        ctxt->master[i].courier            = -1;
        ctxt->master[i].speed              = -1;
        ctxt->master[i].stop_sensor        = -1;
        ctxt->master[i].location.index     = -1;
        ctxt->master[i].feedback_alpha     = -1;
        ctxt->master[i].stop_offset        = INT_MAX;
        ctxt->master[i].clearance_offset   = INT_MAX;
        ctxt->master[i].fudge_factor       = INT_MAX;
    }

    int tid;
    int result = Receive(&tid, (char*)&ctxt->track, sizeof(ctxt->track));
    assert(myParentTid() == tid,
           "BLASTER received init message from invalid process(%d)", tid);
    assert(sizeof(ctxt->track) == result,
           "BLASTER received invalid message (%d)", result);

    result = Reply(tid, NULL, 0);
    assert(0 == result, "Failed releasing parent (%d)", result);
}

static inline void blaster_spawn_masters(const track_node* const track) {

    for (int i = 0; i < NUM_TRAINS; i++) {
        const int init_data[2] = { i, (int)track };
        const int train_num    = pos_to_train(i);
        const int       tid    = Create(TRAIN_MASTER_PRIORITY, train_master);
        assert(tid > 0, "[Blaster] Failed to create train master (%d)", tid);

        const int result = Send(tid,
                                (char*)init_data, sizeof(init_data),
                                NULL, 0);

        assert(result == 0, "[Blaster] Failed to send to master %d (%d)",
               train_num, result);
        UNUSED(result);
        UNUSED(train_num);
    }
}

static void
blaster_try_send_master(blaster_context* const ctxt, const int index) {

    master_req req;

    if (ctxt->master[index].courier == -1) return; // no way to send work

    if (ctxt->master[index].speed >= 0) {
        req.type                  = MASTER_CHANGE_SPEED;
        req.arg1                  = ctxt->master[index].speed;
        ctxt->master[index].speed = -1;

    } else if (ctxt->master[index].reverse) {
        req.type                    = MASTER_REVERSE;
        ctxt->master[index].reverse = false;

    } else if (ctxt->master[index].whereis) {
        req.type                    = MASTER_WHERE_ARE_YOU;
        ctxt->master[index].whereis = false;

    } else if (ctxt->master[index].stop_sensor != -1) {
        req.type                        = MASTER_STOP_AT_SENSOR;
        req.arg1                        = ctxt->master[index].stop_sensor;
        ctxt->master[index].stop_sensor = -1;

    } else if (ctxt->master[index].location.index != -1) {
        req.type = MASTER_GOTO_LOCATION;
        req.arg1 = ctxt->master[index].location.index;
        req.arg2 = ctxt->master[index].location.offset;
        ctxt->master[index].location.index = -1;

    } else if (ctxt->master[index].short_move) {
        req.type = MASTER_SHORT_MOVE;
        req.arg1 = ctxt->master[index].short_move;
        ctxt->master[index].short_move = 0;

    } else if (ctxt->master[index].dump_velocity) {
        req.type                          = MASTER_DUMP_VELOCITY_TABLE;
        ctxt->master[index].dump_velocity = false;

    } else if (ctxt->master[index].feedback_threshold) {
        req.type = MASTER_UPDATE_FEEDBACK_THRESHOLD;
        req.arg1 = ctxt->master[index].feedback_threshold;
        ctxt->master[index].feedback_threshold = 0;

    } else if (ctxt->master[index].feedback_alpha != -1) {
        req.type = MASTER_UPDATE_FEEDBACK_ALPHA;
        req.arg1 = ctxt->master[index].feedback_alpha;
        ctxt->master[index].feedback_alpha = -1;

    } else if (ctxt->master[index].stop_offset != INT_MAX) {
        req.type = MASTER_UPDATE_STOP_OFFSET;
        req.arg1 = ctxt->master[index].stop_offset;
        ctxt->master[index].stop_offset = INT_MAX;

    } else if (ctxt->master[index].clearance_offset != INT_MAX) {
        req.type = MASTER_UPDATE_CLEARANCE_OFFSET;
        req.arg1 = ctxt->master[index].clearance_offset;
        ctxt->master[index].clearance_offset = INT_MAX;

    } else if (ctxt->master[index].fudge_factor != INT_MAX) {
        req.type = MASTER_UPDATE_FUDGE_FACTOR;
        req.arg1 = ctxt->master[index].fudge_factor;
        ctxt->master[index].fudge_factor = INT_MAX;

    } else { return; }


    const int result = Reply(ctxt->master[index].courier,
                             (char*)&req,
                             sizeof(req));
    if (result) ABORT("Failed to send to train %d (%d)", index, result);
    ctxt->master[index].courier = -1;
}

static void
blaster_toggle_horn(blaster_context* const ctxt,
                    const int tid,
                    const int index) {

    ctxt->master[index].horn ^= -1;
    put_train_cmd((char)pos_to_train(index),
                  ctxt->master[index].horn ? TRAIN_HORN : TRAIN_FUNCTION_OFF);

    const int result = Reply(tid, NULL, 0);
    if (result < 0)
        ABORT("[BLASTER] Failed to reply to horn toggler %d (%d)",
              index, result);
}

void train_blaster() {
    train_blaster_tid = myTid();

    int result = RegisterAs((char*)TRAIN_BLASTER_NAME);
    if (result)
        ABORT("[Blaster] Failed to register with name server (%d)");

    int             tid;
    blaster_req     req;
    blaster_context context;

    blaster_init_context(&context);
    blaster_spawn_masters(context.track);

    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));

        assert(result >= (int)sizeof(int),
               "[BLASTER] Received invalid request size %d", result);

        assert(req.type <= BLASTER_TOGGLE_HORN,
               "[Blaster] Received invalid request %d from %d", req.type, tid);

        const int index = req.arg1; // index of the train

        switch (req.type) {
        case BLASTER_SET_SPEED:
            context.master[index].speed = req.arg2;
            break;
        case BLASTER_REVERSE:
            context.master[index].reverse = true;
            break;
        case BLASTER_WHERE_ARE_YOU:
            context.master[index].whereis = true;
            break;
        case BLASTER_STOP_AT_SENSOR:
            context.master[index].stop_sensor = req.arg2;
            break;
        case BLASTER_GOTO_LOCATION:
            context.master[index].location.index  = req.arg2;
            context.master[index].location.offset = req.arg3;
            break;
        case BLASTER_SHORT_MOVE:
            context.master[index].short_move = req.arg2;
            break;
        case BLASTER_DUMP_VELOCITY_TABLE:
            context.master[index].dump_velocity = true;
            break;
        case BLASTER_UPDATE_FEEDBACK_THRESHOLD:
            context.master[index].feedback_threshold = req.arg2;
            break;
        case BLASTER_UPDATE_FEEDBACK_ALPHA:
            context.master[index].feedback_alpha = req.arg2;
            break;
        case BLASTER_UPDATE_STOP_OFFSET:
            context.master[index].stop_offset = req.arg2;
            break;
        case BLASTER_UPDATE_CLEARANCE_OFFSET:
            context.master[index].clearance_offset = req.arg2;
            break;
        case BLASTER_UPDATE_FUDGE_FACTOR:
            context.master[index].fudge_factor = req.arg2;
            break;

        case BLASTER_TOGGLE_HORN:
            blaster_toggle_horn(&context, tid, index);
            continue;
        case MASTER_BLASTER_REQUEST_COMMAND:
            context.master[index].courier = tid;
            blaster_try_send_master(&context, index);
            continue;
        }

        result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("[BLASTER] Failed to reply to %d (%d)", req.type, result);

        blaster_try_send_master(&context, index);
    }
}



/* Client Code */

int train_set_speed(const int train, const int speed) {
    NORMALIZE_TRAIN(train_index, train);

    if (speed < 0 || speed >= TRAIN_REVERSE) {
        log("[Blaster] Invalid speed (%d) for train %d", speed, train);
        return INVALID_SPEED;
    }

    blaster_req req = {
        .type = BLASTER_SET_SPEED,
        .arg1 = train_index,
        .arg2 = speed * 10
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_reverse(const int train) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_REVERSE,
        .arg1 = train_index
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - (sizeof(int) * 2),
                NULL, 0);
}

int train_where_are_you(const int train) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_WHERE_ARE_YOU,
        .arg1 = train_index
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - (sizeof(int) * 2),
                NULL, 0);
}

int train_stop_at_sensor(const int train, const int bank, const int num) {
    NORMALIZE_TRAIN(train_index, train);
    NORMALIZE_SENSOR(sensor_index, bank, num);

    blaster_req req = {
        .type = BLASTER_STOP_AT_SENSOR,
        .arg1 = train_index,
        .arg2 = sensor_index
    };

    return Send(train_blaster_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_goto_location(const int train,
                        const int bank,
                        const int num,
                        const int off) {
    NORMALIZE_TRAIN(train_index, train);
    NORMALIZE_SENSOR(sensor_index, bank, num);

    blaster_req req = {
        .type = BLASTER_GOTO_LOCATION,
        .arg1 = train_index,
        .arg2 = sensor_index,
        .arg3 = off
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req),
                NULL, 0);
}

int train_short_move(const int train, const int off) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_SHORT_MOVE,
        .arg1 = train_index,
        .arg2 = off
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req),
                NULL, 0);
}

int train_dump_velocity_table(const int train) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_DUMP_VELOCITY_TABLE,
        .arg1 = train_index
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - (sizeof(int) * 2),
                NULL, 0);
}

int train_update_feedback_threshold(const int train, const int threshold) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_UPDATE_FEEDBACK_THRESHOLD,
        .arg1 = train_index,
        .arg2 = threshold // convert to micrometers
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_update_feedback_alpha(const int train, const int alpha) {
    NORMALIZE_TRAIN(train_index, train);

    if (alpha > NINTY_TEN) {
        log("[Blaster] Invalid alpha value %d", alpha);
        return INVALID_FEEDBACK_ALPHA;
    }

    blaster_req req = {
        .type = BLASTER_UPDATE_FEEDBACK_ALPHA,
        .arg1 = train_index,
        .arg2 = alpha
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_update_stop_offset(const int train, const int offset) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_UPDATE_STOP_OFFSET,
        .arg1 = train_index,
        .arg2 = offset * 1000
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_update_clearance_offset(const int train, const int offset) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_UPDATE_CLEARANCE_OFFSET,
        .arg1 = train_index,
        .arg2 = offset * 1000
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_update_reverse_time_fudge(const int train, const int fudge) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_UPDATE_FUDGE_FACTOR,
        .arg1 = train_index,
        .arg2 = fudge
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - sizeof(int),
                NULL, 0);
}

int train_toggle_horn(const int train) {
    NORMALIZE_TRAIN(train_index, train);

    blaster_req req = {
        .type = BLASTER_TOGGLE_HORN,
        .arg1 = train_index
    };

    return Send(train_blaster_tid,
                (char*)&req, sizeof(req) - (sizeof(int) * 2),
                NULL, 0);
}
