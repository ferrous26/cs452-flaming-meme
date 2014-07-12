
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/courier.h>
#include <tasks/name_server.h>

#include <tasks/clock_server.h>
#include <tasks/mission_control.h>
#include <tasks/train_master.h>
#include <tasks/train_console.h>

typedef struct {
    const int  driver_tid;
    const int  sensor_tid;

    int        train_tid;
    int        train_docked;
    int        sensor_expect;
    master_req next_req;
} tc_context;

#define TC_CHILDERN_PRIORITY 4

static inline void _init_context(tc_context* const ctxt) {
    memset(ctxt, -1, sizeof(*ctxt));
    int scratch = 80;
    int sensor_data[2];

    *(int*)&ctxt->sensor_tid = Create(TC_CHILDERN_PRIORITY, sensor_notifier);
    assert(ctxt->sensor_tid >= 0, "Error creating Courier to Driver");

    int result = Send(ctxt->sensor_tid, (char*)&scratch, sizeof(scratch),
                      NULL, 0);
    assert(result == 0, "Failed to set up first sensor poll %d", result);

    result = Receive(&scratch, (char*)&sensor_data, sizeof(sensor_data));
    assert(result == sizeof(sensor_data),
            "Received invalid sensor data %d / %d", result, sizeof(sensor_data));

    master_req callin = {
        .type = MASTER_UNEXPECTED_SENSOR_FEEDBACK,
        .arg1 = sensor_data[1],
        .arg2 = sensor_data[0]
    };

    *(int*)&ctxt->driver_tid = Create(TC_CHILDERN_PRIORITY, courier);
    assert(ctxt->driver_tid >= 0, "Error creating Courier to Master");

    assert(callin.arg1 >= 0 && callin.arg1 < 80,
           "failed initalizing train %d", callin.arg1);

    courier_package package = {
        .receiver = myParentTid(),
        .message  = (char*)&callin,
        .size     = sizeof(callin)
    };
    result = Send(ctxt->driver_tid, (char*)&package, sizeof(package), NULL, 0);
    assert(result == 0, "Failed handing off package to courier");

    UNUSED(result);
}

void train_console() {
    int tid, result;

    tc_context context;
    _init_context(&context);

    union {
        int driver;
        int sensor[4];
    } buffer;

    FOREVER {
        result = Receive(&tid, (char*)&buffer, sizeof(buffer));

        if (context.driver_tid == tid) {
            const int sensor_num  = 80;
            context.sensor_expect = buffer.driver;

            result = Reply(context.sensor_tid,
                           (char*)&sensor_num,
                           sizeof(sensor_num));
            assert(result == 0, "Failed reply to sensor waiter (%d)", result);

        } else if (context.sensor_tid == tid) {
            master_req callin = {
                .type          = context.sensor_expect == buffer.sensor[1] ?
                                    MASTER_SENSOR_FEEDBACK :
                                    MASTER_UNEXPECTED_SENSOR_FEEDBACK,
                .arg1 = buffer.sensor[1],
                .arg2 = buffer.sensor[0]
            };

            result = Reply(context.driver_tid, (char*)&callin, sizeof(callin));
            assert(result == 0, "Failed reply to driver courier (%d)", result);
        }
    }
}
