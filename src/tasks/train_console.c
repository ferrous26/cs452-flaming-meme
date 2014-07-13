
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>
#include <normalize.h>
#include <track_node.h>

#include <tasks/courier.h>
#include <tasks/priority.h>
#include <tasks/name_server.h>

#include <tasks/clock_server.h>
#include <tasks/mission_control.h>
#include <tasks/train_master.h>
#include <tasks/train_console.h>

#define DRIVER_MASK 0x1
#define SENSOR_MASK 0x2
#define TIMER_MASK  0x4

typedef struct {
    int        docked;
    const int  driver_tid;
    const int  sensor_tid;
    const int  timer_tid;

    int        sensor_last;
    int        sensor_expect;
    int        sensor_timeout;

    int        sensor_iter;
    master_req next_req;

    const track_node* const track;
} tc_context;


static inline void _init_context(tc_context* const ctxt) {
    int tid, result, sensor_data[2];
    memset(ctxt, -1, sizeof(*ctxt));
    
    ctxt->docked         = SENSOR_MASK;
    ctxt->sensor_expect  = 80;
    ctxt->sensor_timeout = 0;

    // Get the track data from the train driver for use later 
    result = Receive(&tid, (char*)&ctxt->track, sizeof(ctxt->track));
    assert(tid == myParentTid(), "sent startup from invalid tid %d", tid);
    assert(sizeof(ctxt->track) == result, "%d", result);

    result = Reply(tid, NULL, 0);
    assert(result == 0, "%d", result);

    // Create the first trask required for sensors
    *(int*)&ctxt->sensor_tid = Create(TC_CHILDREN_PRIORITY, sensor_notifier);
    assert(ctxt->sensor_tid >= 0, "Error creating Courier to Driver");

    result = Send(ctxt->sensor_tid,
                  (char*)&ctxt->sensor_expect, sizeof(ctxt->sensor_expect),
                  NULL, 0);
    assert(result == 0, "Failed to set up first sensor poll %d", result);

    // set up the courier to send data to the driver 
    *(int*)&ctxt->driver_tid = Create(TC_CHILDREN_PRIORITY, courier);
    assert(ctxt->driver_tid >= 0, "Error creating Courier to Master");

    *(int*)&ctxt->timer_tid = Create(TC_CHILDREN_PRIORITY, time_notifier);

    result = Receive(&tid, (char*)&sensor_data, sizeof(sensor_data));
    assert(result == sizeof(sensor_data),
            "Received invalid sensor data %d/%d", result, sizeof(sensor_data));

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
    tnotify_header time_return = {
        .type  = DELAY_RELATIVE,
        .ticks = 1
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

static void try_send_sensor(tc_context* const ctxt) {
    int result; UNUSED(result);

    if (ctxt->docked & SENSOR_MASK) {
        int data[2] = {ctxt->sensor_expect, ctxt->sensor_iter};
        sensor s    = pos_to_sensor(ctxt->sensor_expect); 
        log ("Waiting for %c%d", s.bank, s.num);

        
        result = Reply(ctxt->sensor_tid, (char*)&data, sizeof(data));
        assert(result == 0, "Failed reply to sensor waiter (%d)", result);
        ctxt->docked &= ~SENSOR_MASK;
    }
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

    log("timeout set to %d", ctxt->sensor_timeout);
    result = Reply(ctxt->timer_tid, (char*)&delay_req, sizeof(delay_req));
    ctxt->docked &= ~TIMER_MASK;
}


void train_console() {
    int tid, result, args[4];
    tc_context context;

    _init_context(&context);

    FOREVER {
        result = Receive(&tid, (char*)args, sizeof(args));

        if (tid == context.driver_tid) {
            context.docked |= DRIVER_MASK;
            context.sensor_iter++;

            if (80 == args[0]) {
                log ("TRAIN LOST");
                context.sensor_expect  = 80;
                context.sensor_timeout = 0; 
                continue;
            } else {
                context.sensor_expect  = args[1];
                context.sensor_timeout = args[2]; 
                assert(result == 3 * sizeof(int),
                       "bad number of args for driver %d",
                       result / sizeof(int));
            }

            try_send_sensor(&context);      
            try_send_timeout(&context);

        } else if (tid == context.sensor_tid) {
            context.docked |= SENSOR_MASK;
            if (context.sensor_iter > args[0]) {
                try_send_sensor(&context);
                continue;
            }

            if (context.docked & DRIVER_MASK) {
                master_req callin = {
                    .type = context.sensor_expect == args[2] ?
                                        MASTER_SENSOR_FEEDBACK :
                                        MASTER_UNEXPECTED_SENSOR_FEEDBACK,
                    .arg1 = args[2],
                    .arg2 = args[1]
                };

                log("Sending Sensor %d", callin.arg2); 
                result = Reply(context.driver_tid, (char*)&callin, sizeof(callin));
                assert(result == 0, "Failed reply to driver courier (%d)", result);
                context.docked &= ~DRIVER_MASK;
            }


        } else if (tid == context.timer_tid) {
            context.docked |= TIMER_MASK;
            if (context.sensor_iter > args[0]) {
                try_send_timeout(&context);
                continue;
            }
            
            if (context.docked & DRIVER_MASK)  {
                master_req callin = {
                    .type = MASTER_SENSOR_TIMEOUT
                };
                log("Sending Timeout %d", args[1]);
                result = Reply(context.driver_tid, (char*)&callin, sizeof(callin));
                assert(result == 0, "Failed reply to driver courier (%d)", result);
                context.docked &= ~DRIVER_MASK;
            }
        }
    }
}
