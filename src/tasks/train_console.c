
#include <std.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/courier.h>
#include <tasks/name_server.h>
#include <tasks/train_station.h>
#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

typedef struct {
    const int cmd_tid;
    const int train_tid;
    const int clock_tid;

    int train_docked;

    int cmd_waiting;
    train_req stored_cmd;

    const mc_req cmd_callin;
} train_detective_context;

static inline void _init_context(train_detective_context* const ctxt) {
    int tid;

    ctxt->train_docked      = 1;
    ctxt->cmd_waiting       = 0;
    *(int*)&ctxt->train_tid = myParentTid();
    int result              = Receive(&tid,
                                      (char*)&ctxt->cmd_callin,
                                      sizeof(ctxt->cmd_callin));

    assert(tid == ctxt->train_tid,
           "Invalid task called into setting up train console %d", tid);
    assert(result == sizeof(ctxt->cmd_callin),
           "received invalid return from %d (%d)", tid, result);

    courier_package pack = {
        .receiver = WhoIs((char*)MISSION_CONTROL_NAME),
        .message  = (char*)&ctxt->cmd_callin,
        .size     = sizeof(ctxt->cmd_callin),
    };

    *(int*)&ctxt->cmd_tid = Create(4, courier);
    assert(ctxt->cmd_tid >= 0,
           "failed to create command listener (%d)", ctxt->cmd_tid);
    result = Send(ctxt->cmd_tid, (char*)&pack, sizeof(pack), NULL, 0);
    assert(result == 0, "Failed sending cmd packet to courier %d", result);
}

static void tc_send_command(train_detective_context* const ctxt,
                            const train_req* const req) {
    assert(ctxt->cmd_waiting == 0,
           "Received train command while command buffer is blocked");

    int result = Reply(ctxt->train_tid, (char*)req, sizeof(*req));
    assert(result == 0, "Train Console Failed to notify train %d", result);
    
    result = Reply(ctxt->cmd_tid,
                   (char*)&ctxt->cmd_callin,
                   sizeof(ctxt->cmd_callin));
    assert(result == 0, "Train Console can't requery command %d", result);
}

static void tc_handle_command(train_detective_context* const ctxt,
                              const train_req* const req) {
    if (ctxt->train_docked) {
        tc_send_command(ctxt, req);
        ctxt->train_docked = 0;
    } else {
        memcpy(&ctxt->stored_cmd, (void*)req, sizeof(*req));
        ctxt->cmd_waiting = 1;
    }
}

static void tc_train_pickup(train_detective_context* const ctxt) {
    if (ctxt->cmd_waiting) {
        tc_send_command(ctxt, &ctxt->stored_cmd);
        ctxt->cmd_waiting = 0;
    } else {
        ctxt->train_docked = 1;
    }
}

void __attribute__((noreturn)) train_console() {
    int tid, result; 
    train_detective_context context;
    _init_context(&context);

    union {
        sensor_name sensor;
        train_req   train;
        int         int_value;
    } buffer;

    FOREVER {
        result = Receive(&tid, (char*)&buffer, sizeof(buffer));
        assert(result >= 0, "hello");

        if (tid == context.train_tid) {
            assert(context.train_docked == 0,
                   "Train somehow double docked int train console");
            tc_train_pickup(&context);
        } else if (tid == context.cmd_tid) {
            tc_handle_command(&context, &buffer.train); 
        }
    }
}
