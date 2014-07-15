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


static void master_resume_short_moving(master* const ctxt, const int time);
static void master_reverse_step1(master* const ctxt, const int time);
static void master_reverse_step2(master* const ctxt);
static void master_reverse_step3(master* const ctxt);
static inline void master_reverse_step4(master* const ctxt, const int time);

static inline int __attribute__((const, always_inline))
master_estimate_timeout(int time_current, int time_next) {
    if (time_next <= 0) return 0;
    return time_current + ((time_next * 5) >> 2);
}

static inline void
master_update_velocity_ui(const master* const ctxt) {
    // NOTE: we currently display in units of (integer) rounded off mm/s
    char  buffer[32];
    char* ptr = vt_goto(buffer,
                        TRAIN_ROW + ctxt->train_id,
                        TRAIN_SPEED_COL);;

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

static void master_start_accelerate(master* const ctxt,
                                    const int to_speed,
                                    const int time) {
    ctxt->accelerating = 1;

    // calculate when to wake up and send off that guy
    const int start_dist = physics_starting_distance(ctxt, to_speed);
    const int start_time = physics_starting_time(ctxt, start_dist);

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = time + start_time + 50
        },
        .req = {
            .type = MASTER_ACCELERATION_COMPLETE
        }
    };

    const int result = Reply(ctxt->acceleration_courier,
                             (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to send delay for acceleration (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);


    log("[%s] Accelerating to %d over %d um in %d ticks",
        ctxt->name, to_speed, start_dist, start_time);

    // shuffle state around
}

static void master_start_deccelerate(master* const ctxt,
                                     const int from_speed,
                                     const int time) {
    ctxt->accelerating = -1;

    // calculate when to wake up and send off that guy
    const int stop_dist = physics_stopping_distance(ctxt, from_speed);
    const int stop_time = physics_stopping_time(ctxt, stop_dist);

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = time + stop_time
        },
        .req = {
            .type = MASTER_ACCELERATION_COMPLETE
        }
    };

    const int result = Reply(ctxt->acceleration_courier,
                             (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to send delay for decceleration (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);

    log("[%s] Deccelerating %d um over %d ticks",
        ctxt->name, stop_dist, stop_time);


    // shuffle state around
}

static void master_complete_accelerate(master* const ctxt,
                                       const int time) {
    ctxt->accelerating = 0;

    const track_type type = ctxt->current_sensor == 80 ?
        TRACK_STRAIGHT : velocity_type(ctxt->current_sensor);

    ctxt->current_velocity = physics_velocity(ctxt,
                                              ctxt->current_speed,
                                              type);

    if (ctxt->short_moving_distance)
        master_resume_short_moving(ctxt, time);
    else if (ctxt->reversing)
        master_reverse_step2(ctxt);

    log("[%s] Done acceleration", ctxt->name);
}

static void master_set_speed(master* const ctxt,
                             const int speed,
                             const int time) {
    ctxt->last_speed    = ctxt->current_speed;
    ctxt->current_speed = speed;

    ctxt->last_time     = time; // time stamp the change in speed
    put_train_speed(ctxt->train_gid, speed / 10);

    if (ctxt->last_speed == 0)
        master_start_accelerate(ctxt, speed, time);
    else if (ctxt->current_speed == 0)
        master_start_deccelerate(ctxt, ctxt->last_speed, time);
    else
        log("[%s] Unsupported acceleration %d -> %d",
            ctxt->name, ctxt->last_speed, ctxt->current_speed);

    // also need to update estimates

    master_update_velocity_ui(ctxt);
}

static void master_reverse_step1(master* const ctxt, const int time) {

    if (ctxt->current_speed == 0) {
        // doesn't matter, had sex
        put_train_speed(ctxt->train_gid, TRAIN_REVERSE);
        ctxt->current_direction = -ctxt->current_direction;
        return;
    }

    master_set_speed(ctxt, 0, time); // STAHP!
    ctxt->reversing = 1;
}

static void master_reverse_step2(master* const ctxt) {

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_RELATIVE,
            .ticks = ctxt->reverse_time_fudge_factor
        },
        .req = {
            .type = MASTER_REVERSE3
        }
    };

    int result = Reply(ctxt->acceleration_courier,
                       (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to fudge delay reverse (%d)",
              ctxt->name, result);

    ctxt->reversing = 0; // all done as far as acceleration logic is concerned
}

