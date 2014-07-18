
#include <debug.h>
#include <tasks/train_master.h>
#include <tasks/train_blaster.h>
#include <tasks/train_control.h>
#include <tasks/path_admin.h>
#include <tasks/path_worker.h>
#include <tasks/name_server.h>
#include <tasks/priority.h>
#include <tasks/courier.h>
#include <tasks/clock_server.h>


typedef struct {
    int              train_id;
    int              train_gid;
    char             name[8];

    int              my_tid;             // tid of self
    int              blaster;            // tid of control task
    int              control;            // tid of control task


    int              path_admin;         // tid of the path admin
    int              path_worker;        // tid of the path worker

    int              checkpoint;         // sensor we last had update at
    int              checkpoint_offset;  // in micrometers
    int              checkpoint_time;

    int              destination;        // destination sensor
    int              destination_offset; // in centimeters

    int              path_steps;
    const path_node* path;

} master;


static inline bool master_try_fast_forward(master* const ctxt) {

    int start = -1;
    for (int i = ctxt->path_steps; i >= 0; i--) {
        if (ctxt->path[i].type == PATH_SENSOR) {

            if (start == -1) start = i;

            if (ctxt->path[i].data.sensor == ctxt->checkpoint) {

                if (start != i) {
                    const sensor head =
                        pos_to_sensor(ctxt->path[start].data.sensor);
                    const sensor tail =
                        pos_to_sensor(ctxt->path[i].data.sensor);

                    log("[%s] %c%d >> %c%d",
                        ctxt->name, head.bank, head.num, tail.bank, tail.num);
                }

                ctxt->path_steps = i; // successfully fast forwarded
                return true;
            }
        }
    }

    return false; // nope :(
}

static inline void
master_goto(master* const ctxt,
            const master_req* const req,
            const control_req* const pkg,
            const int tid) {

    const int destination = req->arg1;
    const int offset      = req->arg2;

    if (ctxt->destination == destination) {
        if (master_try_fast_forward(ctxt))
            return;

        // TODO: try to rewind?

        // TODO: handle the offset
    }

    // we may need to release the worker
    if (ctxt->path_worker >= 0) {
        const int result = Reply(ctxt->path_worker, NULL, 0);

        UNUSED(result);
        assert(result == 0,
               "[%s] Failed to release path worker (%d)",
               ctxt->name, result);

        ctxt->path_worker = -1;
    }

    // Looks like we need to get the pather to show us a whole new path
    // https://www.youtube.com/watch?v=-kl4hJ4j48s
    ctxt->destination        = destination;
    ctxt->destination_offset = offset;

    const sensor s = pos_to_sensor(destination);
    log("[%s] Finding a route to %c%d + %d cm",
        ctxt->name, s.bank, s.num, offset);

    pa_request request = {
        .type = PA_GET_PATH,
        .req  = {
            .requestor   = ctxt->my_tid,
            .header      = MASTER_PATH_DATA,
            .sensor_to   = destination,
            .sensor_from = ctxt->checkpoint // hmmm
        }
    };

    // NOTE: we probably should not be sending to the path admin
    //       because servers should not send, but path admin does
    //       almost no work, so it shouldn't be an issue
    int worker_tid;
    int result = Send(ctxt->path_admin,
                      (char*)&request, sizeof(request),
                      (char*)&worker_tid, sizeof(worker_tid));
    assert(result >= 0,
           "[%s] Failed to get path worker (%d)",
           ctxt->name, result);
    assert(worker_tid >= 0,
           "[%s] Invalid path worker tid (%d)",
           ctxt->name, worker_tid);

    result = Reply(tid, (char*)pkg, sizeof(control_req));
    assert(result == 0,
           "[%s] Did not wake up %d (%d)", ctxt->name, tid, result);
}

static inline void master_path_update(master* const ctxt,
                                      const int size,
                                      const path_node* path,
                                      const int tid) {

    const sensor to = pos_to_sensor(ctxt->destination);

    sensor from;
    for (int i = size - 1; i >= 0; i--) {
        if (path[i].type == PATH_SENSOR) {
            from = pos_to_sensor(path[i].data.sensor);
            break;
        }
    }

    log("[%s] Got path %c%d -> %c%d in %d steps",
        ctxt->name, from.bank, from.num, to.bank, to.num, size);

    ctxt->path_worker = tid;
    ctxt->path_steps  = size - 1;
    ctxt->path        = path;

    // Get us up to date on where we are
    master_try_fast_forward(ctxt);
}

