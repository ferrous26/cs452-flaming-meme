#include <ui.h>
#include <std.h>
#include <debug.h>
#include <track_data.h>
#include <tasks/priority.h>
#include <tasks/courier.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/term_server.h>
#include <tasks/train_console.h>
#include <tasks/train_server.h>
#include <tasks/train_master.h>
#include <tasks/train_control.h>
#include <tasks/path_admin.h>
#include <tasks/path_worker.h>
#include <tasks/mission_control.h>

#include <tasks/train_blaster.h>
#include <tasks/train_blaster/types.c>
#include <tasks/train_blaster/physics.c>

#define TEMPORARY_ACCEL_FUDGE \
    (ctxt->acceleration_time_fudge_factor - (ctxt->current_speed / 5))


static void blaster_resume_short_moving(blaster* const ctxt, const int time);
static void blaster_reverse_step1(blaster* const ctxt, const int time);
static void blaster_reverse_step2(blaster* const ctxt);
static void blaster_reverse_step3(blaster* const ctxt, const int tid);
static void blaster_reverse_step4(blaster* const ctxt,
                                  const int time,
                                  const int tid);
static void blaster_set_speed(blaster* const ctxt,
                              const int speed,
                              const int time);



static inline int __attribute__((const, always_inline))
blaster_estimate_timeout(int time_current, int time_next) {
    if (time_next <= 0) return 0;
    return time_current + ((time_next * 5) >> 2);
}

void create_console_reply(const blaster* const ctxt,
                          const int s_time,
                          int* result) {

    if (ctxt->reversing) {
        result[0] = 100;
        result[1] = ctxt->next_sensor;
        result[2] = blaster_estimate_timeout(s_time, ctxt->next_time);
    } else {
        result[0] = ctxt->current_sensor;
        result[1] = ctxt->next_sensor;
        result[2] = blaster_estimate_timeout(s_time, ctxt->next_time);
    }
}

static int blaster_create_new_delay_courier(blaster* const ctxt) {

    const int tid = Create(TIMER_COURIER_PRIORITY, time_notifier);

    assert(tid >= 0,
           "[%s] Error setting up a timer notifier (%d)",
           ctxt->name, tid);
    UNUSED(ctxt);

    return tid;
}

static inline void blaster_master_where_am_i(blaster* const ctxt,
                                             const int time) {

    // master is not ready to receive the update :(
    // TODO: maybe we should assert on this?
    if (ctxt->master_courier == -1) return;

    const int       velocity = physics_current_velocity(ctxt);
    const int current_offset = velocity * (time - ctxt->current_time);

    master_req req = {
        .type  = MASTER_BLASTER_LOCATION,
        .arg1  = ctxt->current_sensor,
        .arg2  = ctxt->current_offset + current_offset,
        .arg3  = time,
        .arg4  = velocity,
        .arg5  = physics_current_stopping_distance(ctxt)
    };

    const int result = Reply(ctxt->master_courier, (char*)&req, sizeof(req));
    assert(result == 0,
           "[%s] Failed to respond to master where is command (%d)",
           result);
    UNUSED(result);

    ctxt->master_courier = -1;
}

static void blaster_acceleration_checkpoint(blaster* const ctxt,
                                            const int time) {

     // we need to checkpoint where we are right now
    const track_type type = velocity_type(ctxt->current_sensor);
    const int    velocity = physics_velocity(ctxt,
                                             ctxt->last_speed,
                                             type);
    ctxt->current_offset += (velocity * (time - ctxt->current_time));
    ctxt->current_time    = time;

    ctxt->current_sensor_accelerating = true;
    blaster_master_where_am_i(ctxt, time);
}

