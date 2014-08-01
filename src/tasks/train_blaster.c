#include <ui.h>
#include <std.h>
#include <debug.h>
#include <track_data.h>

#include <tasks/priority.h>
#include <tasks/term_server.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>
#include <tasks/train_server.h>

#include <tasks/courier.h>
#include <tasks/path_admin.h>
#include <tasks/path_worker.h>
#include <tasks/train_console.h>
#include <tasks/train_control.h>
#include <tasks/mission_control.h>

#include <tasks/train_master.h>
#include <tasks/train_blaster.h>
#include <tasks/train_blaster_types.h>
#include <tasks/train_blaster/physics.c>

#define TEMPORARY_ACCEL_FUDGE \
    (ctxt->acceleration_time_fudge_factor - (truth.speed / 5))

static void blaster_master_reverse(blaster* const ctxt,
                                   const int time,
                                   const int tid);
static void blaster_reverse_step1(blaster* const ctxt, const int time);
static void blaster_reverse_step2(blaster* const ctxt);
static void blaster_reverse_step3(blaster* const ctxt,
                                  const int tid,
                                  const int time);
static void blaster_reverse_step4(blaster* const ctxt,
                                  const int tid,
                                  const int time);
static void blaster_set_speed(blaster* const ctxt,
                              const int speed,
                              const int time);


static inline int __attribute__ ((const))
blaster_distance_remaining(train_state* const state) {
    return state->next_distance - state->location.offset;
}

static inline int __attribute__((const, always_inline))
blaster_estimate_timeout(int time_current, int time_next) {
    if (time_next <= 0) return 0;
    return time_current + ((time_next * 5) >> 2);
}

const char* event_to_str(const train_event event) {
    switch (event) {
    case EVENT_ACCELERATION:      return "Acceleration Event";
    case EVENT_VIRTUAL:           return "Virtual Checkpoint Event";
    case EVENT_SENSOR:            return "Sensor Checkpoint Event";
    case EVENT_UNEXPECTED_SENSOR: return "Unexpected Sensor Checkpoint Event";
    }
    return "Unknown Event";
}

// choose a pseudo-magic value for speed to use in the calculation
static int __attribute__ ((unused)) blaster_estimate_speed(blaster* const ctxt) {
    return (truth.speed + truth.next_speed) >> 2;
}

#ifndef BANANANANANANANANANANANA
#define blaster_debug_state( ... )
#else
static void __attribute__ ((unused))
blaster_debug_state(blaster* const ctxt,
                    const train_state* const state) {

    const char*  event = event_to_str(state->event);
    const sensor current_loc = pos_to_sensor(state->location.sensor);
    const sensor    next_loc = pos_to_sensor(state->next_location.sensor);

    const int current_offset = state->location.offset / 1000;
    const int    next_offset = state->next_location.offset / 1000;

    char  buffer[512];
    char* ptr = log_start(buffer);
    ptr = sprintf(ptr,
                  "[%s]\n"
                  "%s @ %d ticks (travelling at %c%d.%d%s)\n"
                  "       Location: %c%d + %d mm (travelled %d mm from last)\n"
                  "  Next Location: %c%d + %d mm in %d mm @ %d ticks\n",
                  ctxt->name,
                  event,
                  state->timestamp,
                  state->direction ? '+' : '-',
                  state->speed / 10,
                  state->speed % 10,
                  state->is_accelerating ? " accelerating" : "",
                  current_loc.bank,
                  current_loc.num,
                  current_offset,
                  state->distance / 1000,
                  next_loc.bank,
                  next_loc.num,
                  next_offset,
                  state->next_distance / 1000,
                  state->next_timestamp);

    ptr = log_end(ptr);
    Puts(buffer, ptr - buffer);
}
#endif

