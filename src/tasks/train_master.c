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

typedef struct train_checkpoint {
    train_event type;
    int         sensor;
    int         offset;
    int         timestamp;
    int         speed;
    train_dir   direction;
    bool        is_accel;
} train_checkpoint;

typedef struct train_action_point {
    int step;
    int offset;
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

    int              turnout_padding;
    train_checkpoint checkpoint;

    int          active;
    int          destination;        // destination sensor
    int          destination_offset; // in centimeters
    int          sensor_to_stop_at;  // special case for handling ss command
    sensor_block sensor_block;

    int              short_moving_distance;
    int              short_moving_timestamp;

    int              path_steps;
    const path_node* path;

    action_point     next_turnout;
    int              next_turnout_step;
    action_point     next_reverse;
    action_point     next_stop;

    struct blaster_context* const blaster_ctxt;

    int reserved[TRACK_MAX];
    const track_node* const track;
} master;

static int master_current_velocity(master* const ctxt) {
    return physics_velocity(ctxt->blaster_ctxt,
                            ctxt->checkpoint.speed,
                            velocity_type(ctxt->checkpoint.sensor));
}

static int master_current_stopping_distance(master* const ctxt) {
    return physics_stopping_distance(ctxt->blaster_ctxt,
                                     ctxt->checkpoint.speed);
}