static void blaster_checkpoint(blaster* const ctxt,
                               const int sensor_hit,
                               const int timestamp,
                               const int offset,
                               const int service_time) {

    ctxt->last_sensor_accelerating = ctxt->current_sensor_accelerating;
    ctxt->last_sensor              = ctxt->current_sensor;
    ctxt->last_distance            = ctxt->current_distance;
    ctxt->last_time                = ctxt->current_time;

    ctxt->current_sensor           = sensor_hit;
    // ctxt->current_distance         = 0; // this is looked up below
    ctxt->current_offset           = offset;
    ctxt->current_time             = timestamp;

    const int result = get_sensor_from(sensor_hit,
                                       &ctxt->current_distance,
                                       &ctxt->next_sensor);

    assert(result == 0,
           "[%s] failed to get next sensor (%d)",
           ctxt->name, result);
    UNUSED(result);

    if (ctxt->current_distance < 0 && ctxt->current_speed > 0) {
        blaster_reverse_step1(ctxt, service_time);
        // TODO: maybe it should just stop?
        log("[%s] ZOMG, heading towards an exit! Reversing!", ctxt->name);
    }

    const int velocity = physics_current_velocity(ctxt);
    ctxt->next_time    = ctxt->current_distance / velocity;

    physics_update_velocity_ui(ctxt);
    blaster_master_where_am_i(ctxt, service_time);
}

static void  __attribute__ ((unused))
blaster_wait_for_next_estimate(blaster* const ctxt) {

    if (ctxt->current_speed == 0) return;

    ctxt->checkpoint_courier = blaster_create_new_delay_courier(ctxt);

    struct {
        tnotify_header head;
        blaster_req     req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = ctxt->current_time + ctxt->next_time
        },
        .req = {
            .type = BLASTER_NEXT_NODE_ESTIMATE,
            .arg1 = ctxt->next_sensor,
        }
    };

    const int result = Send(ctxt->checkpoint_courier,
                            (char*)&msg, sizeof(msg),
                            NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to setup new delay courier (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);
}

static void blaster_expected_sensor(blaster* const ctxt,
                                    const int tid,
                                    const int sensor_hit,
                                    const int time) {

    log("Next node");

    int result = Reply(tid, NULL, 0);
    assert(result == 0, "[%s] Failed to kill estimation task", ctxt->name);
    UNUSED(result);

    if (tid != ctxt->checkpoint_courier) {
        log("[%s] Invalid checkpoint courier checked-in (%d)",
            ctxt->name, tid);
        return;
    }

    // only update if we are early, because it might be a dead sensor
    if (sensor_hit == ctxt->next_sensor)
        blaster_checkpoint(ctxt, sensor_hit, time, 0, time);
}

static void blaster_start_accelerate(blaster* const ctxt,
                                     const int to_speed,
                                     const int time) {

    if (ctxt->acceleration_courier != -1)
        log("[%s] Overriding previous accel. event, estimates will off",
            ctxt->name);

    ctxt->acceleration_courier = blaster_create_new_delay_courier(ctxt);

    // calculate when to wake up and send off that guy
    const int start_dist = physics_starting_distance(ctxt, to_speed);
    const int start_time = physics_starting_time(ctxt, start_dist);

    const track_location start = {
        .sensor = ctxt->current_sensor,
        .offset = ctxt->current_offset
    };
    track_location locs[4];

    // now find out our end position
    int result = get_position_from(start, locs, start_dist);
    assert(result == 0,
           "[%s] Failed to scan ahead on the track (%d)",
           ctxt->name, result);

    // we need to walk the list and see what we have
    int i = 0;
    FOREVER {
        track_location* const l = &locs[i];

        // crap
        if (l->sensor == AN_EXIT) {
            // we can't have these while accelerating...abort!
            log("[%s] Cannot accelerate into an exit!", ctxt->name);
            blaster_acceleration_checkpoint(ctxt, time);
            blaster_set_speed(ctxt, 0, time);
            return;
        }
        // got to the end
        else if (l->sensor == NUM_SENSORS) {
            break;
        }

        i++;
    }

    // if we travel less than one sensor, then unfudge it
    if (i == 0) {
        locs[i - 1].sensor = ctxt->current_sensor;
        locs[i - 1].offset = start_dist;
    }

    // need to create a message that will restart the acceleration
    // process when it comes back in...
    struct {
        tnotify_header head;
        blaster_req      req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = time + start_time + TEMPORARY_ACCEL_FUDGE
        },
        .req = {
            .type = BLASTER_ACCELERATION_COMPLETE,
            .arg1 = locs[i - 1].sensor,
            .arg2 = locs[i - 1].offset,
            .arg3 = time + start_time
        }
    };

    result = Send(ctxt->acceleration_courier,
                  (char*)&msg, sizeof(msg),
                  NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to setup new delay courier (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);

    ctxt->accelerating = 1;
    blaster_acceleration_checkpoint(ctxt, time);
}