static void create_console_reply(const blaster* const ctxt,
                                 const int s_time,
                                 int* result) {

    if (ctxt->reversing) {
        result[0] = 100;
        result[1] = truth.next_location.sensor;
        result[2] = blaster_estimate_timeout(s_time,
                                             truth.next_timestamp -
                                             truth.timestamp);
    } else {
        result[0] = truth.location.sensor;
        result[1] = truth.next_location.sensor;
        result[2] = blaster_estimate_timeout(s_time,
                                             truth.next_timestamp -
                                             truth.timestamp);
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

    physics_update_velocity_ui(ctxt);
    physics_update_prediction_ui(ctxt, time);
    blaster_debug_state(ctxt, &truth);

    // master is not ready to receive the update :(
    // TODO: maybe we should assert on this? (to make sure we are not late)
    if (ctxt->master_courier == -1) return;

    const int velocity = physics_current_velocity(ctxt);
    const int   offset = truth.location.offset +
        (velocity * (time - truth.timestamp));

    // do not allow the offset to be more than distance to next sensor
    const int new_offset = MIN(offset, truth.next_distance);

    ctxt->master_state                 = truth;
    ctxt->master_state.timestamp       = time;
    ctxt->master_state.location.offset = new_offset;

    master_req req = {
        .type = MASTER_BLASTER_LOCATION,
        .arg1 = (int)&ctxt->master_state
    };

    const int result = Reply(ctxt->master_courier, (char*)&req, sizeof(req));
    assert(result == 0,
           "[%s] Failed to respond to master where is command (%d)",
           result);
    UNUSED(result);

    ctxt->master_courier = -1;
    ctxt->master_message = false;
}

static inline void
blaster_position_after_distance(const track_location start,
                                track_location* locs,
                                const int distance) {

    const int result = get_position_from(start, locs, distance);
    assert(result == 0, "Failed to scan ahead on the track (%d)", result);
    UNUSED(result);
}

static void blaster_restart_console(blaster* const ctxt,
                                    const int timestamp) {

    if (ctxt->console_courier >= 0 && ctxt->console_restarted) {

        const int distance = blaster_distance_remaining(&truth);
        const int velocity =
            physics_velocity(ctxt,
                             (truth.speed * 4) / 5,
                             velocity_type(truth.location.sensor));

        if (velocity == 0) {
            log("[%s] Tried to restart console when setting speed to 0",
                ctxt->name);
            return; // abort!
        }

        const int next_timestamp = timestamp + (distance / velocity);

        const int package[] = {
            truth.location.sensor,
            truth.next_location.sensor,
            next_timestamp
        };

        const int result = Reply(ctxt->console_courier,
                                 (char*)package,
                                 sizeof(package));
        assert(result == 0,
               "[%s] Failed to restart console!", ctxt->name);
        UNUSED(result);

        ctxt->console_timeout   = 0;
        ctxt->console_courier   = -1;
        ctxt->console_cancelled = false;
        ctxt->console_restarted = false;
    }
}

static void blaster_start_accelerate(blaster* const ctxt,
                                     const int to_speed,
                                     const int time) {

    if (ctxt->acceleration_courier != -1)
        log("[%s] Overriding previous accel. event, estimates will off",
            ctxt->name);

    // calculate when to wake up and send off that guy
    const int time_delta  = truth.speed ? time - truth.timestamp : 0;
    const int speed_delta = to_speed - truth.speed;
    const int start_dist  = physics_starting_distance(ctxt, speed_delta);
    const int start_time  = physics_starting_time(ctxt, start_dist);
    const int velocity    = ctxt->acceleration_courier == -1 ?
        physics_current_velocity(ctxt) :
        physics_velocity(ctxt,
                         (truth.speed + to_speed) >> 1,
                         velocity_type(truth.location.sensor));

    const track_location current_location = {
        .sensor = truth.location.sensor,
        .offset = truth.location.offset + (time_delta * velocity)
    };

    track_location locs[4];
    blaster_position_after_distance(current_location, locs, start_dist);

    // we need to walk the list and see what we have
    int i = 0;
    for (; i < 4; i++) {
        track_location* const l = &locs[i];

        // if we do not have the room, then give up
        if (l->sensor == AN_EXIT) {
            if (l->offset > 0) {
                log("[%s] Cannot accelerate into a barrier! Reversing!",
                    ctxt->name);
                // TODO: do we want to do this?
                if (ctxt->reverse_courier != -2)
                    blaster_reverse_step2(ctxt);
                else
                    log("[%s] Narrowly avoided race condition!", ctxt->name);
                ctxt->reverse_speed = to_speed;
                return;
            }
            else {
                l->sensor = reverse_sensor(l->sensor);
                // l->offset = 0; // I dunno....
            }
        }
        // but if we do have room, allow it?
        // maybe we should check stop distance as well...

        if (l->offset == 0) {// boom, exactly on target
            break;
        }
        else if (l->offset < 0) {
            // if still on first sensor, then just compensate
            if (l->sensor == current_location.sensor)
                l->offset = current_location.offset + start_dist;
            else // otherwise we need only go back one step
                i--;
            break;
        }
    }

    // this is a weird thing that happens at startup because we
    // have no state history
    if (i == -1) {
        i = 0;
        locs[i].sensor = current_location.sensor;
        locs[i].offset = current_location.offset + start_dist;
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
            .arg1 = locs[i].sensor,
            .arg2 = time + start_time,
            .arg3 = locs[i].offset
        }
    };

    assert(XBETWEEN(locs[i].sensor, -1, NUM_SENSORS),
            "ACCEL %d %d %d %d",
           locs[i].sensor, locs[i].offset, start_dist, truth.location.sensor);

    ctxt->acceleration_courier = blaster_create_new_delay_courier(ctxt);
    int result = Send(ctxt->acceleration_courier,
                      (char*)&msg, sizeof(msg),
                      NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to setup new delay courier (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);

    log("[%s] Acceleration courier %d doing %d -> %d",
        ctxt->name, ctxt->acceleration_courier, truth.speed, to_speed);

    last_accel                    = current_accel;
    current_accel.event           = EVENT_ACCELERATION;
    current_accel.timestamp       = time;
    current_accel.is_accelerating = true;
    current_accel.location        = current_location;
    current_accel.distance        = truth.distance;  // does not change
    current_accel.speed           = truth.speed + (speed_delta / 2); // fudge
    current_accel.direction       = truth.direction; // hmmm
    current_accel.next_location   = truth.next_location;
    current_accel.next_distance   = truth.next_distance;
    current_accel.next_timestamp  = time + start_time;
    current_accel.next_speed      = to_speed;

    blaster_debug_state(ctxt, &current_accel);

    // boom! new truth!
    truth = current_accel;

    ctxt->master_message = true;
    blaster_master_where_am_i(ctxt, time);

    ctxt->console_restarted = true;
    blaster_restart_console(ctxt, time);
}

static void blaster_start_deccelerate(blaster* const ctxt,
                                      const int to_speed,
                                      const int time) {

    if (ctxt->acceleration_courier != -1)
        log("[%s] Overriding previous accel. event, estimates will off",
            ctxt->name);

    // calculate when to wake up and send off that guy
    const int time_delta  = truth.speed ? time - truth.timestamp : 0;
    const int speed_delta = truth.speed - to_speed;
    const int stop_dist   = physics_stopping_distance(ctxt, speed_delta);
    const int stop_time   = physics_stopping_time(ctxt, stop_dist);
    const int velocity    = ctxt->acceleration_courier == -1 ?
        physics_current_velocity(ctxt) :
        physics_velocity(ctxt,
                         (truth.speed + to_speed) >> 2,
                         velocity_type(truth.location.sensor));


    assert(XBETWEEN(truth.location.sensor, -1, NUM_SENSORS),
            "Invalid sensor num %d", truth.location.sensor);
    const track_location current_location = {
        .sensor = truth.location.sensor,
        .offset = truth.location.offset + (time_delta * velocity)
    };

    track_location locs[4];
    blaster_position_after_distance(current_location, locs, stop_dist);

    // we need to walk the list and see what we have
    int i = 0;
    for (; i < 4; i++) {
        track_location* const l = &locs[i];

        if (l->sensor == AN_EXIT) {
            // so, our actual position is going to be the stopping
            // distance minus the remaining offset
            l->offset = stop_dist - l->offset;

            // we need to set the next_location.sensor to be the
            // sensor before the exit, which is almost impossible
            // to determine from here...
            // this is a hack that will probably not work very well...
            l->sensor = reverse_sensor(truth.location.sensor);
            break;
        }
        else if (l->offset == 0) { // boom, exactly on target
            break;
        }
        else if (l->offset < 0) {
            // if still on first sensor, then just compensate
            if (l->sensor == current_location.sensor)
                l->offset = current_location.offset + stop_dist;
            else // otherwise we need only go back one step
                i--;
            break;
        }
    }

    // so...i == -1 means we crash because we chose the wrong guy due to
    // some sort of teleportation, try to compensate
    if (i == -1) {
        // TODO: try and log out what conditions might have caused this
        i = 0;
        locs[0].sensor = current_location.sensor;
        locs[0].offset = current_location.offset + stop_dist;
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
            .arg1 = locs[i].sensor,
            .arg2 = time + stop_time,
            .arg3 = locs[i].offset
        }
    };

    assert(XBETWEEN(locs[i].sensor, -1, NUM_SENSORS),
           "ACCEL %d %d %d %d",
           locs[i].sensor, locs[i].offset, stop_dist, truth.location.sensor);

    ctxt->acceleration_courier = blaster_create_new_delay_courier(ctxt);
    const int result = Send(ctxt->acceleration_courier,
                            (char*)&msg, sizeof(msg),
                            NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to setup new delay courier (%d) to %d",
              ctxt->name, result, ctxt->acceleration_courier);

    log("[%s] Acceleration courier %d doing %d -> %d",
        ctxt->name, ctxt->acceleration_courier, truth.speed, to_speed);

    last_accel                      = current_accel;
    current_accel.event             = EVENT_ACCELERATION;
    current_accel.timestamp         = time;
    current_accel.is_accelerating   = true;
    current_accel.location          = current_location;
    current_accel.distance          = truth.next_distance; // does not change
    current_accel.speed             = truth.speed - (speed_delta / 2); // fudge
    current_accel.direction         = truth.direction;     // hmmm
    current_accel.next_location     = truth.next_location;
    current_accel.next_distance     = truth.next_distance;
    current_accel.next_timestamp    = time + stop_time;
    current_accel.next_speed        = to_speed;

    blaster_debug_state(ctxt, &current_accel);

    // boom! new truth!
    truth = current_accel;

    ctxt->master_message = true;
    blaster_master_where_am_i(ctxt, time);
}

