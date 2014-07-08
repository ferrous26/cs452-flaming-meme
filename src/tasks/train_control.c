
#include <std.h>
#include <debug.h>
#include <vt100.h>
#include <train.h>
#include <syscall.h>

#include <tasks/name_server.h>
#include <tasks/train_driver.h>
#include <tasks/mission_control.h>

#include <tasks/train_control.h>

#define DRIVER_PRIORITY 5

static int train_control_tid;


#define TRAIN_ASSERT(TN)                                        \
assert(TN >= 0 && TN < NUM_TRAINS,                              \
       "[Train Control] %d is an invalid train number", TN)

#define SENSOR_ASSERT(SN)                                       \
assert(SN >= 0 && SN < 80,                                      \
       "[Train Control] %d is an invalid sensor number", SN)

#define NORMALIZE_TRAIN(NAME, TRAIN)                            \
const int NAME = train_to_pos(TRAIN);                           \
if (NAME < 0) {                                                 \
    log("Train %d does not exist!", TRAIN);                     \
    return 0;                                                   \
}                                                               \

typedef struct {
    struct {
        int  horn;
        int  speed;
        int  light;
        int  reverse;
        int  stop;
        int  threshold;
        int  alpha;
        bool where;
        bool dump;
        int  go;

        int stop_offset_valid;
        int stop_offset;
    }   pickup[NUM_TRAINS];
    int drivers[NUM_TRAINS];
} tc_context;

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

static inline void tc_spawn_train_drivers() {
    for (int i=0; i < NUM_TRAINS; i++) {
        int train_num = pos_to_train(i);

        int setup[2] = {train_num, i};
        int tid      = Create(DRIVER_PRIORITY, train_driver);
        assert(tid > 0, "Failed to create train driver (%d)", tid);

        tid = Send(tid, (char*)&setup, sizeof(setup), NULL, 0);
        assert(tid == 0, "Failed to send to train %d (%d)", train_num, tid);
    }
}

static void mc_try_send_train(tc_context* const ctxt, const int train_index) {
    train_req req;

    if (-1 == ctxt->drivers[train_index]) return;
    if (ctxt->pickup[train_index].reverse) {
        req.type                          = TRAIN_REVERSE_DIRECTION;
        ctxt->pickup[train_index].reverse = 0;

    } else if (ctxt->pickup[train_index].speed >= 0) {
        req.type                        = TRAIN_CHANGE_SPEED;
        req.one.int_value               = ctxt->pickup[train_index].speed;
        ctxt->pickup[train_index].speed = -1;

    } else if (-1 != ctxt->pickup[train_index].stop) {
        req.type          = TRAIN_STOP;
        req.one.int_value = ctxt->pickup[train_index].stop;
        ctxt->pickup[train_index].stop = -1;

    } else if (ctxt->pickup[train_index].where) {
        req.type                        = TRAIN_WHERE_ARE_YOU;
        ctxt->pickup[train_index].where = false;

    } else if (ctxt->pickup[train_index].light) {
        req.type                        = TRAIN_TOGGLE_LIGHT;
        ctxt->pickup[train_index].light = 0;

    } else if (ctxt->pickup[train_index].horn) {
        req.type                       = TRAIN_HORN_SOUND;
        ctxt->pickup[train_index].horn = 0;

    } else if (ctxt->pickup[train_index].go != -1) {
        req.type                     = TRAIN_GO_TO;
        req.one.int_value            = ctxt->pickup[train_index].go;
        ctxt->pickup[train_index].go = -1;

    } else if (ctxt->pickup[train_index].dump) {
        req.type                       = TRAIN_DUMP;
        ctxt->pickup[train_index].dump = false;

    } else if (ctxt->pickup[train_index].threshold >= 0) {
        req.type                        = TRAIN_THRESHOLD;
        req.one.int_value               = ctxt->pickup[train_index].threshold;
        ctxt->pickup[train_index].threshold = -1;

    } else if (ctxt->pickup[train_index].alpha >= 0) {
        req.type          = TRAIN_ALPHA;
        req.one.int_value = ctxt->pickup[train_index].alpha;
        ctxt->pickup[train_index].alpha = -1;

    } else if (ctxt->pickup[train_index].stop_offset_valid) {
        req.type          = TRAIN_SET_STOP_OFFSET;
        req.one.int_value = ctxt->pickup[train_index].stop_offset;
        ctxt->pickup[train_index].stop_offset_valid = 0;
    
    } else { return; }

    Reply(ctxt->drivers[train_index], (char*)&req, sizeof(req));
    int result = Reply(ctxt->drivers[train_index],
                       (char*)&req,
                       sizeof(req) - sizeof(int));
    if (!result) ABORT("Failed to send to train %d (%d)", train_index, result);
    ctxt->drivers[train_index] = -1;
}

