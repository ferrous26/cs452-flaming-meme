
#include <ui.h>
#include <std.h>
#include <debug.h>
#include <syscall.h>
#include <normalize.h>
#include <track_node.h>
#include <track_data.h>
#include <char_buffer.h>

#include <tasks/priority.h>
#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <tasks/courier.h>
#include <tasks/sensor_farm.h>
#include <tasks/train_blaster.h>
#include <tasks/train_console.h>

#define LOG_HEAD    "[TRAIN_CONSOLE]\t"
#define DRIVER_MASK 0x1
#define SENSOR_MASK 0x2
#define TIMER_MASK  0x4

#define BUFFER_SIZE      8
#define SENT_WAITER_SIZE 16

#define CHECK_CREATE(tid, msg) assert(tid >= 0, msg " (%d)", tid)
#define CHECK_SENSOR(s_num) assert(XBETWEEN(s_num, -1, NUM_SENSORS), \
                            "bad sensor num %d", s_num)

TYPE_BUFFER(int, 8)

typedef struct {
    const int  train_pos;

    const int  driver_tid;
    const int  timer_tid;
    const int  sensor_tid;
    int        state;

    int        sensor_last;
    int        sensor_expect;
    int        sensor_timeout;
    int        sensor_iter;

    int_buffer waiters;
    int_buffer next_sensors;

    int        sent_waiters_count;
    int        sent_waiters[SENT_WAITER_SIZE];
    int        sent_sensors[SENT_WAITER_SIZE];

    const track_node* const track;
} tc_context;

static void _get_next_sensors(const track_node* const track,
                              int_buffer* const list) {
    switch (track->type) {
    case NODE_SENSOR:
        intb_produce(list, track->num);
        break;
    case NODE_BRANCH:
        _get_next_sensors(track->edge[DIR_STRAIGHT].dest, list);
        _get_next_sensors(track->edge[DIR_CURVED].dest,   list);
        break;
    case NODE_NONE:
    case NODE_MERGE:
        _get_next_sensors(track->edge[DIR_AHEAD].dest, list);
        break;
    case NODE_ENTER:
        log("WEIRD NODE!! %d", track->type);
    case NODE_EXIT:
        break;
    }
}

static inline bool __attribute__((pure))
_is_console_dead(const tc_context* const ctxt) {
    return 0 ==  ctxt->sent_waiters_count
        && (DRIVER_MASK | TIMER_MASK) == (ctxt->state & (DRIVER_MASK | TIMER_MASK));
}

static void try_send_sensor(tc_context* const ctxt, int sensor_num) {
    int tid, result; UNUSED(result);
    int data[2] = {sensor_num, ctxt->sensor_iter};

    if (0 == intb_count(&ctxt->waiters)) {
        tid = Create(TC_CHILDREN_PRIORITY, sensor_notifier);
        CHECK_CREATE(tid, "Failed to create sensor waiter");

        result = Send(tid, (char*)&data, sizeof(data), NULL, 0);
        assert(result == 0, "Failed send to sensor waiter (%d)", result);
    } else {
        tid = intb_consume(&ctxt->waiters);
        result = Reply(tid, (char*)&data, sizeof(data));
        assert(result == 0, "Failed reply to sensor waiter %d (%d)",
               tid, result);
    }

    // sensor s = pos_to_sensor(sensor_num);
    // log("Waiting For %c%d with %d\t%d",
    //        s.bank, s.num, tid, ctxt->sensor_iter);
    ctxt->sent_waiters_count += 1;
    for (int i = 0; i < SENT_WAITER_SIZE; i++) {
        if (-1 == ctxt->sent_waiters[i]) {
            ctxt->sent_waiters[i] = tid;
            ctxt->sent_sensors[i] = sensor_num;
            return;
        }
    }
    ABORT("TRAIN CONSOLE has run out of waiting space!");
}

