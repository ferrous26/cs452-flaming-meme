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
#include <tasks/path_admin.h>
#include <tasks/path_worker.h>
#include <tasks/mission_control.h>

#include <tasks/train_master.h>
#include <tasks/train_master/types.c>
#include <tasks/train_master/physics.c>

#define TEMPORARY_ACCEL_FUDGE \
    (ctxt->acceleration_time_fudge_factor - (ctxt->current_speed / 5))


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

static int master_create_new_delay_courier(master* const ctxt) {

    const int tid = Create(TIMER_COURIER_PRIORITY, time_notifier);

    assert(tid >= 0,
           "[%s] Error setting up a timer notifier (%d)",
           ctxt->name, tid);
    UNUSED(ctxt);

    return tid;
}

static void master_checkpoint(master* const ctxt, const int time) {

     // we need to checkpoint where we are right now
    const track_type type = velocity_type(ctxt->current_sensor);
    const int    velocity = physics_velocity(ctxt,
                                             ctxt->last_speed,
                                             type);
    ctxt->current_offset += (velocity * (time - ctxt->current_time));
    ctxt->current_time    = time;
}

static void master_start_accelerate(master* const ctxt,
                                    const int to_speed,
                                    const int time) {

    bool new_courier = false;

    if (ctxt->accelerating != 0) {
        log("[%s] Cancelling previous accel., estimates will be fucked up",
            ctxt->name);
        ctxt->acceleration_courier = master_create_new_delay_courier(ctxt);
        new_courier = true;
    }

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
            .ticks = time + start_time + TEMPORARY_ACCEL_FUDGE
        },
        .req = {
            .type = MASTER_ACCELERATION_COMPLETE,
            .arg1 = start_dist,
            .arg2 = start_time
        }
    };


    int result;

    if (new_courier)
        result = Send(ctxt->acceleration_courier,
                      (char*)&msg, sizeof(msg),
                      NULL , 0);
    else
        result = Reply(ctxt->acceleration_courier, (char*)&msg, sizeof(msg));

    if (result < 0)
        ABORT("[%s] Failed to send delay for acceleration (%d) to %d (%d)",
              ctxt->name, result, ctxt->acceleration_courier, new_courier);

    master_checkpoint(ctxt, time);
}

static void master_start_deccelerate(master* const ctxt,
                                     const int from_speed,
                                     const int time) {

    bool new_courier = false;

    if (ctxt->accelerating != 0) {
        log("[%s] Cancelling previous accel., estimates will be fucked up",
            ctxt->name);
        ctxt->acceleration_courier = master_create_new_delay_courier(ctxt);
        new_courier = true;
    }

    ctxt->accelerating = -1;

    // calculate when to wake up and send off that guy
    const int stop_dist = physics_stopping_distance(ctxt, from_speed);
    const int stop_time = physics_stopping_time(ctxt, stop_dist);

    // figure out next event, because that will affect the
    // stop_dist that we pass to the accel complete handler

    // it will also affect what event we expect next and how
    // to handle it

    // we should also update our estimates if we can

    struct {
        tnotify_header head;
        master_req      req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = time + stop_time
        },
        .req = {
            .type = MASTER_ACCELERATION_COMPLETE,
            .arg1 = stop_dist,
            .arg2 = time + stop_time
        }
    };


    int result;

    if (new_courier)
        result = Send(ctxt->acceleration_courier,
                      (char*)&msg, sizeof(msg),
                      NULL, 0);
    else
        result = Reply(ctxt->acceleration_courier, (char*)&msg, sizeof(msg));

    if (result < 0)
        ABORT("[%s] Failed to send delay for decceleration (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);

    master_checkpoint(ctxt, time);
}

static void master_complete_accelerate(master* const ctxt,
                                       const int tid,
                                       const int dist_travelled,
                                       const int time_taken) {

    if (tid != ctxt->acceleration_courier) {
        // message came from cancelled acceleration, so we will ignore
        // the data and tell the courier to die (politely)
        const int result = Reply(tid, NULL, 0);
        UNUSED(result);
        assert(result == 0,
               "[%s] Failed to kill old delay courier (%d)",
               ctxt->name, result);
    }

    ctxt->accelerating    = 0;
    ctxt->current_time   += time_taken;
    ctxt->current_offset += dist_travelled;

    ctxt->current_sensor_accelerating = false;

    if (ctxt->short_moving_distance)
        master_resume_short_moving(ctxt, time_taken);
    else if (ctxt->reversing == 1)
        master_reverse_step2(ctxt);
}

