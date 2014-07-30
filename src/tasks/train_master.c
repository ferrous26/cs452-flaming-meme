#include <std.h>
#include <debug.h>
#include <track_data.h>

#include <tasks/priority.h>
#include <tasks/courier.h>
#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>
#include <tasks/track_reservation.h>

#include <tasks/path_admin.h>
#include <tasks/path_worker.h>

#include <tasks/train_control.h>
#include <tasks/train_blaster.h>
#include <tasks/train_blaster_types.h>
#include <tasks/train_blaster_physics.h>

#include <tasks/train_master.h>

// we want to clear a turnout by at least this much in order to compensate
// for position error
#define TURNOUT_CLEARANCE_OFFSET_DEFAULT  50000 // um

// we want to flip a turnout when we are
#define TURNOUT_DISTANCE 200000

#define CHECK_WHOIS(tid, msg) \
if (tid < 0) ABORT("[%s]\t"msg" (%d)", ctxt->name, tid)

typedef struct action_point {
    const path_node* step;
    int offset;
    const path_node* action; // path node with the desired turnout state
} action_point;

typedef struct master_context {
    // identifying variables
    int  train_id;
    int  train_gid;
    char name[32];

    // the tids that are used by this
    int my_tid;              // tid of self
    int blaster;             // tid of control task
    int control;             // tid of control task
    int path_admin;          // tid of the path admin
    int path_worker;         // tid of the path worker
    int mission_control_tid; // tid of mission control

    int          turnout_padding;
    train_state  checkpoint;

    int          active;
    int          destination;        // destination sensor
    int          destination_offset; // in centimeters
    int          sensor_to_stop_at;  // special case for handling ss command
    sensor_block sensor_block;

    int              short_moving_distance;
    int              short_moving_timestamp;

    const path_node* path_step;      // the current step we are on
    const path_node* path;           // start of the path array (i.e. the last step)
    int              allowed_sensor;
    bool             path_stopping;  // state marker indicating we are stopping
    bool             path_completed; // true when stop command has been thrown during path

    action_point   next_turnout; // data about the next turnout
    action_point   next_stop;    // data about the next stop

    struct blaster_context* const blaster_ctxt;

    int reserved[TRACK_MAX];

    int last_reservation_speed;
    const track_node* const track;
} master;

static int master_current_velocity(master* const ctxt) {
    return physics_velocity(ctxt->blaster_ctxt,
                            ctxt->checkpoint.speed,
                            velocity_type(ctxt->checkpoint.location.sensor));
}

static int master_current_stopping_distance(master* const ctxt) {
    return physics_stopping_distance(ctxt->blaster_ctxt,
                                     ctxt->checkpoint.speed);
}

static inline int master_head_distance(master* const ctxt) {
    return  (ctxt->checkpoint.direction == DIRECTION_FORWARD ?
             ctxt->blaster_ctxt->measurements.front :
             ctxt->blaster_ctxt->measurements.back);
}

static inline int master_tail_distance(master* const ctxt) {
    return ctxt->blaster_ctxt->measurements.pickup +
        (ctxt->checkpoint.direction == DIRECTION_FORWARD ?
         ctxt->blaster_ctxt->measurements.back :
         ctxt->blaster_ctxt->measurements.front);
}

static void master_release_control_courier(master* const ctxt,
                                           const control_req* const pkg,
                                           const int tid) {

    const int result = Reply(tid, (char*)pkg, sizeof(control_req));
    assert(result == 0,
           "[%s] Failed to send courier back to Lenin (%d)",
           ctxt->name, result);
    UNUSED(result);
    UNUSED(ctxt);
}