static inline void master_location_update(master* const ctxt,
                                          const master_req* const req,
                                          const int tid) {

    log("[%s] Got position update");
    ctxt->checkpoint        = req->arg1;
    ctxt->checkpoint_offset = req->arg2;
    ctxt->checkpoint_time   = req->arg3;

    UNUSED(tid); // TODO: reply to courier...but when?
}

static void master_init(master* const ctxt) {

    memset(ctxt, 0, sizeof(master));

    int tid;
    int init[2];
    int result = Receive(&tid, (char*)&init, sizeof(init));

    if (result != sizeof(init))
        ABORT("[Aunt] Failed to receive init data (%d)", result);

    ctxt->train_id  = init[0];
    ctxt->train_gid = pos_to_train(ctxt->train_id);

    result = Reply(tid, NULL, 0);
    if (result != 0)
        ABORT("[Aunt] Failed to wake up Lenin (%d)", result);

    sprintf(ctxt->name, "AUNT%d", ctxt->train_gid);
    ctxt->name[7] = '\0';
    if (RegisterAs(ctxt->name))
        ABORT("[Aunt] Failed to register train (%d)", ctxt->train_gid);

    ctxt->my_tid  = myTid(); // cache it, avoid unneeded syscalls
    ctxt->blaster = init[1];
    ctxt->control = myParentTid();

    ctxt->path_admin = WhoIs((char*)PATH_ADMIN_NAME);
    assert(ctxt->path_admin >= 0,
           "[%s] I started up before the path admin! (%d)",
           ctxt->name, ctxt->path_admin);

    ctxt->path_worker = -1;
    ctxt->destination = -1;
    ctxt->path_steps  = -1;
}

static void master_init_couriers(master* const ctxt,
                                 const courier_package* const cpkg,
                                 const courier_package* const bpkg) {

    const int control_courier = Create(TRAIN_COURIER_PRIORITY, courier);
    assert(control_courier >= 0,
           "[%s] Error setting up command courier (%d)",
           ctxt->name, control_courier);

    int result = Send(control_courier,
                      (char*)cpkg, sizeof(courier_package),
                      NULL, 0);
    assert(result == 0,
           "[%s] Error sending package to command courier %d",
           ctxt->name, result);

    const int blaster_courier = Create(TRAIN_COURIER_PRIORITY, courier);
    assert(blaster_courier >= 0,
           "[%s] Error setting up blaster courier (%d)",
           ctxt->name, blaster_courier);

    result = Send(blaster_courier,
                  (char*)bpkg, sizeof(courier_package),
                  NULL, 0);
    assert(result == 0,
           "[%s] Error sending package to blaster courier %d",
           ctxt->name, result);

    UNUSED(result);
}

// who really runs Bartertown!
void train_master() {

    master context;
    master_init(&context);

    const control_req control_callin = {
        .type = MASTER_CONTROL_REQUEST_COMMAND,
        .arg1 = context.train_id
    };

    const courier_package control_package = {
        .receiver = context.control,
        .message  = (char*)&control_callin,
        .size     = sizeof(control_callin)
    };

    const blaster_req blaster_callin = {
        .type = MASTER_BLASTER_WHERE_ARE_YOU
    };

    const courier_package blaster_package = {
        .receiver = context.blaster,
        .message  = (char*)&blaster_callin,
        .size     = sizeof(blaster_callin)
    };

    master_init_couriers(&context, &control_package, &blaster_package);


    FOREVER {
        int tid;
        master_req req;

        int result = Receive(&tid, (char*)&req, sizeof(req));
        assert(result > 0,
               "[%s] Invalid message size (%d)", context.name, result);

        switch (req.type) {
        case MASTER_GOTO_LOCATION:
            master_goto(&context, &req, &control_callin, tid);
            break;
        case MASTER_PATH_DATA:
            master_path_update(&context, req.arg1, (path_node*)req.arg2, tid);
            break;
        case MASTER_BLASTER_LOCATION:
            master_location_update(&context, &req, tid);
            break;
        }
    }
}
