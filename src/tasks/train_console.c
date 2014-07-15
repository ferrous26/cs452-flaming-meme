
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>
#include <normalize.h>
#include <track_node.h>
#include <char_buffer.h>

#include <tasks/courier.h>
#include <tasks/priority.h>
#include <tasks/name_server.h>

#include <tasks/clock_server.h>
#include <tasks/mission_control.h>
#include <tasks/train_master.h>
#include <tasks/train_console.h>

#define DRIVER_MASK     0x1
#define SENSOR_MASK     0x2
#define TIMER_MASK      0x4

#define BUFFER_SIZE      8
#define SENT_WAITER_SIZE 16
#define CANCEL_REQUEST (-100)

#define CHECK_CREATE(tid, msg) assert(tid >= 0, msg " (%d)", tid)

TYPE_BUFFER(int, 8);

typedef struct {
    int        docked;
    const int  driver_tid;
    const int  timer_tid;

    int        sensor_last;
    int        sensor_expect;
    int        sensor_timeout;
    int        sensor_iter;

    int_buffer waiters;
    int_buffer next_sensors;
    int        sent_waiters[SENT_WAITER_SIZE];

    const track_node* const track;
} tc_context;


static inline void
_get_next_sensors(const track_node* const track, int_buffer* const list) {

    switch (track->type) {
    case NODE_SENSOR:
        intb_produce(list, track->num);
        break;
    case NODE_BRANCH:
        _get_next_sensors(track->edge[DIR_STRAIGHT].dest, list);
        _get_next_sensors(track->edge[DIR_CURVED].dest,   list);
        break;
    case NODE_MERGE:
        _get_next_sensors(track->edge[DIR_AHEAD].dest, list);
        break;
    case NODE_ENTER:
    case NODE_NONE:
        log("WEIRD NODE!! %d", track->type);
    case NODE_EXIT:
        break;
    }
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
    for (int i = 0; i < SENT_WAITER_SIZE; i++) {
        if (-1 == ctxt->sent_waiters[i]) {
            ctxt->sent_waiters[i] = tid;
            return;
        }
    }
    ABORT("TRAIN CONSOLE has run out of waiting space!");
}