static void blaster_set_speed(blaster* const ctxt,
                             const int speed,
                             const int time) {


    const int set_speed = speed ? (speed < 10 ? 0 : speed) : speed;

    if (speed && speed < 10) {
        log("[%s] Trying to set a non-zero speed less than 1 (%d)",
            ctxt->name, speed);
    }

    // if we are reversing, then store the speed such that the reverse
    // will choose that speed when it finishes
    if (ctxt->reversing) {
        ctxt->reverse_speed = set_speed;
        return;
    }

    put_train_speed((char)ctxt->train_gid, (char)(set_speed / 10));

    if (set_speed == truth.speed) {
        log("[%s] Speed already set to %d", ctxt->name, set_speed);
        return;
    }

    if (truth.speed > set_speed) {
        if (set_speed != 0)
            log("[%s] Unsupported decceleration %d -> %d",
                ctxt->name, current_accel.speed / 10, set_speed / 10);
        blaster_start_deccelerate(ctxt, set_speed, time);
    }
    else {
        if (truth.speed != 0)
            log("[%s] Unsupported acceleration %d -> %d",
                ctxt->name, current_accel.speed / 10, set_speed / 10);
        blaster_start_accelerate(ctxt, set_speed, time);
    }
}

static inline void
blaster_master_set_speed(blaster* const ctxt,
                         const blaster_req* const req,
                         const int time,
                         const int tid) {

    const int speed = req->arg1;

    blaster_set_speed(ctxt, speed, time);

    const int result = Reply(tid, NULL, 0);
    assert(result == 0,
           "[%s] Failed to return master command courier (%d)",
           ctxt->name, result);
    UNUSED(result);
}