static TEXT_COLD void _init_context(tc_context* const ctxt) {
    int tid, result, init_data[2], sensor_data[2];

    memset(ctxt, -1, sizeof(*ctxt));

    ctxt->state              = 0;
    ctxt->sensor_timeout     = 0;
    ctxt->sensor_expect      = 80;
    ctxt->sent_waiters_count = 0;

    intb_init(&ctxt->waiters);
    intb_init(&ctxt->next_sensors);

    // Get the track data from the train driver for use later
    result = Receive(&tid, (char*)init_data, sizeof(init_data));
    assert(tid == myParentTid(), "sent startup from invalid tid %d", tid);
    assert(sizeof(init_data) == result, "Invalid init data %d", result);

    result = Reply(tid, NULL, 0);
    assert(result == 0, LOG_HEAD "Failed responding to parent %d", result);

    *(int*)&ctxt->train_pos     = init_data[0];
    *(track_node**)&ctxt->track = (track_node*)init_data[1];

    assert(XBETWEEN(ctxt->train_pos, -1, NUM_TRAINS),
           "%d is invalid train identifier", ctxt->train_pos);

    // Create the first trask required for sensors
    const int sensor_tid = Create(TC_CHILDREN_PRIORITY, sensor_notifier);
    CHECK_CREATE(tid, "Failed to create first sensor notifier");

    // set up the courier to send data to the driver
    *(int*)&ctxt->driver_tid = Create(TC_DRIVER_COURIER_PRIORITY, courier);
    CHECK_CREATE(ctxt->driver_tid, "Failed to create driver courier");

    *(int*)&ctxt->timer_tid  = Create(TC_CHILDREN_PRIORITY, time_notifier);
    CHECK_CREATE(ctxt->timer_tid, "Failed to create timer courier");

    *(int*)&ctxt->sensor_tid = Create(TC_CHILDREN_PRIORITY, courier);
    CHECK_CREATE(ctxt->sensor_tid, "Failed to create farm courier");

    result = Send(sensor_tid,
                  (char*)&ctxt->sensor_expect, sizeof(ctxt->sensor_expect),
                  NULL, 0);
    assert(result == 0, "Failed to set up first sensor poll %d", result);

    result = Receive(&tid, (char*)&sensor_data, sizeof(sensor_data));
    assert(tid == sensor_tid, "received sensor from %d", tid);
    assert(result == sizeof(sensor_data),
            "Received invalid sensor data %d/%d", result, sizeof(sensor_data));


    intb_produce(&ctxt->waiters, tid);

    blaster_req_type failure = BLASTER_CONSOLE_LOST;
    blaster_req      callin  = {
        .type = BLASTER_UNEXPECTED_SENSOR_FEEDBACK,
        .arg1 = sensor_data[1],
        .arg2 = sensor_data[0]
    };

    courier_package package = {
        .receiver = myParentTid(),
        .message  =  (char*)&callin,
        .size     =  sizeof(callin)
    };

    struct {
        tnotify_header head;
        int body;
    } time_return = {
        .head = {
            .type  = DELAY_RELATIVE,
            .ticks = 1
        },
        .body = ctxt->sensor_iter - 1
    };

    if (sensor_data[0] == REQUEST_REJECTED) {
        package.message = (char*)&failure;
        package.size    = sizeof(failure);
    } else {
        assert(XBETWEEN(callin.arg1, -1, NUM_SENSORS),
               "failed initalizing train %d", callin.arg1);
    }
    result = Send(ctxt->driver_tid,
                  (char*)&package, sizeof(package),
                  NULL, 0);


    assert(result == 0, "Failed handing off package to courier");
    result = Send(ctxt->timer_tid,
                  (char*)&time_return, sizeof(time_return),
                  NULL, 0);
    assert(0 == result, "failed setting up timer %d", result);


    package.receiver = WhoIs((char*)SENSOR_FARM_NAME);
    package.size     = 0;
    result           = Send(ctxt->sensor_tid,
                            (char*)&package, sizeof(package),
                            NULL, 0);
    assert(result == 0, "Failed handing off package to courier");
}

static int __attribute__((pure))
get_sensor_index(const tc_context* const ctxt, const int tid) {
    for (int i = 0; i < SENT_WAITER_SIZE; i++) {
        if (tid == ctxt->sent_waiters[i]) return i;
    }
    return -1;
}

static inline bool can_send_timeout(tc_context* const ctxt) {
    return (ctxt->state & TIMER_MASK)
        &&  ctxt->sensor_timeout > Time();
}

static void try_send_timeout(tc_context* const ctxt) {
    assert(ctxt->sensor_timeout >= 0, "Invalid timeout time %d",
           ctxt->sensor_timeout);

    if (!can_send_timeout(ctxt)) return;
    int   result; UNUSED(result);

    struct {
        tnotify_header head;
        int body[2];
    } delay_req = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = ctxt->sensor_timeout
        },
        .body = {ctxt->sensor_iter,
                 ctxt->sensor_timeout}
    };
    UNUSED(delay_req);

    // log("Waiting Until %d on %d", ctxt->sensor_timeout, ctxt->timer_tid);
    result = Reply(ctxt->timer_tid, (char*)&delay_req, sizeof(delay_req));
    assert(result == 0, "failed sending for timer (%d)", result);

    ctxt->sensor_timeout  = 0;
    ctxt->state          &= ~TIMER_MASK;
}

static inline int __attribute__ ((const))
sizeof_cancel_req (const int ele_count) {
    assert(ele_count > 0 && ele_count <= 8,
           "bad cancel req size (%d)", ele_count);
    return (int)sizeof(sf_type) + ((ele_count << 1) + 1) * (int)sizeof(int);
}