static void blaster_start_deccelerate(blaster* const ctxt,
                                      const int from_speed,
                                      const int time) {

    if (ctxt->acceleration_courier != -1)
        log("[%s] Overriding previous accel. event, estimates will off",
            ctxt->name);

    ctxt->acceleration_courier = blaster_create_new_delay_courier(ctxt);

    // calculate when to wake up and send off that guy
    const int stop_dist = physics_stopping_distance(ctxt, from_speed);
    const int stop_time = physics_stopping_time(ctxt, stop_dist);

    const track_location start = {
        .sensor = ctxt->current_sensor,
        .offset = ctxt->current_offset
    };
    track_location locs[4];

    // now find out our end position
    int result = get_position_from(start, locs, stop_dist);
    assert(result == 0,
           "[%s] Failed to scan ahead on the track (%d)",
           ctxt->name, result);

    // we need to walk the list and see what we have
    int i = 0;
    FOREVER {
        track_location* const l = &locs[i];

        if (l->sensor == AN_EXIT) {
            // we're gonna hit the barrier!
            // so we set our offset to just be the end of the track
            if (i > 0)
                locs[i - 1].offset -= l->offset;
            else
                ABORT("[%s] You found an important test case!", ctxt->name);
            break;
        }
        // got to the end
        else if (l->sensor == NUM_SENSORS) {
            break;
        }

        i++;
    }

    // if we travel less than one sensor, then unfudge it
    if (i == 0) {
        locs[i - 1].sensor = ctxt->current_sensor;
        locs[i - 1].offset = stop_dist;
    }

    // need to create a message that will restart the acceleration
    // process when it comes back in...
    struct {
        tnotify_header head;
        blaster_req     req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = time + stop_time
        },
        .req = {
            .type = BLASTER_ACCELERATION_COMPLETE,
            .arg1 = locs[i - 1].sensor,
            .arg2 = locs[i - 1].offset,
            .arg3 = time + stop_time
        }
    };

    result = Send(ctxt->acceleration_courier,
                  (char*)&msg, sizeof(msg),
                  NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to setup new delay courier (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);

    ctxt->accelerating = -1;
    blaster_acceleration_checkpoint(ctxt, time);
}

static void blaster_complete_accelerate(blaster* const ctxt,
                                        const int tid,
                                        const int sensor_hit,
                                        const int offset,
                                        const int timestamp,
                                        const int service_time) {

    const int expected_tid = ctxt->acceleration_courier;
    ctxt->acceleration_courier = -1;

    // no matter what, we want to kill the delay courier
    const int result = Reply(tid, NULL, 0);
    UNUSED(result);
    assert(result == 0,
           "[%s] Failed to kill old delay courier %d (%d)",
           ctxt->name, tid, result);

    // message came from cancelled acceleration, so we will ignore the data
    if (tid != expected_tid) return;

    // figure out what to do with the checkpoint data
    if (sensor_hit == ctxt->last_sensor) {
        // we're really late, so just ignore this checkpoint data
        log("[%s] Acceleration finished later than expected", ctxt->name);
    }
    else if (sensor_hit == ctxt->current_sensor) {
        // doing ok
        ctxt->current_time   = timestamp;
        ctxt->current_offset = offset;
    }
    else if (sensor_hit == ctxt->current_sensor) {
        // we were early or the sensor is dead
        // for now, assume that sensor is dead, keep this estimate
        // so we need to move all the state up (and over)
        blaster_checkpoint(ctxt, sensor_hit, timestamp, offset, service_time);
    }
    else {
        log("[%s] Acceleration finished after teleportation", ctxt->name);

        const sensor expected = pos_to_sensor(sensor_hit);
        const sensor   actual = pos_to_sensor(ctxt->current_sensor);
        log("[%s] Expected %c%d, but was %c%d",
            ctxt->name, expected.bank, expected.num, actual.bank, actual.num);

        //blaster_checkpoint(ctxt, sensor_hit, timestamp, offset, service_time);
    }

    ctxt->accelerating   = 0;
    ctxt->current_sensor_accelerating = false;

    if (ctxt->short_moving_distance)
        blaster_resume_short_moving(ctxt, timestamp);
    else if (ctxt->reversing == 1)
        blaster_reverse_step2(ctxt);
}