static inline int
master_turnout_stopping_distance(master* const ctxt) {
    return master_current_stopping_distance(ctxt) +
        ctxt->turnout_padding;
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

    const int tid = Create(TRAIN_COURIER_PRIORITY, delayed_one_way_courier);
    assert(tid >= 0,
           "[%s] Failed to create one time courier (%d)",
           ctxt->name, tid);

    const int result = Send(tid, (char*)&message, sizeof(message), NULL, 0);
    assert(result == 0,
           "[%s] Failed to setup one time courier (%d)",
           ctxt->name, result);
    UNUSED(result);
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
        ctxt->checkpoint.offset + (master_current_velocity(ctxt) * time_delta);

    const track_location l = {
        .sensor = ctxt->checkpoint.sensor,
        .offset = distance_delta
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
                            const int start_index,
                            const int offset) {

    const path_node* const start = &ctxt->path[start_index];

    int i = start_index;
    for (; i <= ctxt->path_steps; i++) {
        // now, we want to walk back through the path...
        if (ctxt->path[i].type == PATH_SENSOR) {

            // if the distance from the start point
            const int dist = start->dist - ctxt->path[i].dist;
            if (dist > offset) {
                const action_point l = {
                    .step   = i,
                    .offset = abs(dist - offset)
                };
                return l;
            }
        }
    }

    const action_point l = {
        .step = -1
    };

    return l;
}

static inline bool master_try_fast_forward(master* const ctxt) {

#ifdef DEBUG
    int start = -1;
#endif
    for (int i = ctxt->path_steps; i >= 0; i--) {
        if (ctxt->path[i].type == PATH_SENSOR) {

#ifdef DEBUG
            if (start == -1) start = i;
#endif
            if (ctxt->path[i].data.sensor == ctxt->checkpoint.sensor) {
#ifdef DEBUG
                if (start != i) {
                    const sensor head =
                        pos_to_sensor(ctxt->path[start].data.sensor);
                    const sensor tail =
                        pos_to_sensor(ctxt->path[i].data.sensor);

                    log("[%s] %c%d >> %c%d",
                        ctxt->name, head.bank, head.num, tail.bank, tail.num);

                }
#endif
                ctxt->path_steps = i; // successfully fast forwarded
                return true;
            }
        }
    }

    return false; // nope :(
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

    pa_request request = {
        .type = PA_GET_PATH,
        .req  = {
            .requestor   = ctxt->my_tid,
            .header      = MASTER_PATH_DATA,
            .sensor_to   = destination,
            .sensor_from = ctxt->checkpoint.sensor, // hmmm
            .opts        = PATH_NO_REVERSE_MASK | ctxt->train_id,
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
        if (master_try_fast_forward(ctxt))
            return;

        // TODO: try to rewind?

        // TODO: handle the offset
    }

    master_goto(ctxt, destination, offset);
    master_release_control_courier(ctxt, pkg, tid);
}

static inline void master_calculate_turnout_point(master* const ctxt) {

    // this handles the case where we need to fast forward the route
    const int start_index = MIN(ctxt->next_turnout_step, ctxt->path_steps);

    for (int i = start_index - 1; i >= 0; i--) {
        if (ctxt->path[i].type == PATH_TURNOUT) {

            const int direction_offset = TURNOUT_DISTANCE +
                (ctxt->checkpoint.direction == DIRECTION_FORWARD ?
                 ctxt->blaster_ctxt->measurements.front :
                 ctxt->blaster_ctxt->measurements.back);

            // the actual turnout metadata
            ctxt->next_turnout_step = i;
            // find out at which step we need to act...
            ctxt->next_turnout =
                master_find_action_location(ctxt, i, direction_offset);

            const sensor s =
                pos_to_sensor(ctxt->path[ctxt->next_turnout.step].data.sensor);
            log("[%s] Want to flip %d to %c at step %d + %d mm (step %d)",
                ctxt->name,
                ctxt->path[i].data.turnout.num,
                ctxt->path[i].data.turnout.state,
                s.bank, s.num,
                ctxt->next_turnout.offset / 1000, i);

            return;
        }
    }

    ctxt->next_turnout.step = -1;
}

static inline void master_calculate_stopping_point(master* const ctxt) {
    const int stop_dist = master_current_stopping_distance(ctxt);
    ctxt->next_stop = master_find_action_location(ctxt, 0, stop_dist);

    const sensor s =
        pos_to_sensor(ctxt->path[ctxt->next_stop.step].data.sensor);
    log("[%s] Want to stop at %c%d + %d mm (%d mm)",
        ctxt->name, s.bank, s.num, ctxt->next_stop.offset, stop_dist);
}

static void master_delay_flip_turnout(master* const ctxt,
                                      const int turn,
                                      const int direction,
                                      const int action_time) {

    const int c = Create(MASTER_TURNOUT_PRIORITY, delayed_one_way_courier);
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

static void master_check_turnout_throwing(master* const ctxt,
                                          const int velocity,
                                          const int offset,
                                          const int time) {

    // check if we need to fast forward the turnout handling, this is separate
    // from regular fast forwarding because it may need to happen more
    // frequently since multiple turnouts can exist between sensors
    while (ctxt->path_steps < ctxt->next_turnout_step)
        master_calculate_turnout_point(ctxt);

    while (ctxt->checkpoint.sensor ==
           ctxt->path[ctxt->next_turnout.step].data.sensor) {

        // how much longer we have to wait until we reach the danger zone
        const int delay =
            (ctxt->next_turnout.offset - offset) / velocity;

        const path_node* const node = &ctxt->path[ctxt->next_turnout_step];

        if (delay < 0 ) {
            log("[%s] Not enough time to throw turnout %d!",
                ctxt->name, node->data.turnout.num);
        }
        else {
            master_delay_flip_turnout(ctxt,
                                      node->data.turnout.num,
                                      node->data.turnout.state,
                                      time + delay);
            log("[%s] Sending turnout %d command to %c",
                ctxt->name, node->data.turnout.num, node->data.turnout.state);
        }

        // start the process again!
        master_calculate_turnout_point(ctxt);
    }
}

static void master_check_throw_stop_command(master* const ctxt,
                                            const int velocity,
                                            const int offset,
                                            const int time) {

    // figure out where we need to be stopping
    master_calculate_stopping_point(ctxt);

    if (ctxt->checkpoint.sensor ==
        ctxt->path[ctxt->next_stop.step].data.sensor) {

        const int delay = (ctxt->next_stop.offset - offset) / velocity;
        master_set_speed(ctxt, 0, time + delay);

        // if we still have turnouts to throw, then throw them all right now!
        for (int i = ctxt->path_steps; i >= 0; i--) {
            const path_node* const node = &ctxt->path[i];
            if (node->type == PATH_TURNOUT) {
                master_delay_flip_turnout(ctxt,
                                          node->data.turnout.num,
                                          node->data.turnout.state,
                                          time + 2); // the magic 2!
            }
        }

        ctxt->path_steps = -1;
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

    ctxt->path_worker  = tid;
    ctxt->path_steps   = size - 1;
    ctxt->path         = path;

    // Get us up to date on where we are
    master_try_fast_forward(ctxt);

    // wait until we know where we are starting from
    ctxt->next_turnout_step = ctxt->path_steps;

    // now, the million dollar question is: are we too late to throw
    // the turnout; we determine this by looking at the
    const int velocity = master_current_velocity(ctxt);
    const int     time = Time();
    // how far we have travelled from the sensor since the checkpoint
    const int   offset = ctxt->checkpoint.offset +
        (velocity * (time - ctxt->checkpoint.timestamp));

    master_calculate_turnout_point(ctxt);
    master_check_turnout_throwing(ctxt, velocity, offset, time);
    master_check_throw_stop_command(ctxt, velocity, offset, time);

    for (int i = ctxt->path_steps; i >= 0; i--) {
        switch(ctxt->path[i].type) {
        case PATH_SENSOR: {
            const sensor print = pos_to_sensor(ctxt->path[i].data.int_value);
            log("%d  \tS\t%c%d\t%d", i, print.bank, print.num, ctxt->path[i].dist);
            break;
        }
        case PATH_TURNOUT:
            log("%d  \tT\t%d %c\t%d",
                i, ctxt->path[i].data.turnout.num, ctxt->path[i].data.turnout.state,
                ctxt->path[i].dist);
            break;
        case PATH_REVERSE:
            log("%d  \tR\t\t%d", i, ctxt->path[i].dist);
            break;
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
        (stop_dist + (current_location.offset - ctxt->checkpoint.offset));
    const int      stop_time = dist_remaining / velocity;

    master_set_speed(ctxt, 0, time + stop_time);

    ctxt->short_moving_distance = 0;
}

static inline void
master_short_move_command(master* const ctxt,
                          const int offset,
                          const control_req* const pkg,
                          const int tid) {

    const int speed = master_short_move(ctxt, offset);
    if (speed)
        log("[%s] Short move %d mm at speed %d",
            ctxt->name, offset / 1000, speed / 10);
    else
        log("[%s] Distance %d mm too short to start and stop!",
            ctxt->name, offset / 1000);

    master_release_control_courier(ctxt, pkg, tid);
}

static inline void
master_check_sensor_to_stop_at(master* const ctxt) {

    if (!(ctxt->sensor_to_stop_at == ctxt->checkpoint.sensor &&
          ctxt->checkpoint.type == EVENT_SENSOR)) return;

    master_set_speed(ctxt, 0, 0);

    ctxt->sensor_to_stop_at = -1;
    const sensor s = pos_to_sensor(ctxt->checkpoint.sensor);
    log("[%s] Hit sensor %c%d at %d. Stopping!",
        ctxt->name, s.bank, s.num, ctxt->checkpoint.timestamp);
}

void reserve_stop_dist(master* const ctxt,
                       const int dist,
                       const int dir,
                       const track_node* const node,
                       const track_node** lst,
                       int* const insert) {

    if (dist <= 0) return;

    const track_node* const used_node = dir ? node->reverse : node;
    assert(XBETWEEN(used_node - ctxt->track, -1, TRACK_MAX+1),
           "bad track index %d", used_node - ctxt->track);

    const int res = get_reserve_length(used_node);
    lst[*insert]  = used_node;
    *insert      += 1;
    // next reserve the forward direction;

    if (dir == 1) {
        reserve_stop_dist(ctxt, dist-res, 0, node, lst, insert);
        return;
    }

    switch (node->type) {
    case NODE_NONE:
    case NODE_SENSOR:
    case NODE_MERGE:
        reserve_stop_dist(ctxt, dist-res, 1, node->edge[DIR_AHEAD].dest,
                          lst, insert);
        break;
    case NODE_BRANCH:
        reserve_stop_dist(ctxt, dist-res, 1, node->edge[DIR_STRAIGHT].dest,
                          lst, insert);
        reserve_stop_dist(ctxt, dist-res, 1, node->edge[DIR_CURVED].dest,
                          lst, insert);
        break;
    case NODE_EXIT:
        break;
    case NODE_ENTER:
        assert(false, "I'm legit curious if this can happen TROLOLO");
    }
}

static inline void
master_check_sensor_to_block_until(master* const ctxt) {

    if (!(ctxt->sensor_block.sensor == ctxt->checkpoint.sensor &&
          ctxt->checkpoint.type == EVENT_SENSOR)) return;

    const int result = Reply(ctxt->sensor_block.tid, NULL, 0);
    assert(result == 0,
           "[%s] Fuuuuu at %d (%d)",
           ctxt->name, ctxt->sensor_block.tid, result);
    UNUSED(result);

    log("[%s] Waking up %d", ctxt->name, ctxt->sensor_block.tid);

    ctxt->sensor_block.sensor = -1;
}

static inline void master_location_update(master* const ctxt,
                                          const master_req* const req,
                                          const blaster_req* const pkg,
                                          const int tid) {

    // log("[%s] Got position update", ctxt->name);
    ctxt->checkpoint.type      = req->arg1;
    ctxt->checkpoint.sensor    = req->arg2;
    ctxt->checkpoint.offset    = req->arg3;
    ctxt->checkpoint.timestamp = req->arg4;
    ctxt->checkpoint.speed     = req->arg5;
    ctxt->checkpoint.direction = req->arg6;
    ctxt->checkpoint.is_accel  = req->arg7;

    const int result = Reply(tid, (char*)pkg, sizeof(blaster_req));
    assert(result == 0,
           "[%s] Failed to send courier back to blaster (%d)",
           ctxt->name, result);
    UNUSED(result);

    if (ctxt->checkpoint.type == EVENT_SENSOR) ctxt->active = 1;
    if (ctxt->active) {
        int   insert = 1;
        const track_node* path_way[40];

        const int dist         = master_current_stopping_distance(ctxt);
        const track_node* node = &ctxt->track[ctxt->checkpoint.sensor];
        path_way[0]            = node->reverse;

        reserve_stop_dist(ctxt, dist + ctxt->checkpoint.offset + 50000,
                          0, node, path_way, &insert);
        assert(XBETWEEN(insert, 0, 40), "bad insert length %d", insert); 

        if (!reserve_section(ctxt->train_id, path_way, insert)) {
            log ("TRAIN %d Encrouching", ctxt->train_id);
            master_set_speed(ctxt, 0, 0);
        }
    }

    master_check_sensor_to_stop_at(ctxt);
    master_check_sensor_to_block_until(ctxt);

    if (ctxt->short_moving_distance &&
        // no longer accelerating
        !ctxt->checkpoint.is_accel &&
        // and enough time has passed
        ctxt->checkpoint.timestamp > (ctxt->short_moving_timestamp + 10)) {

        master_short_move2(ctxt);
    }

    if (ctxt->path_steps >= 0) {
        if (master_try_fast_forward(ctxt)) {
            // now, the million dollar question is: are we too late to throw
            // the turnout; we determine this by looking at the
            const int velocity = master_current_velocity(ctxt);
            const int     time = Time();
            // how far we have travelled from the sensor since the checkpoint
            const int   offset = ctxt->checkpoint.offset +
                (velocity * (time - ctxt->checkpoint.timestamp));

            // now, check if we need to handle a turnout
            master_check_turnout_throwing(ctxt, velocity, offset, time);

            // and if it is stopping time
            master_check_throw_stop_command(ctxt, velocity, offset, time);
        }
        else { // shit, we need a new path...
            log("[%s] Finding a new route!", ctxt->name);
            master_goto(ctxt, ctxt->destination, ctxt->destination_offset);
        }
    }
}

static inline void
master_stop_train(master* const ctxt, const int tid) {

    log("Stopping!");
    // TODO: courier this directly to the train?
    master_set_speed(ctxt, 0, 0);

    const int result = Reply(tid, NULL, 0); // kill it with fire (prejudice)
    assert(result == 0,
           "[%s] Failed to kill delay notifier (%d)",
           ctxt->name, result);
    UNUSED(result);
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
    ctxt->path_steps          = -1;
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
            master_location_update(&context, &req, &blaster_callin, tid);
            break;

        case MASTER_STOP_TRAIN:
            master_stop_train(&context, tid);
            break;

        case MASTER_UPDATE_TWEAK:
            master_update_tweak(&context, &control_callin, &req, tid);
            break;
        }
    }
}