static void reject_remaining_sensors(const tc_context* const ctxt) {
    struct cancel_list {
        sf_type type;
        int size;
        struct {
            int sensor;
            int tid;
        } ele[8];
    } cancel_req;

    cancel_req.size = 0;
    cancel_req.type = SF_W_SENSORS;

    for (int i = 0; i < SENT_WAITER_SIZE; i++) {
        const int tid = ctxt->sent_waiters[i];

        if (-1 != tid) {
            cancel_req.ele[cancel_req.size].tid    = tid;
            cancel_req.ele[cancel_req.size].sensor = ctxt->sent_sensors[i];
            cancel_req.size++;
        }
    }

    if (cancel_req.size > 0) {
        int result = Reply(ctxt->sensor_tid,
                           (char*)&cancel_req,
                           sizeof_cancel_req(cancel_req.size));

        assert(result == 0, "can't send sensor return %d", result);
        UNUSED(result);
    }
}

static inline void _driver_lost(tc_context* const ctxt) {
    reject_remaining_sensors(ctxt);
    ctxt->sensor_iter++;

    const char* sen_name = XBETWEEN(ctxt->sensor_expect, -1, NUM_SENSORS) ?
                               ctxt->track[ctxt->sensor_expect].name : "[!]";

    log(LOG_HEAD "TRAIN %d LOST EXPECTING %s ~ %x",
        ctxt->train_pos, sen_name, ctxt->state);

    ctxt->sensor_expect  = 80;
    ctxt->sensor_timeout = 0;
    try_send_sensor(ctxt, 80);
}

static inline void _driver_reverse(tc_context* const ctxt,
                                   const int s_front,
                                   const int timeout) {
    log(LOG_HEAD "REVERSE!");
    reject_remaining_sensors(ctxt);
    ctxt->sensor_timeout = timeout;
    ctxt->sensor_iter++;

    CHECK_SENSOR(s_front);
    const track_node* const node = ctxt->track[s_front].reverse;
    assert(node - ctxt->track < TRACK_MAX,
           "Invalid track node pointer %p off %d",
           node, node - ctxt->track);

    _get_next_sensors(node->edge[DIR_AHEAD].dest, &ctxt->next_sensors);
    while (intb_count(&ctxt->next_sensors) > 0) {
        const int sensor_num = intb_consume(&ctxt->next_sensors);
        assert(XBETWEEN(sensor_num, -1, NUM_SENSORS),
               "Bad sensor num %d", sensor_num);
        try_send_sensor(ctxt, sensor_num);
    }
}

static inline void _driver_setup_sensors(tc_context* const ctxt,
                                         const int last_sensor,
                                         const int next_sensor,
                                         const int timeout) {
    CHECK_SENSOR(last_sensor);
    CHECK_SENSOR(next_sensor);
    ctxt->sensor_timeout = timeout;
    ctxt->sensor_expect  = next_sensor;
    ctxt->sensor_last    = last_sensor;
    ctxt->sensor_iter++;

    reject_remaining_sensors(ctxt);

    _get_next_sensors(ctxt->track[last_sensor].edge[DIR_AHEAD].dest,
                      &ctxt->next_sensors);

    if (0 == intb_count(&ctxt->next_sensors)) {
        log("no sensors exist in the future, exit?");

        ctxt->sensor_expect = ctxt->track[last_sensor].reverse->num;
        try_send_sensor(ctxt, ctxt->sensor_expect);

        return;
    }

    while (intb_count(&ctxt->next_sensors) > 0) {
        const int sensor_num = intb_consume(&ctxt->next_sensors);
        CHECK_SENSOR(sensor_num);
        try_send_sensor(ctxt, sensor_num);
    }

}

static inline bool _try_return_waiter(tc_context* const ctxt, int tid) {
    int index = get_sensor_index(ctxt, tid);
    if (index < 0) return false;

    ctxt->sent_waiters[index] = -1;
    ctxt->sent_waiters_count  -= 1;

    if (intb_count(&ctxt->waiters) < BUFFER_SIZE) {
        intb_produce(&ctxt->waiters, tid);
    } else {
        log (LOG_HEAD "killing extranious child %d", tid);
        int result = Reply(tid, NULL, 0);
        assert(0 == result, LOG_HEAD "Failed killing waiter task %d", tid);
        UNUSED(result);
    }
    return true;
}

static inline void _perform_dead_check(tc_context* const ctxt) {
    if (_is_console_dead(ctxt)) {
        const blaster_req_type msg = BLASTER_CONSOLE_LOST;

        int result = Reply(ctxt->driver_tid, (char*)&msg, sizeof(msg));
        assert(result  == 0, "Failed reply to driver courier (%d)", result);
        UNUSED(result);

        ctxt->state &= ~DRIVER_MASK;
    }
}