static void blaster_set_speed(blaster* const ctxt,
                             const int speed,
                             const int time) {

    // if we are reversing, then store the speed such that the reverse
    // will choose that speed when it finishes
    if (ctxt->reversing) {
        ctxt->last_speed = ctxt->current_speed;
        return;
    }

    ctxt->last_speed    = ctxt->current_speed;
    ctxt->current_speed = speed;

    put_train_speed((char)ctxt->train_gid, (char)(speed / 10));

    if (ctxt->last_speed > ctxt->current_speed) {
        blaster_start_deccelerate(ctxt, ctxt->last_speed, time);
        if (ctxt->current_speed != 0)
            log("[%s] Unsupported decceleration %d -> %d",
                ctxt->name, ctxt->last_speed / 10, ctxt->current_speed / 10);
    }
    else {
        blaster_start_accelerate(ctxt, speed, time);
        if (ctxt->last_speed != 0)
            log("[%s] Unsupported acceleration %d -> %d",
                ctxt->name, ctxt->last_speed / 10, ctxt->current_speed / 10);
    }

    physics_update_velocity_ui(ctxt);
}

static void blaster_reverse_step1(blaster* const ctxt, const int time) {

    if (ctxt->reverse_courier != -1) {
        log("[%s] Already reversing!", ctxt->name);
        return;
    }

    if (ctxt->current_speed == 0) {
        // doesn't matter, had sex
        put_train_speed((char)ctxt->train_gid, TRAIN_REVERSE);
        ctxt->direction = -ctxt->direction;
        return;
    }

    blaster_set_speed(ctxt, 0, time); // STAHP!
    ctxt->reversing = 1;
}

static void blaster_reverse_step2(blaster* const ctxt) {

    ctxt->reverse_courier = blaster_create_new_delay_courier(ctxt);

    struct {
        tnotify_header head;
        blaster_req     req;
    } msg = {
        .head = {
            .type  = DELAY_RELATIVE,
            .ticks = ctxt->reverse_time_fudge_factor
        },
        .req = {
            .type = BLASTER_REVERSE3
        }
    };

    int result = Send(ctxt->reverse_courier,
                      (char*)&msg, sizeof(msg),
                      NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to setup reverse fudge (%d)",
              ctxt->name, result);

    ctxt->reversing = 2; // all done as far as acceleration logic is concerned
}

