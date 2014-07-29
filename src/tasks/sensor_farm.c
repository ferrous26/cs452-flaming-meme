#include <ui.h>
#include <debug.h>
#include <syscall.h>
#include <normalize.h>
#include <track_data.h>

#include <tasks/priority.h>

#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/train_server.h>


#include <tasks/track_reservation.h>
#include <tasks/sensor_farm.h>

#define LOG_HEAD         "[SENSOR FARM]\t"
#define SENSOR_LIST_SIZE 9

static int sensor_farm_tid;

typedef struct {
    sensor* sensor_insert;
    sensor  recent_sensors[SENSOR_LIST_SIZE];

    int     wait_all;

    int     wait_train [NUM_TRAINS];
    int     sensor_delay[NUM_SENSORS];
    int     sensor_error[NUM_SENSORS];
    
    const track_node* const track;
} sf_context;

static void __attribute__ ((noreturn)) sensor_poll() {
    RegisterAs((char*)SENSOR_POLL_NAME);
    Putc(TRAIN, SENSOR_RESET);

    int       sensor_state[NUM_SENSOR_BANKS];
    const int ptid = myParentTid();
    struct {
        sf_type type;  
        int     sensor;
        int     time;
    } req = {
        .type = SF_U_SENSOR
    };

    Putc(TRAIN, SENSOR_POLL);
    for (int i = 0; i < NUM_SENSOR_BANKS; i++) {
        // set up the inital state
        sensor_state[i] = get_train_bank();
    }

    FOREVER {
        Putc(TRAIN, SENSOR_POLL);
        int c    = get_train_bank();
        req.time = Time();
        int bank = 0;

        FOREVER {
            assert(c >= 0, "sensor_poll got bad return (%d)", c);

            for (int mask = 0x8000, i = 0; mask > 0; mask = mask >> 1, i++) {
                if ((c & mask) > (sensor_state[bank] & mask)) {
                    req.sensor = (bank<<4) + i; 
                    Send(ptid, (char*)&req, sizeof(req), NULL, 0);
                }
            }

            sensor_state[bank++] = c;
            if (bank < NUM_SENSOR_BANKS) {
                c = get_train_bank();
            } else break;
        }
    }

}

inline static void _update_sensors(sf_context* const ctxt,
                                   const int sensor_num,
                                   const int time) {

    assert(XBETWEEN(sensor_num, -1, NUM_SENSORS), 
           "Can't update invalid sensor num %d", sensor_num);

    sensor* next     = ctxt->sensor_insert++;
    *next            = pos_to_sensor(sensor_num);
    const int waiter = ctxt->sensor_delay[sensor_num];

    if(ctxt->sensor_insert == ctxt->recent_sensors + SENSOR_LIST_SIZE) {
        ctxt->sensor_insert = ctxt->recent_sensors;
    }
    memset(ctxt->sensor_insert, 0, sizeof(sensor));

    const int reply[2] = {time, sensor_num};
    if (-1 != waiter) {
        ctxt->sensor_delay[sensor_num] = -1;
        Reply(waiter, (char*)&reply, sizeof(reply));
    } else { 
        int train     = reserve_who_owns(ctxt->track[sensor_num].reverse);
        const int train_tid = train == -1 ? -1 : ctxt->wait_train[train];

        if (-1 != train_tid) {
            Reply(train_tid, (char*)&reply, sizeof(reply));
            ctxt->wait_train[train] = -1;
        } else if (-1 != ctxt->wait_all) {
            Reply(ctxt->wait_all, (char*)&reply, sizeof(reply));
            ctxt->wait_all = -1;
        }
    }

    char buffer[64];
    for(int i =0; next->bank != 0; i++) {
        const char* const output = next->num < 10 ? "%c 0%d": "%c %d";
        char*                pos = vt_goto(buffer, SENSOR_ROW+i, SENSOR_COL);
        pos = sprintf(pos, output, next->bank, next->num);
        Puts(buffer, pos-buffer);

        if (next-- == ctxt->recent_sensors) {
            next = ctxt->recent_sensors + SENSOR_LIST_SIZE-1;
        }
    }
}

static inline void _sensor_delay(sf_context* const ctxt,
                                 const int tid,
                                 const int sensor_num) {
    assert(sensor_num >= 0 && sensor_num < NUM_SENSORS,
           LOG_HEAD "can't delay on invalid sensor %d", sensor_num);

    if (-1 != ctxt->sensor_delay[sensor_num]) {
        log(LOG_HEAD "%d rejected from sensor %d", tid, sensor_num);

        int reply[2] = {REQUEST_REJECTED, sensor_num};
        int result   = Reply(tid, (char*)&reply, sizeof(reply));
        assert(result == 0, "Failed notifing task %d on sensor", tid);
        UNUSED(result);
        return;
    }

    ctxt->sensor_delay[sensor_num] = tid;
}