static inline void mc_update_train_speed(tc_context* const ctxt,
                                         const int index,
                                         const int speed) {
    TRAIN_ASSERT(index);
    ctxt->pickup[index].speed = speed;
    mc_try_send_train(ctxt, index);
}

static inline void mc_toggle_light(tc_context* const ctxt, const int index) {
    TRAIN_ASSERT(index);

    ctxt->pickup[index].light ^= 1;
    mc_try_send_train(ctxt, index);
}

static inline void mc_toggle_horn(tc_context* const ctxt, const int index) {
    TRAIN_ASSERT(index);

    ctxt->pickup[index].horn ^= 1;
    mc_try_send_train(ctxt, index);
}

static inline void mc_reverse_train(tc_context* const ctxt, const int index) {
    TRAIN_ASSERT(index);

    ctxt->pickup[index].reverse ^= 1;
    mc_try_send_train(ctxt, index);
}

static inline void mc_train_stop(tc_context* const ctxt,
                                 const int train_num,
                                 const int sensor_num) {
    TRAIN_ASSERT(train_num);
    SENSOR_ASSERT(sensor_num);

    ctxt->pickup[train_num].stop = sensor_num;
    mc_try_send_train(ctxt, train_num);
}

static inline void mc_train_where_are_you(tc_context* const ctxt,
                                          const int train_num) {
    TRAIN_ASSERT(train_num);

    ctxt->pickup[train_num].where = true;
    mc_try_send_train(ctxt, train_num);
}

static inline void mc_goto(tc_context* const ctxt,
                           const int train_num,
                           const int go) {
    TRAIN_ASSERT(train_num);

    ctxt->pickup[train_num].go = go;
    mc_try_send_train(ctxt, train_num);
}

static inline void mc_train_dump_velocity(tc_context* const ctxt,
                                          const int train_num) {
    TRAIN_ASSERT(train_num);

    ctxt->pickup[train_num].dump = true;
    mc_try_send_train(ctxt, train_num);
}

static inline void mc_threshold(tc_context* const ctxt,
                                const int train_num,
                                const int tweak) {
    TRAIN_ASSERT(train_num);

    ctxt->pickup[train_num].threshold = tweak;
    mc_try_send_train(ctxt, train_num);
}

static inline void mc_alpha(tc_context* const ctxt,
                            const int train_num,
                            const int tweak) {
    TRAIN_ASSERT(train_num);
    ctxt->pickup[train_num].alpha = tweak;
    mc_try_send_train(ctxt, train_num);
}

static inline void mc_set_stop_off(tc_context* const ctxt,
                                   const int train_num,
                                   const int tweak) {
    TRAIN_ASSERT(train_num);

    ctxt->pickup[train_num].stop_offset       = tweak;
    ctxt->pickup[train_num].stop_offset_valid = 1;
    mc_try_send_train(ctxt, train_num);
}

static inline void tc_init_context(tc_context* const ctxt) {
    memset(ctxt,           0, sizeof(*ctxt));
    memset(ctxt->drivers, -1, sizeof(ctxt->drivers));

    for(int i = 0; i < NUM_TRAINS; i++) {
        ctxt->pickup[i].stop      = -1;
        ctxt->pickup[i].threshold = -1;
        ctxt->pickup[i].alpha     = -1;
        ctxt->pickup[i].go        = -1;
    }
}

void train_control() {
    train_control_tid = myTid();
    RegisterAs((char*)TRAIN_CONTROL_NAME);

    int tid, result;
    tc_req     req;
    tc_context context;

    tc_init_context(&context);
    tc_spawn_train_drivers();

    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));
        assert(result == sizeof(req),
               "[TRAIN CONTROL] received invalid request %d", result);

        switch (req.type) {
        case TC_U_TRAIN_SPEED:
            mc_update_train_speed(&context,
                                  req.payload.train_speed.num,
                                  req.payload.train_speed.speed);
            break;
        case TC_U_TRAIN_REVERSE:
            mc_reverse_train(&context, req.payload.int_value);
            break;
        case TC_U_GOTO:
            mc_goto(&context,
                    req.payload.train_go.train,
                    req.payload.int_value);
            break;
        case TC_U_THRESHOLD:
            mc_threshold(&context,
                         req.payload.train_tweak.train,
                         req.payload.train_tweak.tweak);
            break;
        case TC_U_ALPHA:
            mc_alpha(&context,
                     req.payload.train_tweak.train,
                     req.payload.train_tweak.tweak);
            break;
        case TC_U_STOP_OFFSET:
            mc_set_stop_off(&context,
                            req.payload.train_tweak.train,
                            req.payload.train_tweak.tweak);
            break;
        case TC_T_TRAIN_LIGHT:
            mc_toggle_light(&context, req.payload.int_value);
            break;
        case TC_T_TRAIN_HORN:
            mc_toggle_horn(&context, req.payload.int_value);
            break;
        case TC_T_TRAIN_STOP:
            mc_train_stop(&context,
                          req.payload.train_stop.train,
                          req.payload.train_stop.sensor);
            break;

        case TC_Q_TRAIN_WHEREIS:
            mc_train_where_are_you(&context, req.payload.int_value);
            break;

        case TC_Q_DUMP:
            mc_train_dump_velocity(&context, req.payload.int_value);
            break;

        case TC_REQ_WORK:
            context.drivers[req.payload.int_value] = tid;
            mc_try_send_train(&context, req.payload.int_value);
            continue;

        default:
        case TC_TYPE_COUNT:
            ABORT("[TRAIN CONTROL] INVALID REQUEST TYPE %d", req.type);
        }

        result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("[TRAIN CONTROL] failed to reply to  %d (%d)",
                  req.type, result);
    }
}