static void blaster_reverse_step3(blaster* const ctxt, const int tid) {

    // if the reverse was overridden
    if (tid != ctxt->reverse_courier) {
        log("[%s] Someone overrode a reverse command!", ctxt->name);

        const int result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("[%s] Failed to kill old delay reverse courier (%d)",
                  ctxt->name, result);
        return;
    }

    put_train_speed((char)ctxt->train_gid, TRAIN_REVERSE);
    ctxt->direction = -ctxt->direction;

    struct {
        tnotify_header head;
        blaster_req      req;
    } msg = {
        .head = {
            .type  = DELAY_RELATIVE,
            .ticks = 2 // LOLOLOLOLOLOL
        },
        .req = {
            .type = BLASTER_REVERSE4,
        }
    };

    int result = Reply(ctxt->reverse_courier, (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to minor delay reverse (%d)",
              ctxt->name, result);

    ctxt->reversing = 3;
}

static void blaster_reverse_step4(blaster* const ctxt,
                                  const int time,
                                  const int tid) {
     // if the reverse was overridden
    if (tid != ctxt->reverse_courier) {
        log("[%s] Someone overrode a reverse command!", ctxt->name);

        const int result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("[%s] Failed to kill old delay reverse courier (%d)",
                  ctxt->name, result);
        return;
    }

    ctxt->reversing = 0;
    blaster_set_speed(ctxt, ctxt->last_speed, time);

    const int result = Reply(ctxt->reverse_courier, NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to kill delay reverse courier (%d)",
              ctxt->name, result);

    ctxt->reverse_courier = -1;
}

static inline void blaster_detect_train_direction(blaster* const ctxt,
                                                 const int sensor_hit,
                                                 const int time) {
    const sensor s = pos_to_sensor(sensor_hit);

    if (ctxt->direction != DIRECTION_UNKNOWN) {
        // TODO: proper lost logic
        // log("[%s] Halp! I'm lost at %c%d", ctxt->name, s.bank, s.num);
        return;
    }

    if (s.bank == 'B') {
        ctxt->direction = DIRECTION_FORWARD;
        blaster_reverse_step1(ctxt, time);
    } else {
        ctxt->direction = DIRECTION_BACKWARD;
    }
}

static inline void
blaster_reset_simulation(blaster* const ctxt,
                        const int tid,
                        const blaster_req* const req,
                        const int service_time) {

    const int      sensor_hit = req->arg1;
    const int sensor_hit_time = req->arg2;

    blaster_checkpoint(ctxt, sensor_hit, sensor_hit_time, 0, service_time);

    // need to pass the service_time in case we do a reverse
    blaster_detect_train_direction(ctxt, sensor_hit, service_time);

    physics_update_tracking_ui(ctxt, 0);

    int reply[3];
    create_console_reply(ctxt, sensor_hit_time, reply);

    const int result = Reply(tid, (char*)&reply, sizeof(reply));
    assert(result == 0, "failed to wait for next sensor");
    UNUSED(result);

    //blaster_wait_for_next_estimate(ctxt);
}

static inline void
blaster_adjust_simulation(blaster* const ctxt,
                         const int tid,
                         const blaster_req* const req,
                         const int service_time) {

    const int  sensor_hit = req->arg1;
    const int sensor_time = req->arg2;

    // we have to do some stuff in weird order here
    if (ctxt->last_sensor == sensor_hit) {
        const int expected_v = physics_last_velocity(ctxt);
        const int   actual_v = ctxt->last_distance /
            (sensor_time - ctxt->last_time);
        const int    delta_v = actual_v - expected_v;

        // only do feedback if we are in steady state
        if (!(ctxt->last_sensor_accelerating ||
              ctxt->current_sensor_accelerating))
            physics_feedback(ctxt, actual_v, expected_v, delta_v);

        ctxt->current_time   = sensor_time;
        ctxt->current_offset = 0;

        physics_update_tracking_ui(ctxt, delta_v);
    }
    else {
        const int expected_v = physics_current_velocity(ctxt);
        const int   actual_v = ctxt->current_distance /
            (sensor_time - ctxt->current_time);
        const int    delta_v = actual_v - expected_v;

        // only do feedback if we are in steady state
        if (!(ctxt->last_sensor_accelerating ||
              ctxt->current_sensor_accelerating))
            physics_feedback(ctxt, actual_v, expected_v, delta_v);

        blaster_checkpoint(ctxt, sensor_hit, sensor_time, 0, service_time);
        physics_update_tracking_ui(ctxt, delta_v);
    }

    int reply[3];
    create_console_reply(ctxt, sensor_time, reply);
    const int result = Reply(tid, (char*)&reply, sizeof(reply));
    assert(result == 0,
           "[%s] failed to get next sensor (%d)",
           ctxt->name, result);
    UNUSED(result);

    //blaster_wait_for_next_estimate(ctxt);
}

static void blaster_resume_short_moving(blaster* const ctxt,
                                       const int time) {

    // calculate when to wake up and send off that guy
    const int stop_dist = physics_current_stopping_distance(ctxt);

    // how much further to travel before throwing the stop command
    const int dist_remaining = ctxt->short_moving_distance - stop_dist;

    const int stop_time = dist_remaining / physics_current_velocity(ctxt);

    struct {
        tnotify_header head;
        blaster_req      req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = time + stop_time
        },
        .req = {
            .type = BLASTER_FINISH_SHORT_MOVE
        }
    };

    const int result = Reply(ctxt->acceleration_courier,
                             (char*)&msg, sizeof(msg));
    if (result < 0)
        ABORT("[%s] Failed to send delay for decceleration (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);

    ctxt->short_moving_distance = 0;
}

static void blaster_short_move(blaster* const ctxt, const int offset) {
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
            blaster_set_speed(ctxt, speed, Time());
            return; // done
        }
    }

    log("[%s] Distance %d mm too short to start and stop!",
        ctxt->name, offset);
}

