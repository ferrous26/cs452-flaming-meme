
#include <debug.h>
#include <tasks/train_master.h>
#include <tasks/train_blaster.h>
#include <tasks/train_control.h>
#include <tasks/path_admin.h>
#include <tasks/path_worker.h>
#include <tasks/name_server.h>
#include <tasks/priority.h>
#include <tasks/courier.h>
#include <tasks/clock_server.h>
#include <tasks/mission_control.h>

#define TURNOUT_DISTANCE 300000

typedef struct {
    int              train_id;
    int              train_gid;
    char             name[32];

    int              my_tid;             // tid of self
    int              blaster;            // tid of control task
    int              control;            // tid of control task

    int              path_admin;         // tid of the path admin
    int              path_worker;        // tid of the path worker

    train_event      checkpoint_type;
    int              checkpoint;         // sensor we last had update at
    int              checkpoint_offset;  // in micrometers
    int              checkpoint_time;
    int              checkpoint_velocity;
    int              checkpoint_stopping_distance;

    int              destination;        // destination sensor
    int              destination_offset; // in centimeters
    int              sensor_to_stop_at;  // special case for handling ss command

    int              path_steps;
    const path_node* path;
    int              turnout_step;       // next turnout step
    int              reverse_step;       // next reverse step
} master;


static int master_new_delay_courier(master* const ctxt) {
    const int tid = Create(TIMER_COURIER_PRIORITY, time_notifier);

    assert(tid >= 0,
           "[%s] Error setting up a timer notifier (%d)",
           ctxt->name, tid);
    UNUSED(ctxt);

    return tid;
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

    const int result = Reply(tid, (char*)pkg, sizeof(control_req));
    assert(result == 0,
           "[%s] Failed to send courier back to Lenin (%d)",
           ctxt->name, result);
    UNUSED(result);
}

static void
master_where_are_you(master* const ctxt,
                     const control_req* const pkg,
                     const int request_tid,
                     const int courier_tid) {

    const int     time_delta = Time() - ctxt->checkpoint_time;
    const int distance_delta =
        ctxt->checkpoint_offset + (ctxt->checkpoint_velocity * time_delta);

    const track_location l = {
        .sensor = ctxt->checkpoint,
        .offset = distance_delta
    };

    const int result1 = Reply(request_tid, (char*)&l, sizeof(l));
    assert(result1 == 0,
           "[%s] Failed to respond to whereis command (%d)",
           result1);
    UNUSED(result1);

    const int result2 = Reply(courier_tid, (char*)pkg, sizeof(control_req));
    assert(result2 == 0,
           "[%s] Failed to respond to whereis command (%d)",
           result2);
    UNUSED(result2);
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

            if (ctxt->path[i].data.sensor == ctxt->checkpoint) {
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
            .sensor_from = ctxt->checkpoint, // hmmm
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
    const int result = Reply(tid, (char*)pkg, sizeof(control_req));
    assert(result == 0,
           "[%s] Did not wake up %d (%d)", ctxt->name, tid, result);
    UNUSED(result);
}

static inline void master_calculate_turnout_point(master* const ctxt) {
    for (int i = ctxt->turnout_step - 1; i >= 0; i--) {
        if (ctxt->path[i].type == PATH_TURNOUT) {
            ctxt->turnout_step = i;
            log("[%s] FOUND TURN %d", ctxt->name, i);
            return;
        }
    }
    ctxt->turnout_step = 0;
}

static inline void master_calculate_reverse_point(master* const ctxt) {
    for (int i = ctxt->reverse_step - 1; i >= 0; i--) {
        if (ctxt->path[i].type == PATH_TURNOUT) {
            ctxt->reverse_step = i;
            return;
        }
    }
    ctxt->reverse_step = 0;
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
    ctxt->turnout_step = ctxt->path_steps;
    ctxt->reverse_step = ctxt->path_steps;

    master_calculate_turnout_point(ctxt);
    master_calculate_reverse_point(ctxt);
}

static void master_delay_flip_turnout(master* const ctxt,
                                      const int turn,
                                      const int direction,
                                      const int action_time) {
    const int c = master_new_delay_courier(ctxt);

    struct {
        tnotify_header head;
        master_req     req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = action_time
        },
        .req = {
            .type = MASTER_FLIP_TURNOUT,
            .arg1 = turn,
            .arg2 = direction
        }
    };

    const int result = Send(c, (char*)&msg, sizeof(msg), NULL, 0);
    log("[%s] Sent Switch: \t%d\tDir: %d\tTime: %d", ctxt->name,
        turn, direction, action_time);

    if (result < 0)
        ABORT("[%s] Failed to send turnout delay (%d)", ctxt->name, result);
}

static void master_delay_stop(master* const ctxt,
                              const int delay) {
    const int c = master_new_delay_courier(ctxt);

    struct {
        tnotify_header head;
        master_req     req;
    } msg = {
        .head = {
            .type  = DELAY_ABSOLUTE,
            .ticks = ctxt->checkpoint_time + delay
        },
        .req = {
            .type = MASTER_STOP_TRAIN
        }
    };

    const int result = Send(c, (char*)&msg, sizeof(msg), NULL, 0);
    if (result < 0)
        ABORT("[%s] Failed to send turnout delay (%d)", ctxt->name, result);
}