static inline void _sensor_wakeup(sf_context* const ctxt,
                                  const int check_tid,
                                  const int sensor_num) {

    const int stored_tid = ctxt->sensor_delay[sensor_num];
    int reject_msg[2]    = { REQUEST_REJECTED, sensor_num };

    if (stored_tid == check_tid) {
        int result = Reply(stored_tid, (char*)&reject_msg, sizeof(reject_msg));
        assert(result == 0, "failed to wakeup task %d", result);
        UNUSED(result);
        ctxt->sensor_delay[sensor_num] = -1;
    } else {
        log(LOG_HEAD "sensor %d no longer contains task %d",
            sensor_num, check_tid);
    }
}

static inline void _sensor_delay_any(sf_context* const ctxt, const int tid) {
    const int reject  = REQUEST_REJECTED;
    const int old_any = ctxt->wait_all;

    if (-1 != old_any) {
        log(LOG_HEAD "task %d kicked out %d from any position", tid, old_any);
        Reply(old_any, (char*)&reject, sizeof(reject));
    }

    ctxt->wait_all = tid;
}

static inline void _sensor_delay_train(sf_context* const ctxt,
                                       const int train,
                                       const int tid) {
    assert(XBETWEEN(train, -1, NUM_TRAINS), "invalid train num %d", train);
    assert(-1 == ctxt->wait_train[train],
           "train %d can't have %d since only 1 task can take it at a time %d",
           train, tid, ctxt->wait_train[train]);

    ctxt->wait_train[train] = tid;
}


static inline void __attribute__((always_inline)) _flush_sensors() {
    Putc(TRAIN, SENSOR_POLL);
    for(int i = 0; i < NUM_SENSOR_BANKS; i++) get_train_bank();
}

static TEXT_COLD void _init_farm(sf_context* const ctxt) {
    int tid, result;

    if (RegisterAs((char*)SENSOR_FARM_NAME))
        ABORT("Couldn't register sensor farm");

    memset(ctxt, -1, sizeof(*ctxt));
    memset(ctxt->recent_sensors, 0, sizeof(ctxt->recent_sensors));
    ctxt->sensor_insert = ctxt->recent_sensors;

    result = Receive(&tid, (char*)&ctxt->track, sizeof(ctxt->track));
    assert(result == sizeof(ctxt->track), "Failed to get task track");
    _flush_sensors();

    int poll_tid = Create(SENSOR_POLL_PRIORITY, sensor_poll);
    if (poll_tid < 0)
        ABORT(LOG_HEAD "sensor poll creation failed (%d)", poll_tid);
    result = Reply(tid, NULL, 0);

    assert(result == 0, "failed to wake up parent task");
}

void sensor_farm() {
    int tid,  result;
    sf_context context;

    sensor_farm_tid = myTid();

    _init_farm(&context);
    sf_req req;

    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));
        assert(result >= (int)sizeof(req.type),
               LOG_HEAD "received invalid message %d", result);
        assert(req.type < SF_REQ_TYPE_COUNT,
                LOG_HEAD "received invalid type %d from %d", req.type, tid);

        switch(req.type) {
        case SF_U_SENSOR:
            _update_sensors(&context,
                            req.body.update.sensor,
                            req.body.update.time);
            result = Reply(tid, NULL, 0);
            assert(result == 0, "failed replying to poller %d", result);
            break;

        case SF_D_SENSOR:
            _sensor_delay(&context, tid, req.body.sensor);
            break;

        case SF_D_SENSOR_ANY:
            _sensor_delay_any(&context, tid);
            break;

        case SF_D_SENSOR_TRAIN:
            _sensor_delay_train(&context, req.body.train_num, tid);
            break;

        case SF_W_SENSORS:
            for (int i = 0; i < req.body.rev_list.size; i++) {
                const int s_tid = req.body.rev_list.ele[i].tid;
                const int s_num = req.body.rev_list.ele[i].sensor;
                _sensor_wakeup(&context, s_tid, s_num);
            }
            result = Reply(tid, NULL, 0);
            break;

        case SF_REQ_TYPE_COUNT:
            log(LOG_HEAD "got invalid req type %d from %d", req.type, tid);
            Reply(tid, NULL, 0);
            break;
        }
    }
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

    struct {
        sf_type type;
        int     sensor;
    } req = {
        .type   = SF_D_SENSOR,
        .sensor = sensor_to_pos(sensor_bank, sensor_num)
    };

    int result[2];
    int res = Send(sensor_farm_tid,
                   (char*)&req, sizeof(req),
                   (char*)result, sizeof(result));

    if (res < 0) return res;
    return result[0];
}

int delay_sensor_any() {
    sf_type req = SF_D_SENSOR_ANY;

    int sensor_idx;
    int result = Send(sensor_farm_tid,
                      (char*)&req, sizeof(req),
                      (char*)&sensor_idx, sizeof(sensor_idx));

    if (result < 0) return result;
    return sensor_idx;
}