static inline void _init_context(tc_context* const ctxt) {
    int tid, result, sensor_data[2];
    memset(ctxt, -1, sizeof(*ctxt));
    
    ctxt->docked         = 0;
    ctxt->sensor_timeout = 0;
    ctxt->sensor_expect  = 80;
    
    intb_init(&ctxt->waiters);
    intb_init(&ctxt->next_sensors);

    // Get the track data from the train driver for use later 
    result = Receive(&tid, (char*)&ctxt->track, sizeof(ctxt->track));
    assert(tid == myParentTid(), "sent startup from invalid tid %d", tid);
    assert(sizeof(ctxt->track) == result, "%d", result);

    result = Reply(tid, NULL, 0);
    assert(result == 0, "%d", result);

    // Create the first trask required for sensors
    const int sensor_tid = Create(TC_CHILDREN_PRIORITY, sensor_notifier);
    CHECK_CREATE(tid, "Failed to create first sensor notifier");

    result = Send(sensor_tid,
                  (char*)&ctxt->sensor_expect, sizeof(ctxt->sensor_expect),
                  NULL, 0);
    assert(result == 0, "Failed to set up first sensor poll %d", result);

    // set up the courier to send data to the driver 
    *(int*)&ctxt->driver_tid = Create(TC_CHILDREN_PRIORITY, courier);
    CHECK_CREATE(ctxt->driver_tid, "Failed to create driver courier");

    *(int*)&ctxt->timer_tid = Create(TC_CHILDREN_PRIORITY, time_notifier);
    CHECK_CREATE(ctxt->timer_tid, "Failed to create timer courier");

    result = Receive(&tid, (char*)&sensor_data, sizeof(sensor_data));
    assert(tid == sensor_tid, "received sensor from %d", tid);
    assert(result == sizeof(sensor_data),
            "Received invalid sensor data %d/%d", result, sizeof(sensor_data));
    intb_produce(&ctxt->waiters, tid);

    master_req callin = {
        .type = MASTER_UNEXPECTED_SENSOR_FEEDBACK,
        .arg1 = sensor_data[1],
        .arg2 = sensor_data[0]
    };
    courier_package package = {
        .receiver = myParentTid(),
        .message  = (char*)&callin,
        .size     = sizeof(callin)
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

    assert(callin.arg1 >= 0 && callin.arg1 < 80,
          "failed initalizing train %d", callin.arg1);
    result = Send(ctxt->driver_tid, (char*)&package, sizeof(package), NULL, 0);
    assert(result == 0, "Failed handing off package to courier");

    result = Send(ctxt->timer_tid,
                  (char*)&time_return, sizeof(time_return),
                  NULL, 0);
    assert(0 == result, "failed setting up timer %d", result);
}

static int __attribute__((pure))
get_sensor_index(const tc_context* const ctxt, const int tid) {
    for (int i = 0; i < SENT_WAITER_SIZE; i++) {
        if (tid == ctxt->sent_waiters[i]) return i;
    }
    return -1;
}

static void try_send_timeout(tc_context* const ctxt) {
    int result; UNUSED(result);
    if (!((ctxt->docked & TIMER_MASK) && ctxt->sensor_timeout)) return;

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
    
    ctxt->docked &= ~TIMER_MASK;
}

static void reject_remaining_sensors(const tc_context* const ctxt) {
    const int cancel_req = CANCEL_REQUEST;
    
    for (int i = 0; i < SENT_WAITER_SIZE; i++) {
        int tid = ctxt->sent_waiters[i];

        if (-1 != tid) {
            Reply(tid, (char*)&cancel_req, sizeof(cancel_req));
            // not checking this is intentional
        }
    }
}

void train_console() {
    int tid, result, args[4];
    tc_context context;

    
    _init_context(&context);
    // log("DRIVER %d", context.driver_tid);
    // log("TIMER  %d", context.timer_tid);

    FOREVER {
        result = Receive(&tid, (char*)args, sizeof(args));

        if (tid == context.driver_tid) {
            context.docked |= DRIVER_MASK;
            context.sensor_iter++;
            reject_remaining_sensors(&context);

            if (80 == args[0]) {
                log ("TRAIN LOST %x", context.docked);
                context.sensor_expect  = 80;
                context.sensor_timeout = 0; 

                try_send_sensor(&context, 80);
            } else {
                assert(args[0] < 80, "bad sensor %d", args[0]);
                context.sensor_expect  = args[1];
                context.sensor_timeout = args[2]; 
                
                assert(result == 3 * sizeof(int),
                       "bad number of args for driver %d",
                       result / sizeof(int));

                _get_next_sensors(context.track[args[0]].edge[DIR_AHEAD].dest,
                                  &context.next_sensors);

                while (intb_count(&context.next_sensors) > 0) {
                    const int sensor_num = intb_consume(&context.next_sensors);
                    assert(sensor_num < 80 && sensor_num >= 0, "Ba");
                    try_send_sensor(&context, sensor_num); 
                }
            }

            try_send_timeout(&context);

        } else if (tid == context.timer_tid) {
            context.docked |= TIMER_MASK;
            if (result == 0 || context.sensor_iter != args[0]) {
                try_send_timeout(&context);
                continue;
            }

            if (context.docked & DRIVER_MASK)  {
                master_req callin = {
                    .type = MASTER_SENSOR_TIMEOUT
                };

                // log("Sending Timeout %d", args[1]);
                result = Reply(context.driver_tid, (char*)&callin, sizeof(callin));
                assert(result == 0, "Failed reply to driver courier (%d)", result);
                context.docked &= ~DRIVER_MASK;
            }
        } else {
            int index = get_sensor_index(&context, tid);
            assert(index >= 0, "got send from invalid task %d", tid);
            context.sent_waiters[index] = -1;
            
            if (CANCEL_REQUEST != args[1] &&
                    intb_count(&context.waiters) < BUFFER_SIZE) {
                intb_produce(&context.waiters, tid);
            } else {
                // we forced the task back, too dangerous to keep around
                Reply(tid, NULL, 0);
            }


            if (context.sensor_iter != args[0])   continue;
            else if (REQUEST_REJECTED == args[1]) continue;
            // TODO: someone stole this sensor, 
            // need to come up with system - possibly involving the reservations 
            //
            // dont resend since were on the same iteration, just will cause
            // for trains to fight over the bloody sensor, just give up for now
            else if (context.docked & DRIVER_MASK) {
                master_req callin = {
                    .type = context.sensor_expect == args[2] ?
                                        MASTER_SENSOR_FEEDBACK :
                                        MASTER_UNEXPECTED_SENSOR_FEEDBACK,
                    .arg1 = args[2],
                    .arg2 = args[1]
                };

                // sensor s = pos_to_sensor(args[2]); 
                // log("Sending Sensor %c%d", s.bank, s.num); 
                result = Reply(context.driver_tid, (char*)&callin, sizeof(callin));
                assert(result == 0, "Failed reply to driver courier (%d)", result);
                context.docked &= ~DRIVER_MASK;
            }
        }
    }
}