static void master_set_speed(master* const ctxt,
                             const int speed,
                             const int time) {

    // if we are reversing, then store the speed such that the reverse
    // will choose that speed when it finishes
    if (ctxt->reversing) {
        ctxt->last_speed = ctxt->current_speed;
        return;
    }

    ctxt->last_speed                  = ctxt->current_speed;
    ctxt->current_sensor_accelerating = true;
    ctxt->current_speed               = speed;

    put_train_speed(ctxt->train_gid, speed / 10);

    if (ctxt->last_speed == 0)
        master_start_accelerate(ctxt, speed, time);
    else if (ctxt->current_speed == 0)
        master_start_deccelerate(ctxt, ctxt->last_speed, time);
    else
        log("[%s] Unsupported acceleration %d -> %d",
            ctxt->name, ctxt->last_speed / 10, ctxt->current_speed / 10);

    //master_update_estimates(ctxt, time);
    physics_update_velocity_ui(ctxt);
}

static void master_reverse_step1(master* const ctxt, const int time) {

    if (ctxt->current_speed == 0) {
        // doesn't matter, had sex
        put_train_speed(ctxt->train_gid, TRAIN_REVERSE);
        ctxt->direction = -ctxt->direction;
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

    ctxt->reversing = 2; // all done as far as acceleration logic is concerned
}

static void master_reverse_step3(master* const ctxt) {

    put_train_speed(ctxt->train_gid, TRAIN_REVERSE);
    ctxt->direction = -ctxt->direction;

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
        }
    };

    int result = Reply(ctxt->acceleration_courier,
                       (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to minor delay reverse (%d)",
              ctxt->name, result);

    ctxt->reversing = 3;
}

static inline void master_reverse_step4(master* const ctxt, const int time) {
    master_set_speed(ctxt, ctxt->last_speed, time);
    ctxt->reversing = 0;
}

static int master_try_fast_forward_route(master* const ctxt) {
    log("[%s] Attempting to fast forward route", ctxt->name);

    // TODO: fast forward!
    return 0;
}

