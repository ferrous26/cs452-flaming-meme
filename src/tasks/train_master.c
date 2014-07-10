#include <std.h>
#include <debug.h>
#include <tasks/priority.h>
#include <tasks/courier.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/term_server.h>
#include <tasks/train_console.h>
#include <tasks/train_server.h>
#include <tasks/train_blaster.h>
#include <tasks/train_master.h>

typedef enum {
    LM_SENSOR,
    LM_BRANCH,
    LM_MERGE,
    LM_EXIT
} landmark;

typedef struct {
    int      train_id;      // internal index
    int      train_gid;     // global identifier
    char     name[8];

    int      courier;       // tid of the blaster courier

    int last_speed;
    int speed;

    // these will usually be based on actual track feedback, unless we have to
    // go a while without track feedback, in which case this will be estimates
    landmark last_landmark;
    int      last_distance; // from last landmark to expected next landmark
    int      last_time;     // time at which the last landmark was hit
    int      last_velocity;

    landmark next_landmark; // expected next landmark
    int      next_distance; // expected distance to next landmark from last_landmark
    int      next_time;     // estimated time of arrival at next landmark
    int      next_velocity;
} master;

static void master_set_speed(master* const ctxt,
                             const int speed,
                             const int time) {
    ctxt->last_speed = ctxt->speed;
    ctxt->speed      = speed;
    ctxt->last_time  = time;

    put_train_speed(ctxt->train_gid, speed);

    // this causes an acceleration event, so we need to timestamp it
}

static inline void master_wait(master* const ctxt,
                               const blaster_req* const callin) {
    master_req req;
    const int blaster = myParentTid();

    FOREVER {
        int result = Send(blaster,
                          (char*)callin, sizeof(blaster_req),
                          (char*)&req,    sizeof(req));
        if (result <= 0)
            ABORT("[Master] Bad train init (%d)", result);

        int time = Time();

        switch (req.type) {
        case MASTER_CHANGE_SPEED:
            master_set_speed(ctxt, req.arg1, time);
            if (req.arg1 > 0) return;
            break;
        case MASTER_REVERSE:
        case MASTER_WHERE_ARE_YOU:
        case MASTER_STOP_AT_SENSOR:
        case MASTER_GOTO_LOCATION:
        case MASTER_DUMP_VELOCITY_TABLE:

        case MASTER_UPDATE_FEEDBACK_THRESHOLD:
        case MASTER_UPDATE_FEEDBACK_ALPHA:
        case MASTER_UPDATE_STOP_OFFSET:
        case MASTER_UPDATE_CLEARANCE_OFFSET:

        case MASTER_ACCELERATION_COMPLETE:
        case MASTER_NEXT_NODE_ESTIMATE:

        case MASTER_SENSOR_FEEDBACK:
        case MASTER_UNEXPECTED_SENSOR_FEEDBACK:
            ABORT("She's moving when we dont tell her too captain!");
        }
    }
}

static void master_init(master* const ctxt) {
    memset(ctxt, 0, sizeof(master));

    int    tid = 0;
    int result = Receive(&tid, (char*)&ctxt->train_id, sizeof(int));
    if (result < 0)
        ABORT("[Master] Failed to initialize (%d)", result);

    ctxt->train_gid = pos_to_train(ctxt->train_id);

    result = Reply(tid, NULL, 0);
    if (result < 0)
        ABORT("[Master] Failed to initialize (%d)", result);


    // Setup the train name
    sprintf(ctxt->name, "TRAIN%d", ctxt->train_gid);
    ctxt->name[7] = '\0';
    if (RegisterAs(ctxt->name))
        ABORT("[Master] Failed to register train (%d)", ctxt->train_gid);

    // Tell the actual train to stop
    master_set_speed(ctxt, 0, Time());
}

static void master_init_courier(master* const ctxt,
                                const courier_package* const package) {
    // Now we can get down to bidness
    int tid = Create(TRAIN_CONSOLE_PRIORITY, train_console);
    assert(tid >= 0, "[Master] Failed creating the train console (%d)", tid);
    UNUSED(tid);

    ctxt->courier = Create(TRAIN_COURIER_PRIORITY, courier);
    assert(ctxt->courier >= 0,
           "[Master] Error setting up command courier (%d)", ctxt->courier);

    int result = Send(ctxt->courier,
                      (char*)package, sizeof(courier_package),
                      NULL, 0);
    assert(result == 0,
           "[Master] Error sending package to command courier %d", result);
    UNUSED(result);
}

void train_master() {
    master context;
    master_init(&context);

    const blaster_req callin = {
        .type = MASTER_BLASTER_REQUEST_COMMAND,
        .arg1 = context.train_id
    };

    master_wait(&context, &callin);

    const courier_package package = {
        .receiver = myParentTid(),
        .message  = (char*)&callin,
        .size     = sizeof(callin)
    };

    master_init_courier(&context, &package);

    int tid = 0;
    master_req req;

    FOREVER {
        int result = Receive(&tid, (char*)&req, sizeof(req));
        int time   = Time();

        switch (req.type) {
        case MASTER_CHANGE_SPEED:
            master_set_speed(&context, req.arg1, time);
            break;
        case MASTER_REVERSE:
        case MASTER_WHERE_ARE_YOU:
        case MASTER_STOP_AT_SENSOR:
        case MASTER_GOTO_LOCATION:
        case MASTER_DUMP_VELOCITY_TABLE:

        case MASTER_UPDATE_FEEDBACK_THRESHOLD:
        case MASTER_UPDATE_FEEDBACK_ALPHA:
        case MASTER_UPDATE_STOP_OFFSET:
        case MASTER_UPDATE_CLEARANCE_OFFSET:

        case MASTER_ACCELERATION_COMPLETE:
        case MASTER_NEXT_NODE_ESTIMATE:

        case MASTER_SENSOR_FEEDBACK:
        case MASTER_UNEXPECTED_SENSOR_FEEDBACK:
            break;
        default:
            ABORT("She's moving when we dont tell her too captain!");
        }

        result = Reply(tid, (char*)&callin, sizeof(callin));
        if (result)
            ABORT("[Master] Failed responding to command courier (%d)",
                  result);
    }
}
