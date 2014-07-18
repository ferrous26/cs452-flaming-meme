
#include <debug.h>
#include <tasks/train_master.h>
#include <tasks/train_blaster.h>
#include <tasks/train_control.h>
#include <tasks/path_admin.h>
#include <tasks/path_worker.h>
#include <tasks/name_server.h>
#include <tasks/priority.h>
#include <tasks/courier.h>


typedef struct {
    int              train_id;
    int              train_gid;
    char             name[8];

    int              my_tid;       // tid of self
    int              blaster;      // tid of control task
    int              control;      // tid of control task


    int              path_admin;         // tid of the path admin
    int              path_worker;        // tid of the path worker

    int              last_checkpoint;        // sensor we last had update at
    int              last_checkpoint_offset; // in micrometers
    int              last_checkpoint_time;

    int              destination;        // destination sensor
    int              destination_offset; // in centimeters

    int              path_finding_steps;
    const path_node* path;

} master;


static inline int master_try_fast_forward(master* const ctxt) {
    log("[%s] Attempting to fast forward route", ctxt->name);

    // TODO: fast forward!
    return 0;
}

static inline void
master_goto(master* const ctxt,
            const int destination,
            const int offset,
            const int tid) {

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
            .sensor_from = ctxt->last_checkpoint // hmmm
        }
    };

    int   worker_tid;
    const int result = Send(ctxt->path_admin,
                            (char*)&request, sizeof(request),
                            (char*)&worker_tid, sizeof(worker_tid));
    assert(result <= 0,
           "[%s] Failed to get path worker (%d)",
           ctxt->name, result);
    assert(worker_tid >= 0,
           "[%s] Invalid path worker tid (%d)",
           ctxt->name, worker_tid);
    UNUSED(result);

    UNUSED(tid); // TODO: reply to courier right away!
}

static inline void master_path_update(master* const ctxt,
                                      const int size,
                                      const path_node* path,
                                      const int tid) {

    ctxt->path_worker        = tid;
    ctxt->path_finding_steps = size;
    ctxt->path               = path;

    // TODO: start looking through the path to see where we are...
    //       and what we need to do right now
}

static inline void master_location_update(master* const ctxt,
                                          const master_req* const req,
                                          const int tid) {

    ctxt->last_checkpoint        = req->arg1;
    ctxt->last_checkpoint_offset = req->arg2;
    ctxt->last_checkpoint_time   = req->arg3;

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

    ctxt->path_worker          = -1;
    ctxt->destination          = -1;
    ctxt->path_finding_steps   = -1;
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
            master_goto(&context, req.arg1, req.arg2, tid);
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