static void master_delay_flip_turnout(master* const ctxt,
                                      const int turn,
                                      const int direction,
                                      const int action_time) {

    const int c = Create(TRAIN_COURIER_PRIORITY, delayed_one_way_courier);
    assert(c >= 0, "failed to make one way courier %d", c);

    struct {
        tdelay_header head;
        struct {
            mc_type type;
            turnout turn;
        } body;
    } msg = {
        .head = {
            .receiver = ctxt->mission_control_tid,
            .type     = DELAY_ABSOLUTE,
            .ticks    = action_time
        },
        .body = {
            .type = MC_U_TURNOUT,
            .turn = {
                .num   = turn,
                .state = direction
            }
        }
   };

    const int result = Send(c, (char*)&msg, sizeof(msg), NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to send turnout delay (%d)", ctxt->name, result);
}

static inline void
master_send_blaster(master* const ctxt,
                    const void* const msg,
                    const int size) {

    const int tid = Create(TRAIN_COURIER_PRIORITY, delayed_one_way_courier);
    assert(tid >= 0,
           "[%s] Failed to create one time courier (%d)",
           ctxt->name, tid);

    const int result = Send(tid, (char*)msg, size, NULL, 0);
    assert(result == 0,
           "[%s] Failed to setup one time courier (%d)",
           ctxt->name, result);

    UNUSED(result);
    UNUSED(ctxt);
}

static void
master_set_speed(master* const ctxt, const int speed, const int delay) {

    struct {
        tdelay_header head;
        blaster_req   req;
    } message = {
        .head = {
            .type     = DELAY_ABSOLUTE,
            .ticks    = delay,
            .receiver = ctxt->blaster
        },
        .req  = {
            .type = BLASTER_MASTER_CHANGE_SPEED,
            .arg1 = speed
        }
    };

    master_send_blaster(ctxt, &message, sizeof(message));
}

static void
master_set_reverse(master* const ctxt, const int delay) {

    struct {
        tdelay_header head;
        blaster_req   req;
    } message = {
        .head = {
            .type     = DELAY_ABSOLUTE,
            .ticks    = delay,
            .receiver = ctxt->blaster
        },
        .req  = {
            .type = BLASTER_MASTER_REVERSE
        }
    };

    master_send_blaster(ctxt, &message, sizeof(message));
}

static void
master_stop_at_sensor(master* const ctxt,
                      const control_req* const pkg,
                      const int sensor_idx,
                      const int tid) {

    const sensor s = pos_to_sensor(sensor_idx);
    log("[%s] Gonna hit the brakes when we hit sensor %c%d",
        ctxt->name, s.bank, s.num);
    ctxt->sensor_to_stop_at = sensor_idx;

    master_release_control_courier(ctxt, pkg, tid);
}

static void
master_block_until_sensor(master* const ctxt,
                          const control_req* const pkg,
                          const master_req* const req,
                          const int tid) {

    ctxt->sensor_block.tid    = req->arg1;
    ctxt->sensor_block.sensor = req->arg2;

    const sensor s = pos_to_sensor(req->arg2);
    log("[%s] Blocking %d until we hit sensor %c%d",
        ctxt->name, req->arg2, s.bank, s.num);

    master_release_control_courier(ctxt, pkg, tid);
}

static void master_update_tweak(master* const ctxt,
                                const control_req* const pkg,
                                const master_req* const req,
                                const int request_tid) {

    const train_tweakable tweak = (train_tweakable)req->arg1;
    const int value = req->arg2;

    switch (tweak) {
    case TWEAK_TURNOUT_CLEARANCE:
        log("[%s] Updating turnout clearance offset to %d mm (was %d mm)",
            ctxt->name, value / 1000, ctxt->turnout_padding / 1000);
        ctxt->turnout_padding = value;
        break;

    case TWEAK_FEEDBACK_THRESHOLD:
    case TWEAK_FEEDBACK_RATIO:
    case TWEAK_STOP_OFFSET:
    case TWEAK_REVERSE_STOP_TIME_PADDING:
    case TWEAK_ACCELERATION_TIME_FUDGE:
    case TWEAK_COUNT:
        log("[%s] I do not have a tweakable for id %d", ctxt->name, tweak);
        break;
    }

    master_release_control_courier(ctxt, pkg, request_tid);
}

static track_location
master_current_location(master* const ctxt, const int time) {

    const int     time_delta = time - ctxt->checkpoint.timestamp;
    const int distance_delta =
        ctxt->checkpoint.location.offset +
        (master_current_velocity(ctxt) * time_delta);

    // offset is not allowed to grow beyond reasonable limits
    // this somewhat compensates for trains stalling on the track
    const int new_offset = MIN(distance_delta,
                               ctxt->checkpoint.next_distance);

    const track_location l = {
        .sensor = ctxt->checkpoint.location.sensor,
        .offset = new_offset
    };

    return l;
}

static void
master_where_you_at(master* const ctxt,
                    const control_req* const pkg,
                    const int request_tid,
                    const int courier_tid) {

    const track_location l = master_current_location(ctxt, Time());

    const int result = Reply(request_tid, (char*)&l, sizeof(l));
    assert(result == 0,
           "[%s] Failed to respond to whereis command (%d)",
           result);
    UNUSED(result);

    master_release_control_courier(ctxt, pkg, courier_tid);
}

static action_point
master_find_action_location(master* const ctxt,
                            const path_node* const start_step,
                            const int offset) {

    const path_node* step = start_step;
    for (; step <= ctxt->path_step; step++) {
        // now, we want to walk back through the path...
        if (step->type == PATH_SENSOR) {

            // if the distance from the start point
            const int dist = start_step->dist - step->dist;
            if (dist > offset) {
                const action_point l = {
                    .step   = step,
                    .offset = abs(dist - offset),
                    .action = start_step
                };
                return l;
            }
        }
    }

    const action_point l = {
        .step   = NULL,
        .action = start_step
    };

    return l;
}

static inline const path_node*
master_try_fast_forward(master* const ctxt) {

#ifdef DEBUG
    const path_node* start = ctxt->path_step;
#endif
    for (const path_node* step = ctxt->path_step; step >= ctxt->path; step--) {
        if (step->type == PATH_SENSOR) {

#ifdef DEBUG
            if (!start) start = step;
#endif
            if (step->data.sensor == ctxt->checkpoint.location.sensor) {
#ifdef DEBUG
                if (start != step) {
                    const sensor head =
                        pos_to_sensor(start->data.sensor);
                    const sensor tail =
                        pos_to_sensor(step->data.sensor);

                    log("[%s] %c%d >> %c%d",
                        ctxt->name, head.bank, head.num, tail.bank, tail.num);
                }
#endif

                return step; // successfully fast forwarded
            }
        }
    }

    // cancel path, we need something else!
    return NULL;
}

static void __attribute__ ((unused))
master_debug_path(master* const ctxt) {

    for (const path_node* step = ctxt->path_step; step >= ctxt->path; step--) {
        const int i = step - ctxt->path;
        switch(step->type) {
        case PATH_SENSOR: {
            const sensor print = pos_to_sensor(step->data.int_value);
            log("%d  \tS\t%c%d\t%d", i, print.bank, print.num, step->dist);
            break;
        }
        case PATH_TURNOUT:
            log("%d  \tT\t%d %c\t%d",
                i, step->data.turnout.num, step->data.turnout.state,
                step->dist);
            break;
        case PATH_REVERSE:
            log("%d  \tR\t\t%d", i, step->dist);
            break;
        }
    }
}

static void
master_goto(master* const ctxt, const int destination, const int offset) {

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

    // Do not allow path start reverse if train is in motion
    const int opts = ctxt->train_id |
        (ctxt->checkpoint.speed ? PATH_START_REVERSE_OFF_MASK : 0);

    pa_request request = {
        .type = PA_GET_PATH,
        .req  = {
            .requestor   = ctxt->my_tid,
            .header      = MASTER_PATH_DATA,
            .sensor_to   = destination,
            .opts        = opts,
            .sensor_from = ctxt->checkpoint.location.sensor, // hmmm
            .reserve     = 600,
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
    UNUSED(result);
}

static inline void
master_goto_command(master* const ctxt,
                    const master_req* const req,
                    const control_req* const pkg,
                    const int tid) {

    const int destination = req->arg1;
    const int offset      = req->arg2;

    if (ctxt->destination == destination) {
        ctxt->path_step = master_try_fast_forward(ctxt);
        if (ctxt->path_step) return;

        // TODO: handle the offset
    }

    master_goto(ctxt, destination, offset);
    master_release_control_courier(ctxt, pkg, tid);
}

static inline void master_calculate_turnout_point(master* const ctxt,
                                                  const int time) {

    // this handles the case where we need to fast forward the route
    const path_node* const start_step =
        MIN(ctxt->next_turnout.action, ctxt->path_step);

    for (const path_node* step = start_step - 1; step >= ctxt->path; step--) {

        if (step->type == PATH_TURNOUT) {

            const int direction_offset =
                TURNOUT_DISTANCE + master_head_distance(ctxt);

            // find out at which step we need to act...
            ctxt->next_turnout =
                master_find_action_location(ctxt, step, direction_offset);

            // if we do not have enough distance
            if (ctxt->next_turnout.step == NULL) {
                // TODO: should actually check the offset from the sensor
                //       because the train might be sitting on the turnout
                //       and flipping it willl cause multi-track drifting
                if (ctxt->checkpoint.speed == 0) {
                    log("[%s] Risky turnout flipping!", ctxt->name);
                    master_delay_flip_turnout(ctxt,
                                              step->data.turnout.num,
                                              step->data.turnout.state,
                                              time + 2);
                    continue;
                }
            }

            log("[%s] Next turnout is %d to %c (%d) at %d (%d)",
                ctxt->name,
                ctxt->next_turnout.action->data.turnout.num,
                ctxt->next_turnout.action->data.turnout.state,
                ctxt->next_turnout.action - ctxt->path,
                ctxt->next_turnout.step - ctxt->path,
                ctxt->path_step - ctxt->path);

            return;
        }
    }

    ctxt->next_turnout.step   = NULL;
    ctxt->next_turnout.action = NULL;
}

static void master_set_allowed_sensor(master* const ctxt) {

    const path_node* target = ctxt->next_stop.action;
    // setting the limit on the loop to be the current step might be too tight
    // we just want to find the sensor right before the merge
    for (; target <= ctxt->path_step; target++)
        if (target->type == PATH_SENSOR) break;

    assert(target->type == PATH_SENSOR,
           "[%s] Failed to find the sensor before the stop (%d)",
           ctxt->name, target->type);

    int next_dist = 0;
    const int result = get_sensor_from(target->data.sensor,
                                       &next_dist,
                                       &ctxt->allowed_sensor);
    assert(result == 0,
           "[%s] Failed to query Boris",
           ctxt->name);

    if (next_dist < 0) {
        log("[%s] Reversing at an exit!", ctxt->name);
        ctxt->allowed_sensor = target->data.sensor;
    }
}

static inline void master_find_next_stopping_point(master* const ctxt) {

    if (ctxt->path_completed) return;
    // this handles the case where we need to fast forward the route

    const path_node* const start_step =
        MIN(ctxt->next_stop.step, ctxt->path_step);

    for (const path_node* step = start_step - 1; step >= ctxt->path; step--) {
        // if we have found a reverse step
        if (step->type == PATH_REVERSE || step == ctxt->path) {
            // if the last step is a reverse
            if (step->type == PATH_REVERSE && step == ctxt->path)
                ctxt->next_stop.action = step + 1; // stop at non-reverse sensor
            else // we have a stopping point
                ctxt->next_stop.action = step;

            return;
        }
    }

    ABORT("[%s] Hit unreachable code (burst of location updates?)! %p %p %p",
          ctxt->name, start_step, ctxt->path_step, ctxt->next_stop.step);
}

static inline bool master_is_last_path_chunk(master* const ctxt) {
    // if the next stopping point is the very last step in the route
    // _or_ it is the second last step, but the second last step is a reverse
    return (ctxt->next_stop.action == ctxt->path ||
            ((ctxt->next_stop.action == (ctxt->path + 1)) &&
             ctxt->next_stop.step->type == PATH_REVERSE));
}

static inline void master_recalculate_stopping_point(master* const ctxt,
                                                     const int offset) {

    const int  stop_dist = master_current_stopping_distance(ctxt);

    // we want to be sure that we clear the turnout, so add padding
    const int stop_point = stop_dist -
        (master_is_last_path_chunk(ctxt) ?
         ctxt->destination_offset :
         master_tail_distance(ctxt) + ctxt->turnout_padding);

    // find out at which step we need to act...
    ctxt->next_stop =
        master_find_action_location(ctxt, ctxt->next_stop.action, stop_point);

    // we do not have enough track space to stop properly,
    // so just schedule it to happen immediately...
    if (ctxt->next_stop.step == NULL) {
        ctxt->next_stop.step   = ctxt->path_step;
        ctxt->next_stop.offset = offset + 2; // magic number TROLOLO
    }

    const sensor s = pos_to_sensor(ctxt->next_stop.step->data.sensor);
    log("[%s] Next stop at %c%d + %d mm",
        ctxt->name, s.bank, s.num, ctxt->next_stop.offset / 1000);
}

static void master_check_turnout_throwing(master* const ctxt,
                                          const int velocity, // current
                                          const int offset,   // from checkpoint
                                          const int time) {   // stamp!

    // check if we need to fast forward the turnout handling, this is separate
    // from regular fast forwarding because it may need to happen more
    // frequently since multiple turnouts can exist between sensors
    while (ctxt->path_step < ctxt->next_turnout.action)
        master_calculate_turnout_point(ctxt, time);

    while (ctxt->checkpoint.location.sensor ==
           ctxt->next_turnout.step->data.sensor) {

        // how much longer we have to wait until we reach the danger zone
        const int delay = (ctxt->next_turnout.offset - offset) / velocity;

        const path_node* const step = ctxt->next_turnout.action;

        if (delay < 0) {
            // TODO: what if the turnout is already in the correct direction?
            //       right we auto-correct in this case because the only error
            //       handling is a log message...

            if (velocity < 2000) // TROLOLO magic
                master_delay_flip_turnout(ctxt,
                                          step->data.turnout.num,
                                          step->data.turnout.state,
                                          time + 2); // TROLOLO magic

            log("[%s] Not enough time to throw turnout %d!",
                ctxt->name, step->data.turnout.num);
        }
        else {
            master_delay_flip_turnout(ctxt,
                                      step->data.turnout.num,
                                      step->data.turnout.state,
                                      time + delay);
            log("[%s] Sending turnout %d command to %c",
                ctxt->name, step->data.turnout.num, step->data.turnout.state);
        }

        // start the process again!
        master_calculate_turnout_point(ctxt, time);
    }
}

// TODO: this function could be a lot less wasteful on CPU cycles
//       and making it pure/const is not the solution
static int master_choose_short_move_speed(master* const ctxt,
                                          const int distance) {

    // NOTE: we only consider speeds 1-12
    for (int speed = 120; speed >= 10; speed -= 10) {

        const int  stop_dist =
            physics_stopping_distance(ctxt->blaster_ctxt, speed);
        const int start_dist =
            physics_starting_distance(ctxt->blaster_ctxt, speed);
        const int   min_dist = stop_dist + start_dist;

        if (distance > min_dist) return speed;
    }

    return 0; // there are no speeds for a move of that length...
}

static void master_check_throw_stop_command(master* const ctxt,
                                            const int velocity,
                                            const int offset,
                                            const int time) {

    if (ctxt->path_stopping || ctxt->path_completed) return;

    // figure out where we need to be stopping (speed might have changed)
    master_recalculate_stopping_point(ctxt, offset);

    // should we be stopping now?
    if (ctxt->checkpoint.location.sensor ==
        ctxt->next_stop.step->data.sensor) {

        log("[%s] Scheduling stop", ctxt->name);
        const int delay = (ctxt->next_stop.offset - offset) / velocity;

        master_set_speed(ctxt, 0, time + delay);
        master_set_allowed_sensor(ctxt);

        ctxt->path_stopping = true;

        // if this is the last stop, then we are done
        if (master_is_last_path_chunk(ctxt)) {

            if (ctxt->path[1].type == PATH_REVERSE)
                log("[%s] Pretend we just reversed at the end", ctxt->name);

            log("[%s] Done path", ctxt->name);
            ctxt->path_completed = true;

            // and we do not need to start back up again
        }
        else {
            log("[%s] Done path chunk", ctxt->name);
            master_find_next_stopping_point(ctxt);
            assert(ctxt->next_stop.step,
                   "[%s] Had non-sensor finishing step",
                   ctxt->name);
        }
    }
}

static inline int
master_short_move(master* const ctxt, const int offset) {

    // NOTE: we only consider speeds 2-12
    for (int speed = 120; speed > 10; speed -= 10) {

        const int  stop_dist = physics_stopping_distance(ctxt->blaster_ctxt,
                                                         speed);
        const int start_dist = physics_starting_distance(ctxt->blaster_ctxt,
                                                         speed);
        const int   min_dist = stop_dist + start_dist;

        if (offset > min_dist) {
            master_set_speed(ctxt, speed, 0);
            ctxt->short_moving_distance  = offset - start_dist;
            ctxt->short_moving_timestamp = Time();
            return speed;
        }
    }

    return 0;
}

static void master_short_move2(master* const ctxt) {

    const int time = Time();
    const track_location current_location =
        master_current_location(ctxt, time);

    const int       velocity = master_current_velocity(ctxt);
    const int      stop_dist = master_current_stopping_distance(ctxt);
    const int dist_remaining = ctxt->short_moving_distance -
        (stop_dist +
         (current_location.offset - ctxt->checkpoint.location.offset));
    const int      stop_time = dist_remaining / velocity;

    master_set_speed(ctxt, 0, time + stop_time);

    ctxt->short_moving_distance = 0;
}

static void master_setup_next_short_move(master* const ctxt,
                                         const int offset,
                                         const int delay) {

    // the extra distance past the branch that we have travelled needs
    // to be appeneded to the expected distance

    // we still want the offset, and we want to add it to the current
    // offset, but we still need to determine if we have hit the allowed_sensor

    // edge distance if we hit the allowed sensor
    const int edge_dist_post =
        ctxt->track[ctxt->checkpoint.location.sensor].reverse->edge[DIR_AHEAD].dist;

    // edge distance if we did not hit the allowed sensor
    const int edge_dist_pre  =
        ctxt->track[ctxt->checkpoint.location.sensor].edge[DIR_AHEAD].dist;

    const int offset_from_path =
        ctxt->checkpoint.location.offset + offset +
        (ctxt->checkpoint.location.sensor == ctxt->allowed_sensor ?
         edge_dist_post : -edge_dist_pre);

    // distance to the first checkpoint (reverse point) in the path
    const int dist_to_path_checkpoint =
        (ctxt->next_stop.action->dist - ctxt->path_step->dist) +
        offset_from_path;

    const sensor s = pos_to_sensor(ctxt->checkpoint.location.sensor);
    log("[%s] Next stop @ %d mm, current stop @ %c%d <%d mm> (%s %d %d %d %d)",
        ctxt->name,
        ctxt->next_stop.action->dist / 1000,
        s.bank, s.num,
        ctxt->path_step->dist / 1000,
        ctxt->checkpoint.location.sensor == ctxt->allowed_sensor ?
        "past allowed sensor" : "NOT past allowed sensor",
        edge_dist_post / 1000,
        edge_dist_pre / 1000,
        offset_from_path / 1000,
        dist_to_path_checkpoint / 1000);

    const int speed =
        master_choose_short_move_speed(ctxt, dist_to_path_checkpoint);

    if (speed) {
        log("[%s] Gonna short move at speed %d", ctxt->name, speed);
        // kick it, real good
        master_set_speed(ctxt, speed, delay);
    }
    // if there are no speeds, then abandon all hope
    else {
        log("[%s] Distance %d mm is too short for a short move",
            ctxt->name, dist_to_path_checkpoint / 1000);
        ctxt->path_step = NULL;
    }
}

static void master_setup_first_short_move(master* const ctxt,
                                          const int offset,
                                          const int delay) {

    // technically, what we want is the offset from where the path begins

    // the offset from the beggining of the path
    const int distance_to_first_point =
        ctxt->next_stop.action->dist -
        (ctxt->checkpoint.location.offset + offset);

    log("[%s] First stop @ %d mm, which is %d mm from the train",
        ctxt->name,
        ctxt->next_stop.action->dist / 1000,
        distance_to_first_point / 1000);

    const int speed =
        master_choose_short_move_speed(ctxt, distance_to_first_point);

    if (speed) {
        log("[%s] Gonna short move at speed %d", ctxt->name, speed);
        master_set_speed(ctxt, speed, delay);
    }
    // if there are no speeds, then abandon hope (find new route?)
    else {
        log("[%s] Distance %d mm is too short for a short move",
            ctxt->name, distance_to_first_point / 1000);
        ctxt->path_step = NULL;
    }
}

static inline void master_path_update(master* const ctxt,
                                      const int size,
                                      const path_node* path,
                                      const int tid) {

    const sensor to = pos_to_sensor(ctxt->destination);

    sensor from = { 'F', 1 };
    for (int i = size - 1; i >= 0; i--) {
        if (path[i].type == PATH_SENSOR) {
            from = pos_to_sensor(path[i].data.sensor);
            break;
        }
    }

    log("[%s] Got path %c%d -> %c%d in %d steps",
        ctxt->name, from.bank, from.num, to.bank, to.num, size);

    ctxt->path_worker    = tid;
    ctxt->path_step      = path + size - 1;
    ctxt->path           = path;
    ctxt->allowed_sensor = NUM_SENSORS;
    ctxt->path_stopping  = false;
    ctxt->path_completed = false;

    master_debug_path(ctxt);

    // some stuff we need to know before we kick off the path finding
    const int velocity = master_current_velocity(ctxt);
    const int     time = Time();
    // how far we have travelled from the sensor since the checkpoint
    const int   offset = ctxt->checkpoint.location.offset +
        (velocity * (time - ctxt->checkpoint.timestamp));


    if (path[size - 1].type == PATH_REVERSE) {
        log("[%s] Started with a reverse", ctxt->name);
        master_set_reverse(ctxt, time + 2);

        // manually fast forward this step
        ctxt->path_step--;
    } else {
        // Get us up to date on where we are
        ctxt->path_step = master_try_fast_forward(ctxt);
    }

    // wait until we know where we are starting from
    ctxt->next_turnout.step   = ctxt->path_step;
    ctxt->next_turnout.action = ctxt->path_step;
    ctxt->next_stop.step      = ctxt->path_step;
    ctxt->next_stop.action    = ctxt->path_step;

    master_calculate_turnout_point(ctxt, time);

    master_find_next_stopping_point(ctxt);
    master_recalculate_stopping_point(ctxt, offset);

    master_check_turnout_throwing(ctxt, velocity, offset, time);
    master_check_throw_stop_command(ctxt, velocity, offset, time);

    // if the train is not moving, or it is slowing down, then kick it off
    if ((ctxt->checkpoint.speed == 0 || ctxt->checkpoint.next_speed == 0) &&
        // delay the short move until the path recalculation
        path[size - 1].type != PATH_REVERSE)
        master_setup_first_short_move(ctxt, offset, time + 2);
}

static inline void
master_short_move_command(master* const ctxt,
                          const int offset,
                          const control_req* const pkg,
                          const int tid) {

    const int speed = master_short_move(ctxt, offset);
    if (speed)
        mark_log("[%s] Short move %d mm at speed %d",
                 ctxt->name, offset / 1000, speed / 10);
    else
        log("[%s] Distance %d mm too short to start and stop!",
            ctxt->name, offset / 1000);

    master_release_control_courier(ctxt, pkg, tid);
}

static inline void
master_check_sensor_to_stop_at(master* const ctxt) {
    if (!(ctxt->sensor_to_stop_at == ctxt->checkpoint.location.sensor &&
          ctxt->checkpoint.event == EVENT_SENSOR)) return;

    master_set_speed(ctxt, 0, 0);

    ctxt->sensor_to_stop_at = -1;
    const sensor s = pos_to_sensor(ctxt->checkpoint.location.sensor);
    log("[%s] Hit sensor %c%d at %d. Stopping!",
        ctxt->name, s.bank, s.num, ctxt->checkpoint.timestamp);
}

static int add_reserve_node(const track_node* const node,
                            const track_node* lst[],
                            int*  const insert) {

    lst[*insert]  = node;
    *insert      += 1;

    return get_reserve_length(node);
}

static void get_reserve_list(const master* const ctxt,
                             const int dist,
                             const int dir,
                             const track_node* const node,
                             const track_node* lst[],
                             int* const insert) {

    int res = 0;
    assert(XBETWEEN(node - ctxt->track, -1, TRACK_MAX+1),
           "bad track index %d", node - ctxt->track);

    if (dist <=  0) return;
    if (1 == dir) {
        res = add_reserve_node(node->reverse, lst, insert);
        if (res >= dist) return;
    }
    res += add_reserve_node(node, lst, insert);

    switch (node->type) {
    case NODE_NONE:
    case NODE_SENSOR:
    case NODE_MERGE:
        get_reserve_list(ctxt, dist-res, 1, node->edge[DIR_AHEAD].dest,
                         lst, insert);
        break;
    case NODE_BRANCH:
        get_reserve_list(ctxt, dist-res, 1, node->edge[DIR_STRAIGHT].dest,
                         lst, insert);
        get_reserve_list(ctxt, dist-res, 1, node->edge[DIR_CURVED].dest,
                         lst, insert);
        break;
    case NODE_EXIT:
        break;
    case NODE_ENTER:
        assert(false, "I'm legit curious if this can happen");
    }
}

static inline void
master_check_sensor_to_block_until(master* const ctxt) {

    if (!(ctxt->sensor_block.sensor == ctxt->checkpoint.location.sensor &&
          ctxt->checkpoint.event == EVENT_SENSOR)) return;

    const int result = Reply(ctxt->sensor_block.tid, NULL, 0);
    assert(result == 0,
           "[%s] Fuuuuu at %d (%d)",
           ctxt->name, ctxt->sensor_block.tid, result);
    UNUSED(result);

    log("[%s] Waking up %d", ctxt->name, ctxt->sensor_block.tid);

    ctxt->sensor_block.sensor = -1;
}

static inline bool __attribute__((pure))
should_perform_reservation(const master* const ctxt) {
    return ctxt->last_reservation_speed < ctxt->checkpoint.next_speed
      //|| !ctxt->checkpoint.is_accelerating
        || EVENT_ACCELERATION != ctxt->checkpoint.event;
}

static inline int perform_reservation(master* const ctxt) {
    assert(XBETWEEN(ctxt->checkpoint.location.sensor, -1, NUM_SENSORS),
           "Can't Reserve From not sensor %d",
           ctxt->checkpoint.location.sensor);

    const track_node* node =
        &ctxt->track[ctxt->checkpoint.location.sensor];

    const int offset = ctxt->checkpoint.location.offset;
    const int head   = master_head_distance(ctxt);
    const int tail   = master_tail_distance(ctxt);

    int moving_dist = 0;
    if (0 != ctxt->checkpoint.speed) {
        const int stop_dist = master_current_stopping_distance(ctxt);
        moving_dist = stop_dist + node->edge[0].dist;
    }

    const track_node* reserve[60];
    int               insert = 0;
    const int head_dist = head + offset + moving_dist + 20000;
    const int tail_dist = MAX(tail - offset + 20000, tail);

    nik_log("[%s] Reserving from %s\tH:%d\tT:%d\tO:%d\tE:%s", ctxt->name,
            node->name, head_dist / 1000, tail_dist / 1000, offset / 1000,
            event_to_str(ctxt->checkpoint.event));

    assert(offset <  2000000, "offset is really big %dmm", offset / 1000);
    assert(offset > -2000000, "offset is really big %dmm", offset / 1000);
    assert(head_dist < 3000000, "WTF HEAD H:%d\tO:%d\tM:%d\ts:%d",
           head / 1000, offset / 1000, moving_dist / 1000,
           ctxt->checkpoint.speed);
    assert(tail_dist < 3000000, "WTF TAIL T:%d\tO:%d\t",
           tail / 1000, offset / 1000);


    get_reserve_list(ctxt, tail_dist, 0, node->reverse, reserve, &insert);
    get_reserve_list(ctxt, head_dist, 0, node,          reserve, &insert);
    assert(XBETWEEN(insert, 0, 60), "bad insert length %d", insert);

    ctxt->last_reservation_speed = ctxt->checkpoint.speed;
    return reserve_section(ctxt->train_id, reserve, insert);
}

static inline void master_location_update(master* const ctxt,
                                          const train_state* const state,
                                          const blaster_req* const pkg,
                                          const int tid) {

    memcpy(&ctxt->checkpoint, state, sizeof(train_state));
    const sensor l = pos_to_sensor(state->location.sensor);
    log("[%s] Got position update (%c%d) %p %s",
        ctxt->name,
        l.bank, l.num,
        state,
        event_to_str(ctxt->checkpoint.event));

    const int result = Reply(tid, (char*)pkg, sizeof(blaster_req));
    assert(result == 0,
           "[%s] Failed to send courier back to blaster (%d)",
           ctxt->name, result);
    UNUSED(result);

    if (ctxt->checkpoint.event == EVENT_SENSOR) ctxt->active = 1;
    if (ctxt->active && should_perform_reservation(ctxt)) {
        const bool got_reservation = perform_reservation(ctxt);

        if (!got_reservation) {
            log("[%s] Encrouching %d", ctxt->name, ctxt->checkpoint.speed);
            master_set_speed(ctxt, 0, 0);
        }
    } else {
        nik_log("[%s] NO NEW RESERVATION", ctxt->name);
    }

    master_check_sensor_to_stop_at(ctxt);
    master_check_sensor_to_block_until(ctxt);

    if (ctxt->short_moving_distance &&
        // no longer accelerating
        !ctxt->checkpoint.is_accelerating &&
        // and enough time has passed
        ctxt->checkpoint.timestamp > (ctxt->short_moving_timestamp + 10)) {

        master_short_move2(ctxt);
    }

    if (ctxt->path_step && ctxt->path_step >= ctxt->path) {
        if (ctxt->checkpoint.speed == 0)
            log("[%s] STOPPED! %d", ctxt->name, ctxt->path_step - ctxt->path);

        // we have a few cases here

        // 1 - we just hit a sensor while reversing legitimately
        //     so we need to recognize that it is the sensor after the previous
        //     sensor in the path and allow it, then mark the reverse sensor as
        //     being allowed as well
        // 2 - it is the reverse sensor which is allowed to be hit, this should
        //     get caught here, too, but maybe not if positioning is off by much

        // check fast forwarding
        const path_node* const still_on_path = master_try_fast_forward(ctxt);

        // if we think we are off path
        if (!still_on_path) {
            // check if we are really off path
            const int reverse  = reverse_sensor(ctxt->allowed_sensor);
            const int in_front = ctxt->allowed_sensor;
            if (!(in_front == ctxt->checkpoint.location.sensor      ||
                  reverse == ctxt->checkpoint.location.sensor       ||
                  in_front == ctxt->checkpoint.next_location.sensor ||
                  reverse == ctxt->checkpoint.next_location.sensor)) {

                ctxt->path_step = NULL;

                // try to find a new path
                const sensor s = pos_to_sensor(ctxt->allowed_sensor);
                const sensor a = pos_to_sensor(ctxt->checkpoint.location.sensor);
                log("[%s] Finding a new route! Allowed: %c%d, Actual: %c%d",
                    ctxt->name,
                    s.bank, s.num,
                    a.bank, a.num);
                master_goto(ctxt, ctxt->destination, ctxt->destination_offset);
                return;
            }
        }
        else {
            // actually fast forward now
            ctxt->path_step = still_on_path;
        }

        // now, the million dollar question is: are we too late to throw
        // the turnout and/or stop; we determine this by looking at the
        const int velocity = master_current_velocity(ctxt);
        const int     time = Time();
        // how far we have travelled from the sensor since the checkpoint
        const int   offset = ctxt->checkpoint.location.offset +
            (velocity * (time - ctxt->checkpoint.timestamp));

        // always check if we need to handle a turnout if still on path
        master_check_turnout_throwing(ctxt, velocity, offset, time);

        // no more commands to be given to the train if we are done
        if (ctxt->path_completed && ctxt->checkpoint.speed == 0) {
            const sensor s = pos_to_sensor(ctxt->destination);
            log("[%s] Arriving at %c%d", ctxt->name, s.bank, s.num);
            ctxt->path_step = NULL;
        }
        // 3 - we hit speed 0, so we should throw the reverse command if
        //     we are not path_completed
        else if (ctxt->checkpoint.speed == 0 && ctxt->path_stopping
                                             && !ctxt->path_completed) {
            log("[%s] Hit reversing point!", ctxt->name);
            master_set_reverse(ctxt, time + 1); // TROLOLO magic number
            master_setup_next_short_move(ctxt, offset, time + 10);
            ctxt->path_stopping = false;
        }
        else {
            // 4 - we hit our desired speed, so if we are short moving, we need
            //     need to check the stop point and turnout point using the
            //     estimated location
            // 5 - regular deal
            log("[%s] Check if we need to throw stop", ctxt->name);
            master_check_throw_stop_command(ctxt, velocity, offset, time);
        }
    }
}

static TEXT_COLD void master_init(master* const ctxt) {
    memset(ctxt, 0, sizeof(master));

    int tid;
    int init[2];
    int result = Receive(&tid, (char*)&init, sizeof(init));

    if (result != sizeof(init))
        ABORT("[Master] Failed to receive init data (%d)", result);

    ctxt->train_id  = init[0];
    ctxt->train_gid = pos_to_train(ctxt->train_id);

    result = Reply(tid, NULL, 0);
    if (result != 0)
        ABORT("[Master] Failed to wake up Lenin (%d)", result);

    sprintf(ctxt->name, "MASTR%d", ctxt->train_gid);
    ctxt->name[7] = '\0';
    if (RegisterAs(ctxt->name))
        ABORT("[Master] Failed to register train (%d)", ctxt->train_gid);

    char colour[32];
    char* ptr = sprintf(colour,
                        ESC_CODE "%s" COLOUR_SUFFIX,
                        train_to_colour(ctxt->train_gid));
    sprintf_char(ptr, '\0');

    ptr = sprintf(ctxt->name,
                  "%sMaster %d" COLOUR_RESET,
                  colour, ctxt->train_gid);
    sprintf_char(ptr, '\0');

    ctxt->my_tid  = myTid(); // cache it, avoid unneeded syscalls
    ctxt->blaster = init[1];
    ctxt->control = myParentTid();

    blaster_req_type info = BLASTER_MASTER_CONTEXT;
    result = Send(ctxt->blaster,
                  (char*)&info, sizeof(info),
                  (char*)&ctxt->blaster_ctxt, sizeof(struct blaster_context*));
    assert(result > 0,
           "[%s] Failed to get context pointer from blaster (%d)",
           ctxt->name, result);
    //TODO: I dont like this but easier than passing both pointers
    *(const track_node**)&ctxt->track = ctxt->blaster_ctxt->track;


    ctxt->path_admin = WhoIs((char*)PATH_ADMIN_NAME);
    CHECK_WHOIS(ctxt->path_admin, "I started before path admin!");

    ctxt->mission_control_tid = WhoIs((char*)MISSION_CONTROL_NAME);
    CHECK_WHOIS(ctxt->mission_control_tid, "Failed, Mission Control Tid");

    ctxt->turnout_padding     = TURNOUT_CLEARANCE_OFFSET_DEFAULT;
    ctxt->path_worker         = -1;
    ctxt->destination         = -1;
    ctxt->sensor_to_stop_at   = -1;
    ctxt->sensor_block.sensor = -1;
}

static TEXT_COLD void master_init_couriers(master* const ctxt,
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
    UNUSED(ctxt);
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
        .type = BLASTER_MASTER_WHERE_ARE_YOU
    };

    const courier_package blaster_package = {
        .receiver = context.blaster,
        .message  = (char*)&blaster_callin,
        .size     = sizeof(blaster_callin)
    };

    master_init_couriers(&context,
                         &control_package,
                         &blaster_package);

    log("[%s] Initialized", context.name);

    FOREVER {
        int tid;
        master_req req;

        int result = Receive(&tid, (char*)&req, sizeof(req));
        UNUSED(result);
        assert(result > 0,
               "[%s] Invalid message size (%d)", context.name, result);

        switch (req.type) {
        case MASTER_GOTO_LOCATION:
            master_goto_command(&context, &req, &control_callin, tid);
            break;

        case MASTER_SHORT_MOVE:
            master_short_move_command(&context,
                                      req.arg1,
                                      &control_callin,
                                      tid);
            break;

        case MASTER_STOP_AT_SENSOR:
            master_stop_at_sensor(&context, &control_callin, req.arg1, tid);
            break;

        case MASTER_BLOCK_UNTIL_SENSOR:
            master_block_until_sensor(&context, &control_callin, &req, tid);
            break;

        case MASTER_WHERE_ARE_YOU:
            master_where_you_at(&context, &control_callin, req.arg1, tid);
            break;

        case MASTER_PATH_DATA:
            master_path_update(&context, req.arg1, (path_node*)req.arg3, tid);
            break;

        case MASTER_BLASTER_LOCATION:
            master_location_update(&context,
                                   (train_state*)req.arg1,
                                   &blaster_callin,
                                   tid);
            break;

        case MASTER_UPDATE_TWEAK:
            master_update_tweak(&context, &control_callin, &req, tid);
            break;
        }
    }
}