static void master_find_route_to(master* const ctxt,
                                 const int destination,
                                 const int offset) {

    if (ctxt->destination == destination) {
        if (master_try_fast_forward_route(ctxt))
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
            .sensor_from = ctxt->current_sensor // hmmmm
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
}

static void master_path_update(master* const ctxt,
                               const int tid,
                               const int size,
                               const path_node* path) {
    ctxt->path_worker        = tid;
    ctxt->path_finding_steps = size;
    ctxt->path               = path;

    // TODO: start looking through the path to see where we are...
    //       and what we need to do right now
}

static inline void master_detect_train_direction(master* const ctxt,
                                                 const int sensor_hit,
                                                 const int time) {
    const sensor s = pos_to_sensor(sensor_hit);

    if (ctxt->direction != DIRECTION_UNKNOWN) {
        log("[%s] Halp! I'm lost at %c%d", ctxt->name, s.bank, s.num);
        // TODO: proper lost logic
        return;
    }

    if (s.bank == 'B') {
        ctxt->direction = DIRECTION_FORWARD;
        master_reverse_step1(ctxt, time);
    } else {
        ctxt->direction = DIRECTION_BACKWARD;
    }
}

static inline void
master_reset_simulation(master* const ctxt,
                        const int tid,
                        const master_req* const req,
                        const int service_time) {

    const int      sensor_hit = req->arg1;
    const int sensor_hit_time = req->arg2;

    // need to pass the service_time in case we do a reverse
    master_detect_train_direction(ctxt, sensor_hit, service_time);

    // reset tracking state
    ctxt->simulating       = false;
    ctxt->last_sensor_accelerating = ctxt->current_sensor_accelerating;
    ctxt->current_sensor   = sensor_hit;
    // ctxt->current_distance = 0; // updated below
    ctxt->last_offset      = 0;
    ctxt->current_offset   = 0;
    ctxt->current_time     = sensor_hit_time;

    log("last is %d, current is %d, next is %d",
        ctxt->last_sensor, ctxt->current_sensor, ctxt->next_sensor);
    int result = get_sensor_from(sensor_hit,
                                 &ctxt->current_distance,
                                 &ctxt->next_sensor);
    log("last is %d, current is %d, next is %d",
        ctxt->last_sensor, ctxt->current_sensor, ctxt->next_sensor);

    assert(result == 0, "failed to get next sensor %d", result);

    const int velocity = physics_current_velocity(ctxt);
    ctxt->next_time    = ctxt->current_distance / velocity;

    // TODO: we should probably hit speed 0, then do a reverse and
    // plan how to get out of here
    if (ctxt->current_distance < 0) {
        master_reverse_step1(ctxt, service_time);
        log("[%s] ZOMG, heading towards an exit! Reversing!", ctxt->name);
    }

    // TODO: If we redo the path and it starts with a reverse we may
    // need to ignore the reverse if we just hit the revese above
    if (ctxt->path_finding_steps >= 0)
        master_find_route_to(ctxt,
                             ctxt->destination,
                             ctxt->destination_offset);

    physics_update_velocity_ui(ctxt);
    physics_update_tracking_ui(ctxt, 0);

    const int reply[3] = {
        ctxt->current_sensor,
        ctxt->next_sensor,
        master_estimate_timeout(sensor_hit_time, ctxt->next_time)
    };

    result = Reply(tid, (char*)&reply, sizeof(reply));
    assert(result == 0, "failed to wait for next sensor");
    UNUSED(result);
}

static inline void
master_check_sensor_to_stop_at(master* const ctxt,
                               const int sensor_hit,
                               const int sensor_time,
                               const int service_time) {

    if (ctxt->sensor_to_stop_at != sensor_hit) return;

    master_set_speed(ctxt, 0, service_time);

    ctxt->sensor_to_stop_at = 80;
    const sensor s = pos_to_sensor(sensor_hit);
    log("[%s] Hit sensor %c%d at %d. Stopping!",
        ctxt->name, s.bank, s.num, sensor_time);
}

static inline void
master_adjust_simulation(master* const ctxt,
                         const int tid,
                         const master_req* const req,
                         const int service_time) {

    const int  sensor_hit = req->arg1;
    const int sensor_time = req->arg2;

    assert(sensor_hit == ctxt->next_sensor,
           "[%s] Bad Expected Sensor %d", ctxt->name, sensor_hit);

    ctxt->simulating = true; // yay, simulation is in full force

    master_check_sensor_to_stop_at(ctxt,
                                   sensor_hit,
                                   sensor_time,
                                   service_time);

    const int expected_v = physics_current_velocity(ctxt);
    const int   actual_v = ctxt->current_distance /
        (sensor_time - ctxt->current_time);
    const int    delta_v = actual_v - expected_v;

    // only do feedback if we are in steady state
    if (!(ctxt->last_sensor_accelerating &&
          ctxt->current_sensor_accelerating))
        physics_feedback(ctxt, actual_v, expected_v, delta_v);

    ctxt->last_sensor_accelerating = ctxt->current_sensor_accelerating;
    ctxt->last_sensor              = ctxt->current_sensor;
    ctxt->last_distance            = ctxt->current_distance;
    ctxt->last_offset              = 0;
    ctxt->last_time                = ctxt->current_time;

    ctxt->current_sensor           = sensor_hit;
    // ctxt->current_distance         = 0; // this is looked up below
    ctxt->current_offset           = 0;
    ctxt->current_time             = sensor_time;

    log("last is %d, current is %d, next is %d",
        ctxt->last_sensor, ctxt->current_sensor, ctxt->next_sensor);
    int result = get_sensor_from(sensor_hit,
                                 &ctxt->current_distance,
                                 &ctxt->next_sensor);
    log("last is %d, current is %d, next is %d",
        ctxt->last_sensor, ctxt->current_sensor, ctxt->next_sensor);

    assert(result == 0,
           "[%s] failed to get next sensor (%d)",
           ctxt->name, result);
    UNUSED(result);

    if (ctxt->current_distance < 0) {
        master_reverse_step1(ctxt, service_time);
        log("[%s] ZOMG, heading towards an exit! Reversing!", ctxt->name);
    }

    const int velocity = physics_current_velocity(ctxt);
    ctxt->next_time    = ctxt->current_distance / velocity;

    physics_update_velocity_ui(ctxt);
    physics_update_tracking_ui(ctxt, delta_v);

    const int reply[3] = {
        ctxt->current_sensor,
        ctxt->next_sensor,
        master_estimate_timeout(sensor_time, ctxt->next_time)
    };
    result = Reply(tid, (char*)&reply, sizeof(reply));
    assert(result == 0,
           "[%s] failed to get next sensor (%d)",
           ctxt->name, result);
    UNUSED(result);
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

    const int stop_time = dist_remaining / physics_current_velocity(ctxt);

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

    // TODO: what we want to do is use sensor feedback to update
    //       short move feedback

    for (int speed = 120; speed > 10; speed -= 10) {

        const int  stop_dist = physics_stopping_distance(ctxt, speed);
        const int start_dist = physics_starting_distance(ctxt, speed);
        const int   min_dist = stop_dist + start_dist;

        //        log("Min dist for %d is %d (%d + %d). Looking for %d",
        //            speed, min_dist, stop_dist, start_dist, max_dist);

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

static inline void master_where_are_you(master* const ctxt,
                                        const int tid,
                                        const int time) {

    const int current_offset =
        physics_current_velocity(ctxt) * (time - ctxt->current_time);

    const track_location l = {
        .sensor = ctxt->current_sensor,
        .offset = ctxt->current_offset + current_offset
    };

    const int result = Reply(tid, (char*)&l, sizeof(l));
    assert(result == 0,
           "[%s] Failed to respond to whereis command (%d)",
           result);
    UNUSED(result);
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
        case MASTER_PATH_DATA:
        case MASTER_SENSOR_TIMEOUT:
        case MASTER_SENSOR_FEEDBACK:
        case MASTER_UNEXPECTED_SENSOR_FEEDBACK:
            ABORT("She's moving when we dont tell her too captain!");
        }
    }
}

static void master_init(master* const ctxt) {
    memset(ctxt, 0, sizeof(master));

    ctxt->my_tid = myTid(); // cache it, avoid unneeded syscalls

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

    ctxt->path_admin         = WhoIs((char*)PATH_ADMIN_NAME);
    assert(ctxt->path_admin >= 0,
           "[%s] I started up before the path admin! (%d)",
           ctxt->name, ctxt->path_admin);
}

static void master_init_delay_courier(master* const ctxt, int* c_tid) {

    const int tid = master_create_new_delay_courier(ctxt);

    tnotify_header head = {
        .type  = DELAY_RELATIVE,
        .ticks = 0
    };

    int result = Send(tid, (char*)&head, sizeof(head), NULL, 0);
    assert(result >= 0,
           "[%s] Failed to setup timer notifier (%d)",
           ctxt->name, result);

    int crap;
    result = Receive(&crap, NULL, 0);
    assert(crap == tid,
           "[%s] Got response from incorrect task (%d != %d)",
           ctxt->name, crap, tid);

    *c_tid = tid;
    UNUSED(ctxt);
}

static void master_init_delay_couriers(master* const ctxt) {
    master_init_delay_courier(ctxt, &ctxt->acceleration_courier);
    master_init_delay_courier(ctxt, &ctxt->checkpoint_courier);
}

static void master_init_sensor_courier(master* const ctxt,
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

    master_init_delay_couriers(&context);
    master_wait(&context, &callin);
    master_init_sensor_courier(&context, &package);

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
            master_where_are_you(&context, req.arg1, time);
            break;
        case MASTER_STOP_AT_SENSOR:
            master_stop_at_sensor(&context, req.arg1);
            break;
        case MASTER_GOTO_LOCATION:
            break;
        case MASTER_PATH_DATA:
            master_path_update(&context,
                               tid,
                               req.arg1,
                               (const path_node*)req.arg2);
            continue;

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
            master_complete_accelerate(&context, req.arg1, req.arg2, time);
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
            log("[%s] sensor timeout...", context.name);

            int lost = 80;
            Reply(tid, (char*)&lost, sizeof(lost));
            continue;
        }

        default:
            ABORT("[%s] Illegal type for a train master %d from %d",
                      context.name, req.type, tid);
        }

        // TODO: does sizeof(callin) do what we want or does it go full
        //       sized? we might be able to cut the message size in half
        result = Reply(tid, (char*)&callin, sizeof(callin));
        if (result)
            ABORT("[%s] Failed responding to command courier (%d)",
                  context.name, result);
    }
}
