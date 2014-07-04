
#include <std.h>
#include <debug.h>
#include <vt100.h>
#include <train.h>
#include <syscall.h>

#include <tasks/name_server.h>
#include <tasks/train_station.h>
#include <tasks/mission_control.h>

#include <tasks/train_control.h>

#define DRIVER_PRIORITY 5

static int train_control_tid;

typedef struct {
    struct {
        int horn;
        int speed;
        int light;
        int reverse;
        sensor_name stop;
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
        
        log("Spawning Train %d", train_num);
        
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
        
    } else if (ctxt->pickup[train_index].stop.bank) {
        req.type       = TRAIN_STOP;
        req.one.sensor = ctxt->pickup[train_index].stop;
        ctxt->pickup[train_index].stop.bank = 0;
        
    } else if (ctxt->pickup[train_index].light) {
        req.type                        = TRAIN_TOGGLE_LIGHT;
        ctxt->pickup[train_index].light = 0;
        
    } else if (ctxt->pickup[train_index].horn) {
        req.type                       = TRAIN_HORN_SOUND;
        ctxt->pickup[train_index].horn = 0;
        
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
    ctxt->pickup[index].speed = speed;
    mc_try_send_train(ctxt, index);
}

static inline void mc_toggle_light(tc_context* const ctxt, const int index) {
    ctxt->pickup[index].light ^= 1;
    mc_try_send_train(ctxt, index);
}

static inline void mc_toggle_horn(tc_context* const ctxt, const int index) {
    ctxt->pickup[index].horn ^= 1;
    mc_try_send_train(ctxt, index);
}

static inline void mc_reverse_train(tc_context* const ctxt, const int index) {
    ctxt->pickup[index].reverse ^= 1;
    mc_try_send_train(ctxt, index);
}

static inline void mc_train_stop(tc_context* const ctxt,
                                 const tc_req* const req) {
    const train_stop* const stop = &req->payload.train_stop;
    const int train_num = train_to_pos(stop->train);
    
    ctxt->pickup[train_num].stop.bank = stop->bank;
    ctxt->pickup[train_num].stop.num  = stop->num;
    
    mc_try_send_train(ctxt, train_num);
}

static inline void tc_init_context(tc_context* const ctxt) {
    memset(ctxt,           0, sizeof(*ctxt));
    memset(ctxt->drivers, -1, sizeof(ctxt->drivers));
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
        case TC_T_TRAIN_LIGHT:
            mc_toggle_light(&context, req.payload.int_value);
            break;
        case TC_T_TRAIN_HORN:
            mc_toggle_horn(&context, req.payload.int_value);
            break;
        case TC_T_TRAIN_STOP:
            mc_train_stop(&context, &req);
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
    
    tc_req req = {
        .type = TC_U_TRAIN_SPEED,
        .payload.train_speed = {
            .num   = (short)train_index,
            .speed = (short)speed
        }
    };
    
    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_reverse(int train) {
    int train_index = train_to_pos(train);
    if (train_index < 0) {
        log("Can't Reverse Invalid Train %d", train);
    	return 0;
    }
    
    tc_req req = {
        .type              = TC_U_TRAIN_REVERSE,
        .payload.int_value = train_index
    };
    
    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_toggle_light(int train) {
    int train_index = train_to_pos(train);
    if (train_index < 0) {
        log("Can't toggle light of invalid train %d", train);
        return 0;
    }
    
    tc_req req = {
        .type              = TC_T_TRAIN_LIGHT,
        .payload.int_value = train_index
    };
    
    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_toggle_horn(int train) {
    int train_index = train_to_pos(train);
    if (train_index < 0) {
        log("Can't toggle horn of invalid train %d", train);
        return 0;
    }
    
    tc_req req = {
        .type              = TC_T_TRAIN_HORN,
        .payload.int_value = train_index
    };
    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}

int train_stop_at(const int train, const int bank, const int num) {
    int train_index = train_to_pos(train);
    if (train_index < 0) {
        log("Can't stop train %d because it does not exist", train);
        return 0;
    }
    
    tc_req req = {
        .type               = TC_T_TRAIN_STOP,
        .payload.train_stop = {
            .train = train,
            .bank  = bank,
            .num   = num
        }
    };
    
    return Send(train_control_tid, (char*)&req, sizeof(req), NULL, 0);
}
