
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/courier.h>
#include <tasks/name_server.h>

#include <tasks/train_station.h>
#include <tasks/mission_control.h>

typedef struct {
    const int driver_tid;
    const int expected_tid;

    int       train_tid;
    int       train_docked;
    
    train_req next_req;
} train_detective_context;

static void create_expected_child(train_detective_context* const ctxt,
                                  const sensor_name* const sensor) {
    *(int*)&ctxt->expected_tid = Create(4, sensor_notifier);

    log("creating sensor process %d %c %d",
        ctxt->expected_tid, sensor->bank, sensor->num);

    Send(ctxt->expected_tid,
         (char*)sensor, sizeof(*sensor),
         NULL, 0);
}

static inline void _init_context(train_detective_context* const ctxt) {
    memset(ctxt, -1, sizeof(*ctxt));

    *(int*)&ctxt->driver_tid = Create(4, courier);
    assert(ctxt->driver_tid >= 0, "Error creating Courier to Driver");

    train_req callin        = {
        .type = TRAIN_HIT_SENSOR
    };
    
    int result = delay_all_sensor(&callin.one.sensor);
    assert(result == sizeof(sensor_name),
           "failed initalizing train %d", result);

    courier_package package = {
        .receiver = myParentTid(),
        .message  = (char*)&callin,
        .size     = sizeof(callin)
    };
    result = Send(ctxt->driver_tid ,(char*)&package, sizeof(package), NULL, 0);



    assert(result == 0, "Failed handing off package to courier");
}

void train_console() {
    int tid, result;

    train_detective_context context;
    _init_context(&context);

    union {
        sensor_name sensor;
        int         int_value;
    } buffer;

    train_req t_req;

    FOREVER {
        result = Receive(&tid, (char*)&buffer, sizeof(buffer));

        if (context.driver_tid == tid) {
            if (-1 == context.expected_tid) {
                create_expected_child(&context, &buffer.sensor);
            } else {
                Reply(context.expected_tid,
                      (char*)&buffer.sensor, sizeof(buffer.sensor));
            }
        } else if (context.expected_tid == tid) {
            t_req.type          = TRAIN_EXPECTED_SENSOR;
            t_req.one.int_value = buffer.int_value;
            Reply(context.driver_tid, (char*)&t_req, sizeof(t_req));
        }
    }
}