int train_set_speed(const int train, int speed) {
    NORMALIZE_TRAIN(train_index, train)

    if (speed < 0 || speed >= TRAIN_REVERSE) {
        log("Can't set train number %d to invalid speed %d", train, speed);
        return 0;
    }

    /*
    if (speed < TRAIN_PRACTICAL_MIN_SPEED && speed != 0) {
        log("Speed %d is too slow to be be practical", speed);
        speed = TRAIN_PRACTICAL_MIN_SPEED;
    }

    if (speed > TRAIN_PRACTICAL_MAX_SPEED) {
        log("Speed %d is too fast to be reliable", speed);
        speed = TRAIN_PRACTICAL_MAX_SPEED;
    }
    */

    tc_req req = {
        .type = TC_U_TRAIN_SPEED,
        .payload.train_speed = {
            .num   = (short)train_index,
            .speed = (short)speed
        }
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_reverse(const int train) {
    NORMALIZE_TRAIN(train_index, train)

    tc_req req = {
        .type              = TC_U_TRAIN_REVERSE,
        .payload.int_value = train_index
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_toggle_light(const int train) {
    NORMALIZE_TRAIN(train_index, train)

    tc_req req = {
        .type              = TC_T_TRAIN_LIGHT,
        .payload.int_value = train_index
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_toggle_horn(const int train) {
    NORMALIZE_TRAIN(train_index, train)

    tc_req req = {
        .type              = TC_T_TRAIN_HORN,
        .payload.int_value = train_index
    };
    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_stop_at(const int train, const int bank, const int num) {
    NORMALIZE_TRAIN(train_index, train)

    tc_req req = {
        .type               = TC_T_TRAIN_STOP,
        .payload.train_stop = {
            .train  = train_index,
            .sensor = (bank-'A') * 16 + (num-1)
        }
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_where_are_you(const int train) {
    NORMALIZE_TRAIN(train_index, train)

    tc_req req = {
        .type              = TC_Q_TRAIN_WHEREIS,
        .payload.int_value = train_index
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_goto(const int train,
               const int bank,
               const int num,
               const int off) {
    NORMALIZE_TRAIN(train_index, train)

    int norm_bank; 
    if (bank >= 'a' && bank <= 'e') {
        norm_bank = bank - 32;
    } else if (bank < 'A' || bank > 'E') {
        log("can't goto on invalid bank %c", bank);
        return 0;
    } else {
        norm_bank = bank;
    }

    tc_req req = {
        .type = TC_U_GOTO,
        .payload.train_go = {
            .train  = (int8)train_index,
            .bank   = (int8)bank,
            .num    = (int8)num,
            .offset = (int8)off
        }
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_dump(const int train) {
    NORMALIZE_TRAIN(train_index, train)
    
    tc_req req = {
        .type = TC_Q_DUMP,
        .payload.int_value = train_index
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_update_threshold(const int train, const int threshold) {
    NORMALIZE_TRAIN(train_index, train)

    tc_req req = {
        .type = TC_U_THRESHOLD,
        .payload.train_tweak = {
            .train = (short)train_index,
            .tweak = (short)threshold
        }
    };

    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_update_alpha(const int train, const int alpha) {
    NORMALIZE_TRAIN(train_index, train)

    if (alpha > NINTY_TEN) {
        log("Invalid alpha value %d", alpha);
        return 0;
    }

    tc_req req = {
        .type = TC_U_ALPHA,
        .payload.train_tweak = {
            .train = (short)train_index,
            .tweak = (short)alpha
        }
    };
    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_set_stop_offset(const int train, const int offset) {
    NORMALIZE_TRAIN(train_index, train)

    tc_req req = {
        .type = TC_U_STOP_OFFSET,
        .payload.train_tweak = {
            .train = (short)train_index,
            .tweak = (short)offset
        }
    };
    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