static void master_reverse_step3(master* const ctxt) {

    put_train_speed(ctxt->train_gid, TRAIN_REVERSE);
    ctxt->current_direction = -ctxt->current_direction;

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_RELATIVE,
            .ticks = 2 // LOLOLOLOLOLOL
        },
        .req = {
            .type = MASTER_REVERSE4,
            .arg1 = ctxt->last_speed
        }
    };

    int result = Reply(ctxt->acceleration_courier,
                       (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to minor delay reverse (%d)",
              ctxt->name, result);
}

static inline void master_reverse_step4(master* const ctxt, const int time) {
    master_set_speed(ctxt, ctxt->last_speed, time);
}

static inline void master_detect_train_direction(master* const ctxt,
                                                 const int sensor_hit,
                                                 const int time) {
    if (ctxt->current_direction != 0) return;

    if ((sensor_hit >> 4) == 1) {
        ctxt->current_direction = 1;
        master_reverse_step1(ctxt, time);
    } else {
        ctxt->current_direction = -1;
    }
}

static inline void __attribute__ ((unused))
master_sensor_feedback(master* const ctxt,
                       const int tid, // the courier
                       const int sensor_hit,
                       const int sensor_time,
                       const int service_time) {

    if (ctxt->sensor_to_stop_at == sensor_hit) {
        master_set_speed(ctxt, 0, service_time);

        ctxt->sensor_to_stop_at = 70;
        const sensor hit = pos_to_sensor(sensor_hit);
        log("[%s] Hit sensor %c%d at %d. Stopping!",
            ctxt->name, hit.bank, hit.num, sensor_time);
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
    if (service_time - ctxt->accelerating > 500)
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

    const int reply[3] = {
        ctxt->current_sensor,
        ctxt->next_sensor,
        master_estimate_timeout(sensor_time, ctxt->next_time)
    };
    result = Reply(tid, (char*)&reply, sizeof(reply));
    assert(result == 0, "failed to get next sensor");
    UNUSED(result);
}

static inline void __attribute__ ((unused))
master_unexpected_sensor_feedback(master* const ctxt,
                                  const int tid, // the courier
                                  const int sensor_hit,
                                  const int sensor_time,
                                  const int service_time) {
    UNUSED(tid);

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
    assert(result == 0, "failed to get next sensor %d", result);

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

    const int reply[3] = {
        ctxt->current_sensor,
        ctxt->next_sensor,
        master_estimate_timeout(sensor_time, ctxt->next_time)
    };

    result = Reply(tid, (char*)&reply, sizeof(reply));
    assert(result == 0, "failed to get next sensor");
    UNUSED(result);
}

static inline void
master_reset_simulation(master* const ctxt,
                        const int tid,
                        const master_req* const req,
                        const int service_time) {

    master_unexpected_sensor_feedback(ctxt, tid, req->arg1, req->arg2, service_time);

    /* // use service_time because this might cause reverse and we */
    /* // want to calculate reverse time from now, now from when the */
    /* // sensor was hit */
    /* master_detect_train_direction(ctxt, req->arg1, service_time); */

    /* // reset tracking state */
    /* ctxt->simualting       = true; */
    /* ctxt->current_landmark = LM_SENSOR; */
    /* ctxt->current_sensor   = req->arg1; */
    /* ctxt->current_distance = 3; // fill this out below */
    /* ctxt->current_time     = req->arg2; */
    /* ctxt->current_velocity = 3; // how do I fill this in */


    /* if (ctxt->path_finding_steps >= 0) { */
    /*     // TODO: find a new path */
    /* } */

    /* const int reply[3] = { */
    /*     ctxt->current_sensor, */
    /*     ctxt->next_sensor, */
    /*     master_estimate_timeout(req->arg2, ctxt->next_time) */
    /* }; */

    /* const int result = Reply(tid, (char*)&reply, sizeof(reply)); */
    /* assert(result == 0, "failed to get next sensor"); */
    /* UNUSED(result); */
}

static inline void
master_adjust_simulation(master* const ctxt,
                         const int tid,
                         const master_req* const req,
                         const int service_time) {

    master_sensor_feedback(ctxt, tid, req->arg1, req->arg2, service_time);


    // only feedback if not accelerating and not just found

    // update all state
}

static void master_stop_at_sensor(master* const ctxt, const int sensor_idx) {
    const sensor s = pos_to_sensor(sensor_idx);
    log("[%s] Gonna hit the brakes when we hit sensor %c%d",
        ctxt->name, s.bank, s.num);
    ctxt->sensor_to_stop_at = sensor_idx;
}

static void master_finish_short_moving(master* const ctxt,
                                       const int time) {
    master_set_speed(ctxt, 0, time);
}

static void master_resume_short_moving(master* const ctxt,
                                       const int time) {

    // calculate when to wake up and send off that guy
    const int stop_dist = physics_current_stopping_distance(ctxt);

    // how much further to travel before throwing the stop command
    const int dist_remaining = ctxt->short_moving_distance - stop_dist;

    const int stop_time = dist_remaining / ctxt->current_velocity;

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = time + stop_time
        },
        .req = {
            .type = MASTER_FINISH_SHORT_MOVE
        }
    };

    const int result = Reply(ctxt->acceleration_courier,
                             (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to send delay for decceleration (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);

    ctxt->short_moving_distance = 0;
}

static void master_short_move(master* const ctxt, const int offset) {
    // offset was given in mm, so convert it to um
    const int offset_um = offset * 1000;
    const int  max_dist = offset_um;

    for (int speed = 120; speed > 10; speed -= 10) {

        const int  stop_dist = physics_stopping_distance(ctxt, speed);
        const int start_dist = physics_starting_distance(ctxt, speed);
        const int   min_dist = stop_dist + start_dist;

        log("Min dist for %d is %d (%d + %d). Looking for %d",
            speed, min_dist, stop_dist, start_dist, max_dist);

        if (min_dist < max_dist) {
            log("[%s] Short move at speed %d", ctxt->name, speed / 10);

            ctxt->short_moving_distance = offset_um - start_dist;
            master_set_speed(ctxt, speed, Time());
            return; // done
        }
    }

    log("[%s] Distance %d mm too short to start and stop!",
        ctxt->name, offset);
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
            return;
        case MASTER_STOP_AT_SENSOR:
            master_stop_at_sensor(ctxt, req.arg1);
            break;
        case MASTER_GOTO_LOCATION:
            break;
        case MASTER_SHORT_MOVE:
            master_short_move(ctxt, req.arg1);
            return;
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
        case MASTER_REVERSE2:
        case MASTER_REVERSE3:
        case MASTER_REVERSE4:
        case MASTER_FINISH_SHORT_MOVE:
        case MASTER_WHERE_ARE_YOU:
        case MASTER_SENSOR_TIMEOUT:
        case MASTER_SENSOR_FEEDBACK:
        case MASTER_UNEXPECTED_SENSOR_FEEDBACK:
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

    ctxt->train_id  = init[0];
    ctxt->train_gid = pos_to_train(ctxt->train_id);

    //I Want this to explicity never be changeable from here
    *(track_node**)&ctxt->track = (track_node*)init[1];
    log("%d", ctxt->track->num);

    result = Reply(tid, NULL, 0);
    if (result < 0)
        ABORT("[Master] Failed to initialize (%d)", result);

    // Setup the train name
    sprintf(ctxt->name, "TRAIN%d", ctxt->train_gid);
    ctxt->name[7] = '\0';
    if (RegisterAs(ctxt->name))
        ABORT("[Master] Failed to register train (%d)", ctxt->train_gid);

    char buffer[32];
    char* ptr = vt_goto(buffer, TRAIN_ROW + ctxt->train_id, TRAIN_NUMBER_COL);

    ptr = sprintf(ptr, "%s%s%s%d%s",
                  ESC_CODE,
                  train_to_colour(ctxt->train_gid),
                  COLOUR_SUFFIX,
                  ctxt->train_gid,
                  COLOUR_RESET);
    Puts(buffer, ptr - buffer);

    // Tell the actual train to stop
    put_train_speed(ctxt->train_gid, 0);

    ctxt->last_sensor        = 80;
    ctxt->sensor_to_stop_at  = -1;
    ctxt->path_finding_steps = -1;
}

static void master_init_courier(master* const ctxt) {

    // Setup the acceleration timer
    ctxt->acceleration_courier = Create(TIME_COURIER_PRIORITY, time_notifier);
    assert(ctxt->acceleration_courier >= 0,
           "[%s] Error setting up the time notifier (%d)",
           ctxt->name, ctxt->acceleration_courier);

    tnotify_header head = {
        .type  = DELAY_RELATIVE,
        .ticks = 0
    };
    int result = Send(ctxt->acceleration_courier,
                  (char*)&head, sizeof(head),
                  NULL, 0);
    assert(result >= 0,
           "[%s] Failed to setup time notifier (%d)",
           ctxt->name, result);

    int tid;
    result = Receive(&tid, NULL, 0);
    assert(tid == ctxt->acceleration_courier,
           "[%s] Got response from incorrect task (%d != %d)",
           ctxt->name, tid, ctxt->acceleration_courier);
}

static void master_init_courier2(master* const ctxt,
                                const courier_package* const package) {

    // Now we can get down to bidness
    int tid = Create(TRAIN_CONSOLE_PRIORITY, train_console);
    assert(tid >= 0, "[%s] Failed creating train console (%d)",
           ctxt->name, tid);
    int result = Send(tid, (char*)&ctxt->track, sizeof(ctxt->track), NULL, 0);

    // Setup the sensor courier
    ctxt->sensor_courier = Create(TRAIN_COURIER_PRIORITY, courier);
    assert(ctxt->sensor_courier >= 0,
           "[%s] Error setting up command courier (%d)",
           ctxt->name, ctxt->sensor_courier);

    result = Send(ctxt->sensor_courier,
                  (char*)package, sizeof(courier_package),
                  NULL, 0);
    assert(result == 0,
           "[%s] Error sending package to command courier %d",
           ctxt->name, result);

    UNUSED(result);
}

void train_master() {
    master context;
    master_init(&context);
    master_init_physics(&context);

    const blaster_req callin = {
        .type = MASTER_BLASTER_REQUEST_COMMAND,
        .arg1 = context.train_id
    };

    const courier_package package = {
        .receiver = myParentTid(),
        .message  = (char*)&callin,
        .size     = sizeof(callin)
    };

    master_init_courier(&context);
    master_wait(&context, &callin);
    master_init_courier2(&context, &package);

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
            master_reverse_step2(&context);
            continue;
        case MASTER_REVERSE3:
            master_reverse_step3(&context);
            continue;
        case MASTER_REVERSE4:
            master_reverse_step4(&context, time);
            continue;

        case MASTER_WHERE_ARE_YOU:
            break;
        case MASTER_STOP_AT_SENSOR:
            master_stop_at_sensor(&context, req.arg1);
            break;
        case MASTER_GOTO_LOCATION:
            break;
        case MASTER_SHORT_MOVE:
            master_short_move(&context, req.arg1);
            break;
        case MASTER_FINISH_SHORT_MOVE:
            master_finish_short_moving(&context, time);
            continue;

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
            master_complete_accelerate(&context, time);
            continue;

        case MASTER_NEXT_NODE_ESTIMATE:
            break;

        case MASTER_SENSOR_FEEDBACK:
            master_adjust_simulation(&context, tid, &req, time);
            continue;
        case MASTER_UNEXPECTED_SENSOR_FEEDBACK:
            master_reset_simulation(&context, tid, &req, time);
            continue;
        case MASTER_SENSOR_TIMEOUT: {
            log ("[%s] sensor timeout...", context.name);

            int lost = 80;
            Reply(tid, (char*)&lost, sizeof(lost));
            continue;
        }

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