static void blaster_reverse_direction(blaster* const ctxt,
                                      const int time) {

    // boom, change the lights
    put_train_speed((char)ctxt->train_gid, (char)TRAIN_REVERSE);
    truth.direction = -truth.direction;

    // need to flip around our next expected place
    truth.event           = EVENT_ACCELERATION;
    truth.location.sensor = reverse_sensor(truth.next_location.sensor);
    truth.location.offset = truth.next_distance - truth.location.offset +
        ctxt->measurements.pickup; // don't forget about the pickup...

    // the next sensor _would_ be the reverse of the current next
    // in most cases, but that is not true if we just went over a
    // merge (now a branch) which is pointed in the opposite direction
    // from which we came, so we need to ask mission control wazzup

    assert(truth.location.sensor < NUM_SENSORS, "HERE");
    const int result = get_sensor_from(truth.location.sensor,
                                       &truth.next_distance,
                                       &truth.next_location.sensor);
    assert(result == 0, "[%s] Fuuuuu", ctxt->name);
    UNUSED(result);

    // this probably does not matter, if we hit an acceleration event next
    // then it will be updated correctly...and we have to hit an acceleration
    // event next...
    truth.next_location.offset = 0; // TODO: is this right?
    // truth.next_timestamp    = I don't fucking know; // I don't need to know

    // https://www.youtube.com/watch?v=gAYL5H46QnQ
    assert(ctxt->console_cancel >= 0,
           "[%s] Console cancel courier not ready to rock!",
           ctxt->name);

    const blaster_req_type cancel = BLASTER_CONSOLE_CANCEL;
    const int cresult = Reply(ctxt->console_cancel,
                              (char*)&cancel, sizeof(blaster_req_type));
    UNUSED(cresult);
    assert(cresult == 0,
           "[%s] Failed to send message to console cancel courier (%d)",
           ctxt->name, cresult);
    ctxt->console_cancel    = -1;
    ctxt->console_cancelled = true;

    ctxt->master_message = true;
    blaster_master_where_am_i(ctxt, time);
}

static void blaster_master_reverse(blaster* const ctxt,
                                   const int time,
                                   const int tid) {
    blaster_reverse_step1(ctxt, time);

    const int result = Reply(tid, NULL, 0);
    assert(result == 0,
           "[%s] Failed to return master courier (%d)",
           ctxt->name, result);
    UNUSED(result);
}

static void blaster_reverse_step1(blaster* const ctxt, const int time) {

    if (ctxt->reverse_courier != -1) {
        log("[%s] Already reversing!", ctxt->name);
        return;
    }

    if (truth.speed == 0) {
        blaster_reverse_direction(ctxt, time);
        return;
    }

    if (truth.is_accelerating)
        ctxt->reverse_speed = current_accel.next_speed;
    else
        ctxt->reverse_speed = truth.speed;

    blaster_set_speed(ctxt, 0, time); // STAHP!
    ctxt->reversing = 1;
    ctxt->reverse_courier = -2;
}