static inline void blaster_wait(blaster* const ctxt,
                                const control_req* const callin) {

    blaster_req req;
    const int control = myParentTid();

    FOREVER {
        int result = Send(control,
                          (char*)callin, sizeof(control_req),
                          (char*)&req,    sizeof(req));
        if (result <= 0)
            ABORT("[%s] Bad train init (%d)", ctxt->name, result);

        int time = Time();

        switch (req.type) {
        case BLASTER_CHANGE_SPEED:
            blaster_set_speed(ctxt, req.arg1, time);
            if (req.arg1 > 0) return;
            break;
        case BLASTER_SHORT_MOVE:
            blaster_short_move(ctxt, req.arg1);
            return;
        case BLASTER_REVERSE:
            blaster_reverse_step1(ctxt, time);
            return;
        case BLASTER_UPDATE_FEEDBACK_THRESHOLD:
            blaster_update_feedback_threshold(ctxt, req.arg1);
            break;
        case BLASTER_UPDATE_FEEDBACK_ALPHA:
            blaster_update_feedback_ratio(ctxt, (ratio)req.arg1);
            break;
        case BLASTER_UPDATE_STOP_OFFSET:
            blaster_update_stopping_distance_offset(ctxt, req.arg1);
            break;
        case BLASTER_UPDATE_CLEARANCE_OFFSET:
            blaster_update_turnout_clearance_offset(ctxt, req.arg1);
            break;
        case BLASTER_UPDATE_FUDGE_FACTOR:
            blaster_update_reverse_time_fudge(ctxt, req.arg1);
            break;
            // We should not get any of these as the first event
            // so we abort if we get them
        case BLASTER_REVERSE2:
        case BLASTER_REVERSE3:
        case BLASTER_REVERSE4:
        case BLASTER_FINISH_SHORT_MOVE:
        case BLASTER_DUMP_VELOCITY_TABLE:
        case BLASTER_ACCELERATION_COMPLETE:
        case BLASTER_NEXT_NODE_ESTIMATE:
        case BLASTER_SENSOR_TIMEOUT:
        case BLASTER_SENSOR_FEEDBACK:
        case BLASTER_UNEXPECTED_SENSOR_FEEDBACK:
        case MASTER_BLASTER_WHERE_ARE_YOU:
        case BLASTER_REQ_TYPE_COUNT:
            ABORT("[%s] Fucked up init somewhere (%d)", ctxt->name, req.type);
        }
    }
}

static TEXT_COLD void blaster_init(blaster* const ctxt) {
    memset(ctxt, 0, sizeof(blaster));

    int tid, init[2];
    int result = Receive(&tid, (char*)init, sizeof(init));

    if (result != sizeof(init))
        ABORT("[Blaster] Failed to initialize (%d from %d)", result, tid);

    ctxt->train_id  = init[0];
    ctxt->train_gid = pos_to_train(ctxt->train_id);

    // Tell the actual train to stop
    put_train_speed((char)ctxt->train_gid, 0);

    //I Want this to explicity never be changeable from here
    *(track_node**)&ctxt->track = (track_node*)init[1];

    result = Reply(tid, NULL, 0);
    if (result < 0)
        ABORT("[Blaster] Failed to initialize (%d)", result);

    // Setup the train name
    sprintf(ctxt->name, "TRAIN%d", ctxt->train_gid);
    ctxt->name[7] = '\0';
    if (RegisterAs(ctxt->name))
        ABORT("[Blaster] Failed to register train (%d)", ctxt->train_gid);

    char buffer[32];
    char* ptr = vt_goto(buffer, TRAIN_ROW + ctxt->train_id, TRAIN_NUMBER_COL);

    ptr = sprintf(ptr, ESC_CODE "%s" COLOUR_SUFFIX "%d" COLOUR_RESET,
                  train_to_colour(ctxt->train_gid),
                  ctxt->train_gid);
    Puts(buffer, ptr - buffer);

    ctxt->last_sensor        = 80;
    ctxt->master_courier     = -1;
    ctxt->reverse_courier    = -1;
    ctxt->checkpoint_courier = -1;
    ctxt->accelerating       =  1;
}