static inline void _print_console_state(tc_context* const ctxt) {

    char  buffer[32];
    char* ptr = vt_goto(buffer, TRAIN_ROW + ctxt->train_pos,
                        TRAIN_CONSOLE_COL);

    if (0 == (ctxt->state & DRIVER_MASK)) {
        *(ptr++)  = 'D';
    } else if (80 == ctxt->sensor_expect) {
        *(ptr++)  = 'L';
    } else if (ctxt->state & TIMER_MASK) {
        *(ptr++)  = 'X';
    } else {
        *(ptr++)  = ' ';
    }

    Puts(buffer, ptr-buffer);
}

void train_console() {
    int tid, result, args[4];
    tc_context context;

    _init_context(&context);
    nik_log(LOG_HEAD "%d DRIVER %d", context.train_pos, context.driver_tid);
    nik_log(LOG_HEAD "%d TIMER  %d", context.train_pos, context.timer_tid);
    nik_log(LOG_HEAD "%d SENSOR %d", context.train_pos, context.sensor_tid);

    FOREVER {
        result = Receive(&tid, (char*)args, sizeof(args));

        if (tid == context.driver_tid) {
            context.state |= DRIVER_MASK;

            switch (args[0]) {
            case 80:
                _driver_lost(&context);
                break;

            case 100:
                assert(result >= 3 * (int)sizeof(int),
                       "train invalid number of args %d", result);
                _driver_reverse(&context, args[1], args[2]);
                break;

            case 200:
                assert(result >= 2 * (int)sizeof(int),
                       "train invalid number of args %d", result);
                log(LOG_HEAD "PASSIVE!");
                context.sensor_timeout = args[1];
                break;

            case 201: {
                log(LOG_HEAD "ADD SENSOR QUERY");
                assert(result >= 3 * (int)sizeof(int),
                       "train invalid number of args %d", result);

                CHECK_SENSOR(args[1]);
                context.sensor_timeout = args[2];

                try_send_sensor(&context, args[1]);
            }   break;
            case 300: {
                log(LOG_HEAD "Delay Until Sensor %d", args[1]);
                CHECK_SENSOR(args[1]);

                reject_remaining_sensors(&context);
                context.sensor_iter++;
                context.sensor_timeout = 0;

                try_send_sensor(&context, args[1]);
            }   break;

            default:
                assert(result >= 3 * (int)sizeof(int),
                       "train invalid number of args %d", result);
                _driver_setup_sensors(&context, args[0], args[1], args[2]);
                break;
            }
            try_send_timeout(&context);

        } else if (tid == context.timer_tid) {
            context.state |= TIMER_MASK;
            if (result == 0 || context.sensor_iter != args[0]) {
                try_send_timeout(&context);

            } else if (context.state & DRIVER_MASK)  {
                blaster_req callin = {
                    .type = BLASTER_CONSOLE_TIMEOUT
                };

                result = Reply(context.driver_tid, (char*)&callin, sizeof(callin));
                assert(result == 0, "Failed reply to driver courier (%d)", result);
                context.state &= ~DRIVER_MASK;
            }
        } else if (tid == context.sensor_tid) {
            context.state |= SENSOR_MASK;

        } else if (_try_return_waiter(&context, tid)) {
            if (context.sensor_iter != args[0]);
            // Everything we want to run has probably happened up top
            else if (REQUEST_REJECTED == args[1]);
            // TODO: someone stole this sensor,
            // need to come up with system - possibly involving the reservations
            //
            // This should actually probably done by misson control, there might
            // be code here such that i can give the other guy control later
            // maybe, this might not be an issue though
            //
            // dont resend since were on the same iteration, just will cause
            // for trains to fight over the bloody sensor, just give up for now
            else if (context.state & DRIVER_MASK) {
                struct blaster_pack {
                    blaster_req_type type;
                    int              time;
                    int              sensor;
                } pack = {
                    .type   = context.sensor_expect == args[2] ?
                                          BLASTER_SENSOR_FEEDBACK :
                                          BLASTER_UNEXPECTED_SENSOR_FEEDBACK,
                    .time   = args[2],
                    .sensor = args[1]
                };

                result = Reply(context.driver_tid, (char*)&pack, sizeof(pack));
                assert(result == 0, "Failed reply to driver courier (%d)", result);
                context.state &= ~DRIVER_MASK;
            }
        } else if (args[0] == BLASTER_CONSOLE_CANCEL) {
            context.sensor_iter++;
            context.sensor_timeout = 0;
            reject_remaining_sensors(&context);
            result = Reply(tid, (char*)&args[0], sizeof(args[0]));
            assert(0 == result, "Failed returning cancel courier");

        } else {
            log(LOG_HEAD, "Train %d got unexpected message from %d",
                context.train_pos, tid);
            assert(false, LOG_HEAD "%d got send from invalid task %d",
                   context.train_pos, tid);
        }

        _perform_dead_check(&context);
        _print_console_state(&context);
    }
}