static void blaster_reverse_step2(blaster* const ctxt) {

    assert(ctxt->reverse_courier == -2,
           "[%s] Reversing race condition (%d)",
           ctxt->name, ctxt->reverse_courier);

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

static void blaster_reverse_step3(blaster* const ctxt,
                                  const int tid,
                                  const int time) {

    // if the reverse was overridden
    if (tid != ctxt->reverse_courier) {
        log("[%s] Someone overrode a reverse command!", ctxt->name);

        const int result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("[%s] Failed to kill old delay reverse courier (%d)",
                  ctxt->name, result);
        return;
    }

    blaster_reverse_direction(ctxt, time);

    struct {
        tnotify_header head;
        blaster_req     req;
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
                                  const int tid,
                                  const int time) {

     // if the reverse was overridden
    if (tid != ctxt->reverse_courier) {
        log("[%s] Someone overrode a reverse command!", ctxt->name);

        const int result = Reply(tid, NULL, 0);
        if (result < 0)
            ABORT("[%s] Failed to kill old delay reverse courier %d (%d)",
                  ctxt->name, tid, result);
        return;
    }

    ctxt->reversing = 0;
    blaster_set_speed(ctxt, ctxt->reverse_speed, time);

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
    UNUSED(time);

    if (truth.direction != DIRECTION_UNKNOWN) {
        // TODO: proper lost logic
        // log("[%s] Halp! I'm lost at %c%d", ctxt->name, s.bank, s.num);
        return;
    }

    if (s.bank == 'B') {
        truth.direction = DIRECTION_FORWARD;
        // blaster_reverse_step1(ctxt, time);
    } else {
        truth.direction = DIRECTION_BACKWARD;
    }
}

static bool blaster_kill_courier(const int expected_tid,
                                 const int courier_tid) {

    // no matter what, we want to kill the delay courier
    const int result = Reply(courier_tid, NULL, 0);
    UNUSED(result);
    assert(result == 0,
           "Failed to kill delay courier %d (%d)",
           courier_tid, result);

    return (expected_tid == courier_tid);
}

static inline void
blaster_process_acceleration_event(blaster* const ctxt,
                                   blaster_req* const req,
                                   const int courier_tid,
                                   const int timestamp) {

    // do not process the event if it has been overridden by a future event
    if (!blaster_kill_courier(ctxt->acceleration_courier, courier_tid)) {
        log("[%s] Killing overridden acceleration courier (%d %d)",
            ctxt->name, ctxt->acceleration_courier, courier_tid);
        return;
    }

    // only unset this if we are accepting the acceleration courier
    ctxt->acceleration_courier = -1;

    const int velocity =
        physics_velocity(ctxt,
                         current_accel.next_speed,
                         velocity_type(truth.location.sensor));
    const int delta_t = timestamp - req->arg2;
    const int delta_d = velocity * delta_t;

    last_accel                     = current_accel;
    current_accel.event            = EVENT_ACCELERATION;
    current_accel.timestamp        = timestamp;
    current_accel.is_accelerating  = false;
    current_accel.location.sensor  = req->arg1;
    current_accel.location.offset  = req->arg3 + delta_d;
    current_accel.speed            = last_accel.next_speed;
    current_accel.direction        = last_accel.direction;
    current_accel.next_location    = truth.next_location;
    current_accel.next_distance    = truth.next_distance;
    current_accel.next_timestamp   = truth.next_timestamp;

    // now the hard part: merging acceleration state into the truth...

    // at the very least, this subset of state has to become truth
    truth.event           = current_accel.event;
    truth.is_accelerating = current_accel.is_accelerating; // false!
    truth.speed           = current_accel.speed;
    // this can screw up during boot or if we are lost
    // truth.direction       = current_accel.direction;
    truth.next_speed      = current_accel.next_speed;


    // now, the question is, what other state do we need to merge?

    // late, but unless our calibration is way off, we won't be very late
    if (last_sensor.location.sensor == current_accel.location.sensor) {
        mark_log("[%s] Late acceleration event!", ctxt->name);

        // choose a pseudo-magic value for speed to use in the calculation
        const int    late_delta_t = timestamp - truth.timestamp;
        const int        speed_bs = (truth.speed * 8) / 10;
        const int estimated_speed = MAX(20, speed_bs);
        const int   late_velocity =
            physics_velocity(ctxt,
                             estimated_speed,
                             velocity_type(truth.location.sensor));

        // so just try and add a bit of fudge
        truth.location.offset += late_velocity * late_delta_t;
    }
    // we are (somewhat) on time
    else if (current_sensor.location.sensor == current_accel.location.sensor) {
        // assume our estimates are correct
        truth.location = current_accel.location;
    }
    // we are early (OR dead sensor)
    else if (current_sensor.next_location.sensor ==
             current_accel.location.sensor) {

        // fudge it (in a way that reservations like)
        truth.location.offset = truth.next_distance;

        // but that means the values for next_* are not correct
        const int result = get_sensor_from(truth.location.sensor,
                                           &truth.next_distance,
                                           &truth.next_location.sensor);
        assert(result == 0, "[%s] Fuuuuu (%d)", ctxt->name, result);
        UNUSED(result);

        truth.next_location.offset  = 0;
        current_accel.next_location = truth.next_location;
        current_accel.next_distance = truth.next_distance;
    }
    // I don't fuckin' know what to do in this case
    else {
        log("[%s] Teleported since started acceleration!",
            ctxt->name);

        // choose a pseudo-magic value for speed to use in the calculation
        const int    late_delta_t = timestamp - truth.timestamp;
        const int        speed_bs = (truth.speed * 8) / 10;
        const int estimated_speed = MAX(20, speed_bs);
        const int   late_velocity =
            physics_velocity(ctxt,
                             estimated_speed,
                             velocity_type(truth.location.sensor));

        // so just try and add a bit of fudge
        truth.location.offset += late_velocity * late_delta_t;

        // TODO: this will happen at startup because we will often
        // start from the wrong position on the track

        // TODO: what other cases will cause this?
    }

    // now, we need to adjust our estimates a bit
    truth.timestamp      = current_accel.timestamp;
    truth.next_timestamp = velocity ?
        timestamp + (blaster_distance_remaining(&truth) / velocity) :
        timestamp;

    // god damnit
    assert(truth.location.sensor < TRACK_MAX,
           "[%s] Told ya so bro %d (%d -> %d) [%d, %d, %d]",
           ctxt->name,
           truth.location.sensor,
           last_accel.speed, last_accel.next_speed,
           req->arg1,
           last_accel.location.sensor,
           last_sensor.location.sensor);

    // let the master know where we are
    ctxt->master_message = true;
    blaster_master_where_am_i(ctxt, truth.timestamp);

    if (ctxt->reversing == 1)
        blaster_reverse_step2(ctxt);
}

static inline void
blaster_process_console_timeout(blaster* const ctxt,
                                const int tid,
                                const int time) {
    int result; UNUSED(result);
    log("[%s] timeout %d", ctxt->name, tid);

    if (0 == truth.speed) {
        // train stopped, keep the courier
        // for when we are going
        log("[%s] blocking console %d", ctxt->name, tid);
        ctxt->console_courier = tid;

    } else if (0 == truth.next_speed) {
        if (truth.location.sensor == truth.next_location.sensor) {
            log("[%s] blocking console slowdown %d", ctxt->name, tid);
            ctxt->console_courier = tid;
        } else {
            const int current_s = truth.location.sensor;
            const int next_s    = truth.next_location.sensor;
            const int timeout   = time + blaster_distance_remaining(&truth) / 1000;

            blaster_debug_state(ctxt, &truth);

            const int notify_pack[] = {current_s, next_s, timeout};
            result = Reply(tid, (char*)notify_pack, sizeof(notify_pack));
            assert(0 == result, "failed to courier %d", result);
        }

    } else if (ctxt->console_timeout) {
        int lost = 80;
        result = Reply(tid, (char*)&lost, sizeof(lost));
        assert(0 == result, "failed to courier %d", result);

    } else {
        //TODO: need to get the next node since the sensor may now
        // be dead
        int lost = 80;
        result = Reply(tid, (char*)&lost, sizeof(lost));
        assert(0 == result, "failed to courier %d", result);
        // lost code is place holder until something real can be put in
    }

}

static inline void
blaster_process_sensor_event(blaster* const ctxt,
                             blaster_req* const req,
                             const int courier_tid,
                             const int timestamp) {

    const int  sensor_hit = req->arg1;
    const int sensor_time = req->arg2;

    assert(XBETWEEN(sensor_hit, -1, NUM_SENSORS),
           "processed bad sensor %d", sensor_hit);

    // just need to update state here and move on to next stuffs
    // this is essentially a state reset, we cannot really use previous state

    last_sensor = current_sensor;
    current_sensor.event                = EVENT_SENSOR;
    current_sensor.timestamp            = sensor_time;
    current_sensor.is_accelerating      = truth.is_accelerating;
    current_sensor.location.sensor      = sensor_hit;
    current_sensor.location.offset      = 0;
    current_sensor.distance             = last_sensor.next_distance;
    current_sensor.speed                = truth.speed;
    current_sensor.direction            = truth.direction;
    current_sensor.next_location.offset = 0;
    current_sensor.next_speed           = truth.next_speed;

    assert(current_sensor.location.sensor < NUM_SENSORS,
           "HERE %d should be less than %d (BLAME NIK)",
           current_sensor.location.sensor, NUM_SENSORS);

    const int result = get_sensor_from(current_sensor.location.sensor,
                                       &current_sensor.next_distance,
                                       &current_sensor.next_location.sensor);
    assert(result == 0, "[%s] Fuuuuu %d", ctxt->name, result);
    UNUSED(result);

    // calculate expected next time
    const int v =
        physics_velocity(ctxt,
                         current_sensor.speed,
                         velocity_type(current_sensor.location.sensor));

    current_sensor.next_timestamp = v ?
        current_sensor.timestamp + (current_sensor.next_distance / v) :
        truth.timestamp;

    // now we need to update the truth...except that a sensor hit is always the
    // considered to be the truth...
    // TODO: we may not want to accept this feedback if it is _TOO_ early
    truth = current_sensor;

    blaster_debug_state(ctxt, &truth);

    const int    delta_t = sensor_time - last_sensor.next_timestamp;
    const int expected_v =
        physics_velocity(ctxt,
                         truth.speed,
                         velocity_type(last_sensor.location.sensor));
    const int   actual_v =
        current_sensor.distance / (sensor_time - last_sensor.timestamp);
    const int    delta_v = actual_v - expected_v;

    // only do feedback if we are in steady state
    if (!(last_sensor.is_accelerating || current_sensor.is_accelerating))
        physics_feedback(ctxt, actual_v, expected_v, delta_v);

    // has to be going too fast before we hit the emergency brakes
    // because otherwise it might have been intentional
    if (truth.next_distance < 0) {
        if (physics_current_stopping_distance(ctxt) >= -truth.next_distance) {
            log("[%s] Barreling towards an exit! Hitting emergency brakes!",
                ctxt->name);
            blaster_reverse_step1(ctxt, timestamp);
        }
        else {
            // hacks
            current_sensor.next_location.sensor =
                current_sensor.location.sensor;
        }
    }

    ctxt->master_message = true;

    blaster_master_where_am_i(ctxt, timestamp);
    physics_update_feedback_ui(ctxt, delta_v, delta_t);

    int reply[3];
    create_console_reply(ctxt, sensor_time, reply);
    const int reply_result = Reply(courier_tid, (char*)&reply, sizeof(reply));
    assert(reply_result == 0,
           "[%s] failed to get next sensor (%d)",
           ctxt->name, reply_result);
    UNUSED(reply_result);

    // TODO: send off the estimate guy
    //blaster_wait_for_next_estimate(ctxt);
}

static inline void
blaster_process_unexpected_sensor_event(blaster* const ctxt,
                                        blaster_req* const req,
                                        const int courier_tid,
                                        const int timestamp) {

    const int      sensor_hit = req->arg1;
    const int sensor_hit_time = req->arg2;

    // maybe it wasn't unexpected, just late
    if (current_sensor.next_location.sensor == sensor_hit) {
        blaster_process_sensor_event(ctxt, req, courier_tid, timestamp);
        return;
    }

    assert(XBETWEEN(sensor_hit, -1, NUM_SENSORS),
           "processed bad unexpected sensor %d", sensor_hit);

    // just need to update state here and move on to next stuffs
    // this is essentially a state reset, we cannot really use previous state

    last_sensor = current_sensor;
    current_sensor.event                = EVENT_UNEXPECTED_SENSOR;
    current_sensor.timestamp            = sensor_hit_time;
    current_sensor.is_accelerating      = truth.is_accelerating;
    current_sensor.location.sensor      = sensor_hit;
    current_sensor.location.offset      = 0;
    current_sensor.distance             = 0; // we do not know this (don't care)
    current_sensor.speed                = truth.speed;
    current_sensor.direction            = truth.direction;
    current_sensor.next_location.offset = 0;
    current_sensor.next_speed           = truth.next_speed;

    assert(current_sensor.location.sensor < NUM_SENSORS, "HERE");
    const int result = get_sensor_from(current_sensor.location.sensor,
                                       &current_sensor.next_distance,
                                       &current_sensor.next_location.sensor);
    assert(result == 0, "[%s] Fuuuuu", ctxt->name);
    UNUSED(result);

    // calculate expected next time
    const int v = physics_velocity(ctxt,
                                   current_sensor.speed,
                                   velocity_type(current_sensor.location.sensor));

    current_sensor.next_timestamp = v ?
        current_sensor.timestamp + (current_sensor.next_distance / v) :
        truth.timestamp;

    if (truth.next_distance < 0) {
        if (physics_current_stopping_distance(ctxt) >= -truth.next_distance) {
            log("[%s] Barreling towards an exit! Hitting emergency brakes!",
                ctxt->name);
            blaster_reverse_step1(ctxt, timestamp);

        // TODO:
        // in any case, we need to flip around our next expected sensor
        // and our next expected; and this happens for expected sensors as well!
        }
        else {
            current_sensor.next_location.sensor = current_sensor.location.sensor;
        }
    }

    // now we need to update the truth...except that a sensor hit is always the
    // considered to be the truth...
    truth = current_sensor;

    // need to pass the service_time in case we do a reverse
    blaster_detect_train_direction(ctxt, sensor_hit, timestamp);

    ctxt->master_message = true;
    blaster_master_where_am_i(ctxt, timestamp);

    int reply[3];
    create_console_reply(ctxt, sensor_hit_time, reply);

    const int reply_result = Reply(courier_tid, (char*)&reply, sizeof(reply));
    assert(reply_result == 0,
           "failed to wait for next sensor (%d)",
           reply_result);
    UNUSED(reply_result);

    // TODO: send off the estimate guy
    //blaster_wait_for_next_estimate(ctxt);
}

static inline void blaster_wait(blaster* const ctxt,
                                const control_req* const callin) {

    blaster_req req;
    const int control = myParentTid();

    FOREVER {
        const int result = Send(control,
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
        case BLASTER_REVERSE:
            put_train_speed(ctxt->train_gid, TRAIN_REVERSE);
            break;
        case BLASTER_UPDATE_TWEAK:
            blaster_update_tweak(ctxt, (train_tweakable)req.arg1, req.arg2);
            break;
            // We should not get any of these as the first event
            // so we abort if we get them
        case BLASTER_MASTER_CHANGE_SPEED:
        case BLASTER_MASTER_REVERSE:
        case BLASTER_REVERSE2:
        case BLASTER_REVERSE3:
        case BLASTER_REVERSE4:
        case BLASTER_DUMP_VELOCITY_TABLE:
        case BLASTER_ACCELERATION_COMPLETE:
        case BLASTER_NEXT_NODE_ESTIMATE:
        case BLASTER_CONSOLE_TIMEOUT:
        case BLASTER_CONSOLE_CANCEL:
        case BLASTER_SENSOR_FEEDBACK:
        case BLASTER_UNEXPECTED_SENSOR_FEEDBACK:
        case BLASTER_MASTER_WHERE_ARE_YOU:
        case BLASTER_MASTER_CONTEXT:
        case BLASTER_REQ_TYPE_COUNT:
        case BLASTER_CONSOLE_LOST:
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

    // stop any running trains
    put_train_speed((char)ctxt->train_gid, 0);

    //I Want this to explicity never be changeable from here
    *(track_node**)&ctxt->track = (track_node*)init[1];

    result = Reply(tid, NULL, 0);
    if (result < 0)
        ABORT("[Blaster] Failed to initialize (%d)", result);

    // Setup the train name
    sprintf(ctxt->name, "BLAST%d", ctxt->train_gid);
    ctxt->name[7] = '\0';
    if (RegisterAs(ctxt->name))
        ABORT("[Blaster] Failed to register train (%d)", ctxt->train_gid);

    char colour[32];
    char* ptr = sprintf(colour,
                        ESC_CODE "%s" COLOUR_SUFFIX,
                        train_to_colour(ctxt->train_gid));
    sprintf_char(ptr, '\0');

    ptr = sprintf(ctxt->name,
                  "%sBlaster %d" COLOUR_RESET,
                  colour, ctxt->train_gid);
    sprintf_char(ptr, '\0');


    char buffer[32];
    ptr = vt_goto(buffer, TRAIN_ROW, TRAIN_NUMBER_COL(ctxt->train_id));
    ptr = sprintf(ptr, "%s%d" COLOUR_RESET, colour, ctxt->train_gid);
    Puts(buffer, ptr - buffer);

    ctxt->master_courier     = -1;
    ctxt->reverse_courier    = -1;
    ctxt->checkpoint_courier = -1;
    ctxt->console_cancel     = -1;
    ctxt->console_courier    = -1;
}

static TEXT_COLD void blaster_init_couriers(blaster* const ctxt,
                                            const courier_package* const package) {

    UNUSED(ctxt);

    int result;

    // Setup the sensor courier */
    int tid = Create(TRAIN_CONSOLE_PRIORITY, train_console);
    if (tid < 0) ABORT("[%s] Failed creating train console (%d)",
                       ctxt->name, tid);

    int c_init[] = {ctxt->train_id, (int)ctxt->track};
    result       = Send(tid, (char*)c_init, sizeof(c_init), NULL, 0);
    assert(result == 0, "Failed to setup train console! (%d)", result);

    const int cancel = BLASTER_CONSOLE_CANCEL;
    courier_package console_pack = {
        .receiver = tid,
        .size     = -(int)sizeof(cancel),
        .message  = (char*)&cancel
    };

    tid = Create(TRAIN_COURIER_PRIORITY, courier);
    if (tid < 0) ABORT("[%s] Failed creating train console cancel (%d)",
                       ctxt->name, tid);
    result = Send(tid,
                  (char*)&console_pack, sizeof(console_pack),
                  NULL, 0);
    assert(result == 0,
           "[%s] Error sending package to cancel console courier %d",
           ctxt->name, result);


    tid = Create(TRAIN_COURIER_PRIORITY, courier);
    if (tid < 0) ABORT("[%s] Error setting up command courier (%d)",
                       ctxt->name, tid);


    const int control_courier = Create(TRAIN_COURIER_PRIORITY, courier);
    assert(control_courier >= 0,
           "[%s] Error setting upcommand courier (%d)",
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

    log("[%s] Initialized", context.name);

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
        case BLASTER_MASTER_CHANGE_SPEED:
            blaster_master_set_speed(&context, &req, time, tid);
            continue;

        case BLASTER_REVERSE:
            blaster_reverse_step1(&context, time);
            break;
        case BLASTER_MASTER_REVERSE:
            blaster_master_reverse(&context, time, tid);
            continue;

        case BLASTER_REVERSE2:
            ABORT("[%s] Should never get reverse 2 (%d)", context.name, tid);
        case BLASTER_REVERSE3:
            blaster_reverse_step3(&context, tid, time);
            continue;
        case BLASTER_REVERSE4:
            blaster_reverse_step4(&context, tid, time);
            continue;

        case BLASTER_MASTER_WHERE_ARE_YOU:
            context.master_courier = tid;
            if (context.master_message)
                blaster_master_where_am_i(&context, time);
            continue;

        case BLASTER_MASTER_CONTEXT: {
            struct blaster_context* const ctxt = &context;
            result = Reply(tid, (char*)&ctxt, sizeof(ctxt));
            assert(result == 0,
                   "[%s] Failed to give context pointer to master (%d)",
                   context.name, result);
            continue;
        }

        case BLASTER_DUMP_VELOCITY_TABLE:
            blaster_dump_velocity_table(&context);
            break;
        case BLASTER_UPDATE_TWEAK:
            blaster_update_tweak(&context, (train_tweakable)req.arg1, req.arg2);
            break;

        case BLASTER_ACCELERATION_COMPLETE:
            blaster_process_acceleration_event(&context, &req, tid, time);
            continue;
        case BLASTER_NEXT_NODE_ESTIMATE:
            // blaster_process_checkpoint_event(&context, &req, tid, time);
            continue;
        case BLASTER_SENSOR_FEEDBACK:
            blaster_process_sensor_event(&context, &req, tid, time);
            continue;
        case BLASTER_UNEXPECTED_SENSOR_FEEDBACK:
            blaster_process_unexpected_sensor_event(&context, &req, tid, time);
            continue;

        case BLASTER_CONSOLE_TIMEOUT:
            blaster_process_console_timeout(&context, tid, time);
            continue;

        case BLASTER_CONSOLE_LOST:
            if (!context.console_cancelled)
                blaster_set_speed(&context, 0, time);
            log("[%s] console has be rejected, stop", context.name);
            context.console_courier = tid;
            blaster_restart_console(&context, time);
            continue;

        case BLASTER_CONSOLE_CANCEL:
            context.console_cancel = tid;
            continue;

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