static TEXT_COLD void blaster_init_couriers(blaster* const ctxt,
                                            const courier_package* const package) {

    // Setup the sensor courier
    int tid = Create(TRAIN_CONSOLE_PRIORITY, train_console);
    assert(tid >= 0, "[%s] Failed creating train console (%d)",
           ctxt->name, tid);

    int c_init[2] = {ctxt->train_id, (int)ctxt->track};
    int result    = Send(tid, (char*)c_init, sizeof(c_init), NULL, 0);
    assert(result == 0, "Failed to setup train console! (%d)", result);

    const int control_courier = Create(TRAIN_COURIER_PRIORITY, courier);
    assert(control_courier >= 0,
           "[%s] Error setting up command courier (%d)",
           ctxt->name, control_courier);

    result = Send(control_courier,
                  (char*)package, sizeof(courier_package),
                  NULL, 0);
    assert(result == 0,
           "[%s] Error sending package to command courier %d",
           ctxt->name, result);
}

void train_blaster() {
    blaster context;
    blaster_init(&context);
    blaster_init_physics(&context);

    const control_req callin = {
        .type = BLASTER_CONTROL_REQUEST_COMMAND,
        .arg1 = context.train_id
    };

    const courier_package package = {
        .receiver = myParentTid(),
        .message  = (char*)&callin,
        .size     = sizeof(callin)
    };

    blaster_wait(&context, &callin);
    blaster_init_couriers(&context, &package);

    int tid = 0;
    blaster_req req;

    FOREVER {
        int result = Receive(&tid, (char*)&req, sizeof(req));
        int time   = Time(); // timestamp for request servicing

        assert(req.type < BLASTER_REQ_TYPE_COUNT,
               "[%s] got bad request type %d from %d",
               context.name, req.type, tid);

        switch (req.type) {
        case BLASTER_CHANGE_SPEED:
            blaster_set_speed(&context, req.arg1, time);
            break;

        case BLASTER_REVERSE:
            blaster_reverse_step1(&context, time);
            break;
        case BLASTER_REVERSE2:
            ABORT("[%s] Should never get reverse 2 (%d)", context.name, tid);
        case BLASTER_REVERSE3:
            blaster_reverse_step3(&context, tid);
            continue;
        case BLASTER_REVERSE4:
            blaster_reverse_step4(&context, time, tid);
            continue;

        case MASTER_BLASTER_WHERE_ARE_YOU:
            context.master_courier = tid;
            blaster_master_where_am_i(&context, time);
            continue;

        case BLASTER_SHORT_MOVE:
            blaster_short_move(&context, req.arg1);
            break;
        case BLASTER_FINISH_SHORT_MOVE:
            blaster_set_speed(&context, 0, time);
            continue;

        case BLASTER_DUMP_VELOCITY_TABLE:
            blaster_dump_velocity_table(&context);
            break;
        case BLASTER_UPDATE_FEEDBACK_THRESHOLD:
            blaster_update_feedback_threshold(&context, req.arg1);
            break;
        case BLASTER_UPDATE_FEEDBACK_ALPHA:
            blaster_update_feedback_ratio(&context, (ratio)req.arg1);
            break;
        case BLASTER_UPDATE_STOP_OFFSET:
            blaster_update_stopping_distance_offset(&context, req.arg1);
            break;
        case BLASTER_UPDATE_CLEARANCE_OFFSET:
            blaster_update_turnout_clearance_offset(&context, req.arg1);
            break;
        case BLASTER_UPDATE_FUDGE_FACTOR:
            blaster_update_reverse_time_fudge(&context, req.arg1);
            break;

        case BLASTER_ACCELERATION_COMPLETE:
            blaster_complete_accelerate(&context,
                                        tid,
                                        req.arg1,  // sensor
                                        req.arg2,  // offset
                                        req.arg3,  // expected timestamp
                                        time);     // actual time
            continue;

        case BLASTER_NEXT_NODE_ESTIMATE:
            blaster_expected_sensor(&context, tid, req.arg1, time);
            continue;

        case BLASTER_SENSOR_FEEDBACK:
            blaster_adjust_simulation(&context, tid, &req, time);
            continue;

        case BLASTER_UNEXPECTED_SENSOR_FEEDBACK:
            blaster_reset_simulation(&context, tid, &req, time);
            continue;

        case BLASTER_SENSOR_TIMEOUT: {
            // log("[%s] sensor timeout...", context.name);

            int lost = 80;
            Reply(tid, (char*)&lost, sizeof(lost));
        }   continue;

        case BLASTER_REQ_TYPE_COUNT:
            ABORT("[%s] Illegal type for a train blaster %d from %d",
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
