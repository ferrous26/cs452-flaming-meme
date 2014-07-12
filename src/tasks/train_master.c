#include <ui.h>
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
#include <tasks/train_master/types.c>
#include <tasks/train_master/physics.c>


static inline void
master_update_velocity_ui(const master* const ctxt) {
    // NOTE: we currently display in units of (integer) rounded off mm/s
    char  buffer[16];
    char* ptr      = vt_goto(buffer,
                             TRAIN_ROW + ctxt->train_id,
                             TRAIN_SPEED_COL);

    const char* const format =
        ctxt->current_speed && ctxt->current_speed < 150 ? "%d " : "-    ";

    const int type = velocity_type(ctxt->current_sensor);
    const velocity* const vmap = &ctxt->vmap[type];
    const int    v = physics_velocity(&ctxt->vmap[type],
                                      ctxt->current_speed);
    ptr = sprintf(ptr, format, v / 1000);

    log("Slope %d, Speed %d, Off %d, Delta %d",
        vmap->slope, ctxt->current_speed, vmap->offset, vmap->delta);

    Puts(buffer, ptr - buffer);
}

static void master_set_speed(master* const ctxt,
                             const int speed,
                             const int time) {
    ctxt->last_speed    = ctxt->current_speed;
    ctxt->current_speed = speed;

    ctxt->last_time     = time; // time stamp the change in speed
    put_train_speed(ctxt->train_gid, speed / 10);

    // this causes an acceleration event, so we need to wake up
    // when we finish accelerating
    // also need to update estimates

    master_update_velocity_ui(ctxt);
}

static void master_reverse_step1(master* const ctxt, const int time) {

    const int stop_dist = physics_stopping_distance(ctxt);
    const int stop_time = physics_stopping_time(ctxt, stop_dist);
    master_set_speed(ctxt, 0, time); // STAHP!

    log("[%s] Predict stopping time as %d ticks and %d mm",
        ctxt->name, stop_time, stop_dist / 1000);

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_RELATIVE,
            .ticks = stop_time
        },
        .req = {
            .type = MASTER_REVERSE2
        }
    };

    const int result = Reply(ctxt->acceleration_courier,
                             (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to send delay for reverse (%d)",
              ctxt->name, result);
}

static void master_reverse_step2(master* const ctxt, const int time) {

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_RELATIVE,
            .ticks = 2 // LOLOLOLOL
        },
        .req = {
            .type = MASTER_REVERSE3,
            .arg1 = ctxt->last_speed
        }
    };

    master_set_speed(ctxt, TRAIN_REVERSE * 10, time);

    int result = Reply(ctxt->acceleration_courier,
                       (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to minor delay reverse (%d)",
              ctxt->name, result);
}

static void master_reverse_step3(master* const ctxt,
                                 const int speed,
                                 const int time) {

    // TODO:
    // td_update_train_direction(ctxt, -ctxt->direction);
    master_set_speed(ctxt, speed, time);
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
            master_reverse_step1(ctxt, time);
            break;
        case MASTER_WHERE_ARE_YOU:
        case MASTER_STOP_AT_SENSOR:
        case MASTER_GOTO_LOCATION:

        case MASTER_DUMP_VELOCITY_TABLE:
            master_dump_velocity_table(ctxt);
            break;
        case MASTER_UPDATE_FEEDBACK_THRESHOLD:
            master_update_feedback_threshold(ctxt, req.arg1);
            break;
        case MASTER_UPDATE_FEEDBACK_ALPHA:
            master_update_feedback_ratio(ctxt, (ratio)req.arg1);
            break;
        case MASTER_UPDATE_STOP_OFFSET:
            master_update_stopping_distance_offset(ctxt, req.arg1);
            break;
        case MASTER_UPDATE_CLEARANCE_OFFSET:
            master_update_turnout_clearance_offset(ctxt, req.arg1);
            break;

        case MASTER_ACCELERATION_COMPLETE:
        case MASTER_NEXT_NODE_ESTIMATE:

        case MASTER_SENSOR_FEEDBACK:
        case MASTER_UNEXPECTED_SENSOR_FEEDBACK:

        case MASTER_REVERSE2:
        case MASTER_REVERSE3:
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

    char buffer[16];
    char* ptr = vt_goto(buffer, TRAIN_ROW + ctxt->train_id, TRAIN_NUMBER_COL);

    ptr = sprintf_int(ptr, ctxt->train_gid);
    Puts(buffer, ptr - buffer);

    // Tell the actual train to stop
    master_set_speed(ctxt, 0, Time());
}

