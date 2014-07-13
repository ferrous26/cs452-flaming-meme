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

#include <tasks/mission_control.h>
#include <tasks/train_master.h>
#include <tasks/train_master/types.c>
#include <tasks/train_master/physics.c>



static inline void
master_update_velocity_ui(const master* const ctxt) {
    // NOTE: we currently display in units of (integer) rounded off mm/s
    char  buffer[32];
    char* ptr = vt_goto(buffer,
                        TRAIN_ROW + ctxt->train_id,
                        TRAIN_SPEED_COL);

    if (ctxt->current_direction > 0)
        ptr = sprintf_string(ptr, COLOUR(CYAN));
    else
        ptr = sprintf_string(ptr, COLOUR(MAGENTA));

    const char* const format =
        ctxt->current_speed && ctxt->current_speed < 150 ?
        "%d " COLOUR_RESET:
        "-      " COLOUR_RESET;

    const int type = velocity_type(ctxt->current_sensor);
    const int    v = physics_velocity(ctxt, ctxt->current_speed, type);
    ptr = sprintf(ptr, format, v / 10);

    Puts(buffer, ptr - buffer);
}

static void master_set_speed(master* const ctxt,
                             const int speed,
                             const int time) {
    ctxt->last_speed    = ctxt->current_speed;
    ctxt->current_speed = speed;

    ctxt->last_time     = time; // time stamp the change in speed
    put_train_speed(ctxt->train_gid, speed / 10);

    ctxt->current_state_accelerating = time;

    // this causes an acceleration event, so we need to wake up
    // when we finish accelerating
    // also need to update estimates

    master_update_velocity_ui(ctxt);
}

static void master_reverse_step1(master* const ctxt, const int time) {

    const int stop_dist = physics_stopping_distance(ctxt);
    const int stop_time =
        physics_stopping_time(ctxt, stop_dist) +
        ctxt->reverse_time_fudge_factor;

    master_set_speed(ctxt, 0, time); // STAHP!
    ctxt->reversing = true;

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = time + stop_time
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

    ctxt->current_direction = -ctxt->current_direction;
    master_set_speed(ctxt, speed, time);
    ctxt->reversing = false;
}

static inline void master_detect_train_direction(master* const ctxt,
                                                 const int tid,
                                                 const int sensor_hit,
                                                 const int time) {

    if (ctxt->current_direction != 0) return;

    if ((sensor_hit >> 4) == 1) {
        ctxt->current_direction = 1;
        master_reverse_step1(ctxt, time);

        const int   none = 80;
        const int result = Reply(tid, (char*)&none, sizeof(none));
        assert(result == 0,
               "[%s] Failed to reply to sensor courier (%d)",
               ctxt->name, result);
        UNUSED(result);
        return;
    }

    ctxt->current_direction = -1;
}

static inline void
master_sensor_feedback(master* const ctxt,
                       const int tid, // the courier
                       const int sensor_hit,
                       const int sensor_time,
                       const int service_time) {

    if (ctxt->sensor_to_stop_at == sensor_hit) {
        master_set_speed(ctxt, 40, service_time);

        ctxt->sensor_to_stop_at = 70;
        const sensor hit = pos_to_sensor(sensor_hit);
        log("[%s] Hit sensor %c%d. Stopping!",
            ctxt->name, hit.bank, hit.num);
    }

    assert(sensor_hit == ctxt->next_sensor,
           "[%s] Bad Expected Sensor %d", ctxt->name, sensor_hit);

    const track_type type = velocity_type(ctxt->current_sensor);
    const int expected_v  = physics_velocity(ctxt,
                                             ctxt->current_speed,
                                             type);
    const int actual_v = ctxt->current_distance /
        (sensor_time - ctxt->current_time);
    const int delta_v  = actual_v - expected_v;

    // TODO: this should be a function of acceleration and not a
    //       constant value
    if (service_time - ctxt->current_state_accelerating > 500)
        physics_feedback(ctxt, actual_v, expected_v);

    ctxt->last_time      = ctxt->current_time;
    ctxt->current_time   = sensor_time;
    ctxt->last_distance  = ctxt->current_distance;
    // current distance updated below
    ctxt->last_sensor    = ctxt->current_sensor;
    ctxt->current_sensor = sensor_hit;

    int result = get_sensor_from(sensor_hit,
                                 &ctxt->current_distance,
                                 &ctxt->next_sensor);
    assert(result == 0, "failed to get next sensor");
    UNUSED(result);

    // ZOMG, we are heading towards an exit!
    if (ctxt->current_distance < 0)
        master_reverse_step1(ctxt, service_time);

    const track_type next_type = velocity_type(ctxt->current_sensor);
    ctxt->current_velocity     = physics_velocity(ctxt,
                                                  ctxt->current_speed,
                                                  next_type);
    ctxt->next_time = ctxt->current_distance / ctxt->current_velocity;

    master_update_velocity_ui(ctxt);
    physics_update_tracking_ui(ctxt, delta_v);

    result = Reply(tid,
                   (char*)&ctxt->next_sensor,
                   sizeof(ctxt->next_sensor));
    assert(result == 0, "failed to get next sensor");
    UNUSED(result);
}