static inline void master_simulate_pathing(master* const ctxt) {

    const path_node* const curr_step =
        &ctxt->path[ctxt->path_steps];
    const path_node* const next_step =
        &ctxt->path[MAX(ctxt->path_steps - 1, 0)];
    const path_node* const following_step =
        &ctxt->path[MAX(ctxt->path_steps - 2, 0)];


    // it is in the next sensor range, so calculate how long to wait
    log ("[%s]\tdist: %d\tturn: %d", ctxt->name, next_step->dist,
         ctxt->path[ctxt->turnout_step].dist);

    while (ctxt->turnout_step > 0) {
        const int turn_dist  = ctxt->path[ctxt->turnout_step].dist;
        const int turn_point = turn_dist - TURNOUT_DISTANCE; 
            
        if (next_step->dist < turn_point && following_step->dist < turn_point)
            break;

        const path_node* const node = &ctxt->path[ctxt->turnout_step];
        const int danger_zone = turn_point - curr_step->dist;

        const int delay = danger_zone / ctxt->checkpoint_velocity;

        master_delay_flip_turnout(ctxt,
                                  node->data.turnout.num,
                                  node->data.turnout.dir,
                                  ctxt->checkpoint_time + delay);

        master_calculate_turnout_point(ctxt);
    }

    // it is in the next sensor range, so calculate how long to wait
    if (next_step->dist == ctxt->path[0].dist ||
           following_step->dist == ctxt->path[0].dist) {

        const path_node* const node = &ctxt->path[0];

        const int danger_zone =
            (node->dist - ctxt->checkpoint_stopping_distance) -
            curr_step->dist;
        const int delay = danger_zone / ctxt->checkpoint_velocity;

        master_delay_stop(ctxt, delay);
        log("Gonna stop!");
        ctxt->path_steps = -1;
    }
}

static inline void
master_check_sensor_to_stop_at(master* const ctxt) {

    if (!(ctxt->sensor_to_stop_at == ctxt->checkpoint &&
          ctxt->checkpoint_type == EVENT_SENSOR)) return;

    // TODO: send this directly to blaster
    train_set_speed(ctxt->train_gid, 0);

    ctxt->sensor_to_stop_at = -1;
    const sensor s = pos_to_sensor(ctxt->checkpoint);
    log("[%s] Hit sensor %c%d at %d. Stopping!",
        ctxt->name, s.bank, s.num, ctxt->checkpoint_time);
}

static inline void master_location_update(master* const ctxt,
                                          const master_req* const req,
                                          const blaster_req* const pkg,
                                          const int tid) {

    // log("[%s] Got position update", ctxt->name);

    ctxt->checkpoint_type              = req->arg1;
    ctxt->checkpoint                   = req->arg2;
    ctxt->checkpoint_offset            = req->arg3;
    ctxt->checkpoint_time              = req->arg4;
    ctxt->checkpoint_velocity          = req->arg5;
    ctxt->checkpoint_stopping_distance = req->arg6;

    const int result = Reply(tid, (char*)pkg, sizeof(blaster_req));
    UNUSED(result);
    assert(result == 0,
           "[%s] Failed to send courier back to blaster (%d)",
           ctxt->name, result);

    master_check_sensor_to_stop_at(ctxt);

    if (ctxt->path_steps >= 0) {
        if (master_try_fast_forward(ctxt)) {
            master_simulate_pathing(ctxt);
        }
        else { // shit, we need a new path...
            log("[%s] Finding a new route!", ctxt->name);
            master_goto(ctxt, ctxt->destination, ctxt->destination_offset);
        }
    }
}

static inline void
master_flip_turnout(master* const ctxt,
                    const int turn,
                    const int direction,
                    const int tid) {

    update_turnout(turn, direction);
    log("[%s] Threw Turnout %d %c", ctxt->name, turn, direction);

    const int result = Reply(tid, NULL, 0); // kill it with fire (prejudice)
    assert(result == 0,
           "[%s] Failed to kill delay notifier (%d)",
           ctxt->name, result);
    UNUSED(result);
    UNUSED(ctxt);
}

static inline void
master_stop_train(master* const ctxt, const int tid) {

    log("Stopping!");
    // TODO: courier this directly to the train?
    train_set_speed(ctxt->train_gid, 0);

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

    ctxt->path_admin = WhoIs((char*)PATH_ADMIN_NAME);
    assert(ctxt->path_admin >= 0,
           "[%s] I started up before the path admin! (%d)",
           ctxt->name, ctxt->path_admin);

    ctxt->path_worker       = -1;
    ctxt->destination       = -1;
    ctxt->path_steps        = -1;
    ctxt->sensor_to_stop_at = -1;
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
        UNUSED(result);
        assert(result > 0,
               "[%s] Invalid message size (%d)", context.name, result);

        switch (req.type) {
        case MASTER_GOTO_LOCATION:
            master_goto_command(&context, &req, &control_callin, tid);
            break;

        case MASTER_STOP_AT_SENSOR:
            master_stop_at_sensor(&context, &control_callin, req.arg1, tid);
            break;

        case MASTER_WHERE_ARE_YOU:
            master_where_are_you(&context, &control_callin, req.arg1, tid);
            break;

        case MASTER_PATH_DATA:
            master_path_update(&context, req.arg1, (path_node*)req.arg2, tid);
            break;

        case MASTER_BLASTER_LOCATION:
            master_location_update(&context, &req, &blaster_callin, tid);
            break;

        case MASTER_FLIP_TURNOUT:
            master_flip_turnout(&context, req.arg1, req.arg2, tid);
            break;

        case MASTER_STOP_TRAIN:
            master_stop_train(&context, tid);
            break;
        }
    }
}