static void master_init_courier(master* const ctxt,
                                const courier_package* const package) {
    // Now we can get down to bidness
    int tid = Create(TRAIN_CONSOLE_PRIORITY, train_console);
    assert(tid >= 0,
           "[%s] Failed creating the train console (%d)",
           ctxt->name, tid);
    UNUSED(tid);


    // Setup the sensor courier

    ctxt->sensor_courier = Create(TRAIN_COURIER_PRIORITY, courier);
    assert(ctxt->sensor_courier >= 0,
           "[%s] Error setting up command courier (%d)",
           ctxt->name, ctxt->sensor_courier);

    int result = Send(ctxt->sensor_courier,
                      (char*)package, sizeof(courier_package),
                      NULL, 0);
    assert(result == 0,
           "[%s] Error sending package to command courier %d",
           ctxt->name, result);
    UNUSED(result);


    // Setup the acceleration timer

    ctxt->acceleration_courier = Create(TIME_COURIER_PRIORITY, time_notifier);
    assert(ctxt->acceleration_courier >= 0,
           "[%s] Error setting up the time notifier (%d)",
           ctxt->name, ctxt->acceleration_courier);

    tnotify_header head = {
        .type  = DELAY_RELATIVE,
        .ticks = 0
    };
    result = Send(ctxt->acceleration_courier,
                  (char*)&head, sizeof(head),
                  NULL, 0);
    assert(result >= 0,
           "[%s] Failed to setup time notifier (%d)",
           ctxt->name, result);

    result = Receive(&tid, NULL, 0);
    assert(tid == ctxt->acceleration_courier,
           "[%s] Got response from incorrect task (%d != %d)",
           ctxt->name, tid, ctxt->acceleration_courier);
}

void train_master() {
    master context;
    master_init(&context);
    master_init_physics(&context);

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
        int time   = Time(); // timestamp for request servicing

        switch (req.type) {
        case MASTER_CHANGE_SPEED:
            master_set_speed(&context, req.arg1, time);
            break;

        case MASTER_REVERSE:
            master_reverse_step1(&context, time);
            break;
        case MASTER_REVERSE2:
            master_reverse_step2(&context, time);
            continue;
        case MASTER_REVERSE3:
            master_reverse_step3(&context, req.arg1, time);
            continue;

        case MASTER_WHERE_ARE_YOU:

        case MASTER_STOP_AT_SENSOR:

        case MASTER_GOTO_LOCATION:
            break;

        case MASTER_DUMP_VELOCITY_TABLE:
            master_dump_velocity_table(&context);
            break;
        case MASTER_UPDATE_FEEDBACK_THRESHOLD:
            master_update_feedback_threshold(&context, req.arg1);
            break;
        case MASTER_UPDATE_FEEDBACK_ALPHA:
            master_update_feedback_ratio(&context, (ratio)req.arg1);
            break;
        case MASTER_UPDATE_STOP_OFFSET:
            master_update_stopping_distance_offset(&context, req.arg1);
            break;
        case MASTER_UPDATE_CLEARANCE_OFFSET:
            master_update_turnout_clearance_offset(&context, req.arg1);
            break;

        case MASTER_ACCELERATION_COMPLETE:
        case MASTER_NEXT_NODE_ESTIMATE:

        case MASTER_SENSOR_FEEDBACK:
        case MASTER_UNEXPECTED_SENSOR_FEEDBACK:
            break;
        default:
            ABORT("[%s] Illegal type for a train master %d from %d",
                      context.name, req.type, tid);
        }

        result = Reply(tid, (char*)&callin, sizeof(callin));
        if (result)
            ABORT("[%s] Failed responding to command courier (%d)",
                  context.name, result);
    }
}