static inline void
master_unexpected_sensor_feedback(master* const ctxt,
                                  const int tid, // the courier
                                  const int sensor_hit,
                                  const int sensor_time,
                                  const int service_time) {

    master_detect_train_direction(ctxt, tid, sensor_hit, service_time);

    const sensor hit = pos_to_sensor(sensor_hit);
    log("[%s] Lost position... %c%d", ctxt->name, hit.bank, hit.num);

    ctxt->last_sensor    = ctxt->current_sensor;
    ctxt->last_distance  = ctxt->current_distance;
    ctxt->last_time      = ctxt->current_time;
    ctxt->current_sensor = sensor_hit;
    ctxt->current_time   = sensor_time;
    // current_distance set below
    // TODO: set other values

    int result = get_sensor_from(sensor_hit,
                                 &ctxt->current_distance,
                                 &ctxt->next_sensor);
    assert(result == 0, "failed to get next sensor");
    UNUSED(result);

    // ZOMG, we are heading towards an exit!
    if (ctxt->current_distance < 0)
        master_reverse_step1(ctxt, service_time);

    const track_type    type = velocity_type(ctxt->current_sensor);
    ctxt->current_velocity   = physics_velocity(ctxt,
                                                ctxt->current_speed,
                                                type);
    ctxt->next_time          = ctxt->current_distance / ctxt->current_velocity;

    master_update_velocity_ui(ctxt);
    physics_update_tracking_ui(ctxt, 0);

    result = Reply(tid,
                   (char*)&ctxt->next_sensor,
                   sizeof(ctxt->next_sensor));
    assert(result == 0, "failed to get next sensor");
    UNUSED(result);
}

static void master_stop_at_sensor(master* const ctxt, const int sensor_idx) {
    const sensor s = pos_to_sensor(sensor_idx);
    log("[%s] Gonna hit the brakes when we hit sensor %c%d",
        ctxt->name, s.bank, s.num);
    ctxt->sensor_to_stop_at = sensor_idx;
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
        case MASTER_STOP_AT_SENSOR:
            master_stop_at_sensor(ctxt, req.arg1);
            break;
        case MASTER_GOTO_LOCATION:
            break;
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
        case MASTER_UPDATE_FUDGE_FACTOR:
            master_update_reverse_time_fudge(ctxt, req.arg1);
            break;

            // We should not get any of these as the first event
            // so we abort if we get them
        case MASTER_ACCELERATION_COMPLETE:
        case MASTER_NEXT_NODE_ESTIMATE:
        case MASTER_WHERE_ARE_YOU:
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
    int tid, init[2];
    int result = Receive(&tid, (char*)init, sizeof(init));

    if (result != sizeof(init))
        ABORT("[Master] Failed to initialize (%d)", result);

    ctxt->train_id = init[0];
    ctxt->train_gid = pos_to_train(ctxt->train_id);

    //I Want this to explicity never be changeable from here
    *(track_node**)&ctxt->track = (track_node*)init[1];

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

    ctxt->sensor_to_stop_at = -1;
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
            break;
        case MASTER_STOP_AT_SENSOR:
            master_stop_at_sensor(&context, req.arg1);
            break;
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
        case MASTER_UPDATE_FUDGE_FACTOR:
            master_update_reverse_time_fudge(&context, req.arg1);
            break;

        case MASTER_ACCELERATION_COMPLETE:
        case MASTER_NEXT_NODE_ESTIMATE:

        case MASTER_SENSOR_FEEDBACK:
            master_sensor_feedback(&context,
                                   tid,
                                   req.arg1, // sensor
                                   req.arg2, // time
                                   time);
            continue;
        case MASTER_UNEXPECTED_SENSOR_FEEDBACK:
            master_unexpected_sensor_feedback(&context,
                                              tid,
                                              req.arg1, // sensor
                                              req.arg2, // time
                                              time);
            continue;
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
