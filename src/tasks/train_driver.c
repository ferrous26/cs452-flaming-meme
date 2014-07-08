#include <std.h>
#include <train.h>
#include <debug.h>
#include <ui.h>
#include <track_node.h>
#include <path.h>
#include <dijkstra.h>

#include <tasks/term_server.h>
#include <tasks/train_server.h>

#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

#include <tasks/courier.h>
#include <tasks/train_console.h>
#include <tasks/train_control.h>
#include <tasks/train_driver.h>

#include <tasks/train_driver_types.h>
#include <physics.h> // this should be included last


static void td_reset_train(train_context* const ctxt) __attribute__((unused));

static inline char __attribute__((const, always_inline))
make_speed_cmd(const train_context* const ctxt) {
    return (ctxt->speed & 0xf) | (ctxt->light & 0x10);
}

static int recalculate_stopping_point(train_context* const ctxt) {
    // calculate where we need to stop
    const int total_dist =
        ctxt->path_dist     +
        ctxt->path_past_end +
        ctxt->stop_offset;
    const int  stop_dist = stopping_distance(ctxt);

    // TODO: fix this
    ctxt->stopping_point = total_dist - stop_dist;
    if (ctxt->stopping_point <= STOPPING_DISTANCE_THRESHOLD) {
        log("[Train%d] Not enough time to stop. Aborting path! %d - %d = %d",
            ctxt->num, total_dist, stop_dist, ctxt->stopping_point);
        ctxt->path = -1;
        return 1;
    }

    return 0;
}

static void tr_setup(train_context* const ctxt) {
    int tid;
    int values[2];

    memset(ctxt, 0, sizeof(train_context));
    int result = Receive(&tid, (char*)&values, sizeof(values));
    if (result != sizeof(values))
        ABORT("Train Recived invalid statup data (%d)", result);

    // Only place these should get set
    *((int*)&ctxt->num) = values[0];
    *((int*)&ctxt->off) = values[1];

    assert(ctxt->num > 0 && ctxt->num < 80,
           "Bad Train Number Passed to driver task %d", myTid());

    sprintf(ctxt->name, "TRAIN%d", ctxt->num);
    ctxt->name[7] = '\0';

    if (RegisterAs(ctxt->name))
        ABORT("Failed to register train %d", ctxt->num);

    ctxt->path = -1;
    tr_setup_physics(ctxt);

    Reply(tid, NULL, 0);
}

static void td_update_train_direction(train_context* const ctxt, int dir) {
    char  buffer[16];
    char* ptr = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_SPEED_COL+4);

    char               printout = 'F';
    if (dir < 0)       printout = 'B';
    else if (dir == 0) printout = ' ';

    *(ptr++)        = printout;
    ctxt->direction = dir;
    Puts(buffer, ptr-buffer);
}

static void td_update_train_delta(train_context* const ctxt, const int delta) {
    char  buffer[32];
    char* ptr = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_SENSORS_COL);

    const sensor_name last = sensornum_to_name(ctxt->sensor_last);
    const sensor_name next = sensornum_to_name(ctxt->sensor_next);

    ptr = sprintf_int(ptr, delta);
    ptr = ui_pad(ptr, log10(abs(delta)) + (delta < 0 ? 1 : 0), 5);

    if (last.num < 10)
        ptr = sprintf(ptr, "%c0%d ", last.bank, last.num);
    else
        ptr = sprintf(ptr, "%c%d ", last.bank, last.num);

    // extra trailing white space is intentional in case delta was teh huge
    if (next.num < 10)
        ptr = sprintf(ptr, "%c0%d  ", next.bank, next.num);
    else
        ptr = sprintf(ptr, "%c%d  ", next.bank, next.num);

    Puts(buffer, ptr-buffer);
}

static inline void td_update_ui_speed(train_context* const ctxt) {
    // TODO: this should update based on the velocity estimate
    //       and not the exact mapping; but is not important until
    //       we factor in acceleration

    // NOTE: we currently display in units of (integer) rounded off mm/s
    char  buffer[16];
    char* ptr      = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_SPEED_COL);
    ptr            = sprintf(ptr,
                             ctxt->speed && ctxt->speed < 15 ? "%d " : "-  ",
                             velocity_for_speed(ctxt) / 10);
    Puts(buffer, ptr - buffer);
}

static void td_update_train_speed(train_context* const ctxt,
                                  const int new_speed) {

    ctxt->speed_last   = ctxt->speed;
    ctxt->speed        = new_speed & 0xF;
    put_train_cmd((char)ctxt->num, make_speed_cmd(ctxt));
    ctxt->acceleration_last = Time(); // TEMPORARY
    if (ctxt->path >= 0 && new_speed) {
        if (recalculate_stopping_point(ctxt))
            td_update_train_speed(ctxt, 0);
    }
    td_update_ui_speed(ctxt);
}

static void td_toggle_light(train_context* const ctxt) {
    ctxt->light ^= -1;
    put_train_cmd((char)ctxt->num, make_speed_cmd(ctxt));
}

static void td_toggle_horn(train_context* const ctxt) {
    ctxt->horn ^= -1;
    put_train_cmd((char) ctxt->num,
                  ctxt->horn ? TRAIN_HORN : TRAIN_FUNCTION_OFF);
}

static void td_reverse_direction(train_context* const ctxt) {
    td_update_train_speed(ctxt, 0); // STAHP!
    ctxt->reversing = true;

    // TODO: do not use magic numbers for priorities
    ctxt->courier = Create(6, time_notifier);
    if (ctxt->courier < 0) {
        log("Failed to create delay courier for reverse (%d)",
            ctxt->courier);
        return;
    }

    struct {
        tnotify_header head;
        train_req      req;
    } msg = {
        .head = {
            .type  = DELAY_RELATIVE,
            // TODO: use acceleration function to determine this
            .ticks = ctxt->speed_last * 40
        },
        .req = {
            .type = TRAIN_REVERSE_COURIER
        }
    };

    int result = Send(ctxt->courier,
                      (char*)&msg, sizeof(msg),
                      NULL, 0);
    if (result < 0)
        ABORT("Failed to send delay for reverse (%d)", result);
}

static void td_reverse(train_context* const ctxt) {
    struct {
        tnotify_header head;
        train_req      req;
    } msg = {
        .head = {
            .type  = DELAY_RELATIVE,
            .ticks = 2
        },
        .req = {
            .type = TRAIN_REVERSE_COURIER_FINAL,
            .one.int_value = ctxt->speed_last
        }
    };

    td_update_train_speed(ctxt, TRAIN_REVERSE);

    int result = Reply(ctxt->courier, (char*)&msg, sizeof(msg));
    if (result < 0) ABORT("Failed to minor delay reverse (%d)", result);
}

static void td_reverse_final(train_context* const ctxt,
                             const int speed) {

    td_update_train_direction(ctxt, -ctxt->direction);
    td_update_train_speed(ctxt, speed);
    int result = Reply(ctxt->courier, NULL, 0);
    if (result < 0) log("Failed to kill delay courier (%d)", result);
}

static void __attribute__ ((unused))
debug_path(const train_context* const ctxt) {
    for (int i = ctxt->path - 1, j = 0; i >= 0; i--, j++) {
        switch(ctxt->steps[i].type) {
        case PATH_SENSOR: {
            const sensor_name print =
                sensornum_to_name(ctxt->steps[i].data.int_value);
            log("%d\tS\t%c%d\t%d",
                j, print.bank, print.num, ctxt->steps[i].dist);
            break;
        }
        case PATH_TURNOUT:
            log("%d\tT\t%d %c\t%d",
                j,
                ctxt->steps[i].data.turnout.num,
                ctxt->steps[i].data.turnout.dir,
                ctxt->steps[i].dist);
            break;
        case PATH_REVERSE:
            log("%d\tR\t\t%d", j, ctxt->steps[i].dist);
            break;
        }
    }
}

static void td_goto_next_step(train_context* const ctxt) {
    // NOTE: since we deal with all the turnouts up front right
    //       now, we will just ignore turnout commands
    const path_node* step = &ctxt->steps[ctxt->path];
    while (ctxt->path >= 0 && step->type != PATH_SENSOR) {
        step = &ctxt->steps[--ctxt->path];
    }
}

static void td_goto(train_context* const ctxt,
                    const int sensor,
                    const int offset,
                    const int time) {

    const int node_end = ctxt->sensor_stop = sensor;

    const sensor_name s = sensornum_to_name(sensor);
    log("[Train%d] Going to %c%d + %d cm (%d)",
        ctxt->num, s.bank, s.num, offset, node_end);

    int sensor_next = ctxt->sensor_next;

    // calculate the estimated time to next sensor
    const int delta = abs(time -
                          (ctxt->sensor_next_estim + ctxt->time_last));
    // if we do not have enough time to calculate route
    if (delta > MAX_ROUTING_TIME_ESTIMATE) {
        // then calculate route from next next sensor
        int dist_next = 0;
        get_sensor_from(ctxt->sensor_last, &dist_next, &sensor_next);
    }

    ctxt->path = dijkstra(train_track,
                          train_track + sensor_next,
                          train_track + node_end,
                          ctxt->steps);

    if (ctxt->path < 0) {
        log("[Train%d] Cannot find a route!", ctxt->num);
        return;
    }

#ifdef DEBUG
    debug_path(ctxt);
#endif

    // TODO: we want to copy this bad boy over
    // memcpy(&context.path, &g, sizeof(path));

    ctxt->path--; // first index is at this new value

    // if we start with a reverse, then we can reverse now
    if (ctxt->steps[ctxt->path].type == PATH_REVERSE) {
        td_reverse_direction(ctxt);
    }
    ctxt->path--;

    // get these all in order now
    for (int i = ctxt->path; i >= 0; i--) {
        if (ctxt->steps[i].type == PATH_TURNOUT) {
            const path_turn* const turn =
                &ctxt->steps[i].data.turnout;
            update_turnout(turn->num, turn->dir);
        }
    }

    // figure out how far past the sensor we need to move
    ctxt->path_past_end = offset * 10000; // convert to umeters
    // if we are reversing at the end, then the offset
    // is actually a negative offset from the new direction
    if (ctxt->steps[1].type == PATH_REVERSE)
        ctxt->path_past_end = -ctxt->path_past_end;

    // copy the total distance
    ctxt->path_dist = ctxt->steps[0].dist;
    if (recalculate_stopping_point(ctxt)) return;

    td_goto_next_step(ctxt);
}

static void td_where(train_context* const ctxt, const int time) {
    // TODO: this will be more complicated when we have acceleration
    const sensor_name last = sensornum_to_name(ctxt->sensor_last);
    const     int velocity = velocity_for_speed(ctxt);
    const         int dist = velocity * (time - ctxt->time_last);
    log("[TRAIN%d] %c%d + %d mm",
        ctxt->num, last.bank, last.num, dist / 1000);
}

static void td_train_dump(train_context* const ctxt) {

    for (int i = 0; i < TRACK_TYPE_COUNT; i++)
        log("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            ctxt->off,
            i, // track type
            ctxt->velocity[i][0],
            ctxt->velocity[i][1],
            ctxt->velocity[i][2],
            ctxt->velocity[i][3],
            ctxt->velocity[i][4],
            ctxt->velocity[i][5],
            ctxt->velocity[i][6],
            ctxt->velocity[i][7],
            ctxt->velocity[i][8],
            ctxt->velocity[i][9],
            ctxt->velocity[i][10],
            ctxt->velocity[i][11],
            ctxt->velocity[i][12],
            ctxt->velocity[i][13]);
}

static inline void train_wait_use(train_context* const ctxt,
                                  const int train_tid,
                                  const tc_req* const callin) {
    train_req req;

    FOREVER {
        int result = Send(train_tid,
                          (char*)callin, sizeof(*callin),
                          (char*)&req,    sizeof(req));
        if (result != sizeof(req))
            ABORT("Bad train init (%d)", result);

        switch (req.type) {
        case TRAIN_CHANGE_SPEED:
            td_update_train_speed(ctxt, req.one.int_value);
            if (req.one.int_value > 0) return;
            break;
        case TRAIN_REVERSE_DIRECTION:
            td_update_train_speed(ctxt, TRAIN_REVERSE);
            Delay(2);
            td_update_train_speed(ctxt, 0);
            break;
        case TRAIN_TOGGLE_LIGHT:
            td_toggle_light(ctxt);
            break;
        case TRAIN_HORN_SOUND:
            td_toggle_horn(ctxt);
            break;
        case TRAIN_THRESHOLD:
            td_update_threshold(ctxt, req.one.int_value);
            break;
        case TRAIN_ALPHA:
            td_update_alpha(ctxt, req.one.int_value);
            break;
        case TRAIN_DUMP:
            td_train_dump(ctxt);
            break;
        case TRAIN_STOP:
            ctxt->sensor_stop = req.one.int_value;
            break;
        case TRAIN_GO_TO:
            // TODO: this should be handled here legitimately
            break;
        case TRAIN_SET_STOP_OFFSET:
            ctxt->stop_offset = req.one.int_value * 1000;
            break;

        case TRAIN_HIT_SENSOR:
        case TRAIN_WHERE_ARE_YOU:
        case TRAIN_REQUEST_COUNT:
        case TRAIN_EXPECTED_SENSOR:
        case TRAIN_REVERSE_COURIER:
        case TRAIN_REVERSE_COURIER_FINAL:
            ABORT("She's moving when we dont tell her too captain!");
        }
    }
}

static int path_fast_forward(train_context* const ctxt,
                             const int sensor) {

    for (; ctxt->path >= 0; ctxt->path--) {
        if (ctxt->steps[ctxt->path].type == PATH_SENSOR &&
            ctxt->steps[ctxt->path].data.sensor == sensor) {
            return 1;
        }
    }
    return 0;
}

void train_driver() {
    int           tid;
    train_req     req;
    train_context context;

    tr_setup(&context);

    const int train_tid = myParentTid();
    const tc_req callin = {
        .type              = TC_REQ_WORK,
        .payload.int_value = context.off
    };

    td_toggle_light(&context); // turn on lights when we initialize!
    train_wait_use(&context, train_tid, &callin);

    tid = Create(4, train_console);
    assert(tid >= 0, "Failed creating the train console (%d)", tid);

    const int command_tid         = Create(4, courier);
    const courier_package package = {
        .receiver = train_tid,
        .message  = (char*)&callin,
        .size     = sizeof(callin),
    };

    assert(command_tid >= 0,
           "Error setting up command courier (%d)", command_tid);

    int result = Send(command_tid, (char*)&package, sizeof(package), NULL, 0);
    assert(result == 0, "Error sending package to command courier %d", result);


    FOREVER {
        result = Receive(&tid, (char*)&req, sizeof(req));

        UNUSED(result);
        assert(result == sizeof(req),
               "train %d recived malformed request from %d", context.num, tid);
        assert(req.type < TRAIN_REQUEST_COUNT,
                "Train %d got bad request %d", context.num, req.type);

        const int time = req.two.int_value;

        switch (req.type) {
        case TRAIN_CHANGE_SPEED:
            td_update_train_speed(&context, req.one.int_value);
            break;

        case TRAIN_REVERSE_DIRECTION:
            td_reverse_direction(&context);
            break;
        case TRAIN_REVERSE_COURIER:
            td_reverse(&context);
            continue;
        case TRAIN_REVERSE_COURIER_FINAL:
            td_reverse_final(&context, req.one.int_value);
            continue;

        case TRAIN_TOGGLE_LIGHT:
            td_toggle_light(&context);
            break;
        case TRAIN_HORN_SOUND:
            td_toggle_horn(&context);
            break;
        case TRAIN_STOP:
            context.sensor_stop = req.one.int_value;
            break;
        case TRAIN_SET_STOP_OFFSET:
            context.stop_offset = req.one.int_value * 1000;
            break;

        case TRAIN_HIT_SENSOR: {
            if (context.direction == 0) {
                if ((req.one.int_value >> 4) == 1) {
                    td_update_train_direction(&context, 1);
                    td_reverse_direction(&context);
                    const int none = 80;
                    Reply(tid, (char*)&none, sizeof(none));
                    continue;
                }
                
                td_update_train_direction(&context, -1); 
            }
            log ("[TRAIN%d] lost position...", context.num);
            context.sensor_last = req.one.int_value;
            context.time_last   = req.two.int_value;
            context.dist_last   = context.dist_next;
                
            if (context.path >= 0) {
                if (path_fast_forward(&context, req.one.int_value)) {
                    
                    context.path--;
                    td_goto_next_step(&context);
              
                    context.dist_next   = context.steps[context.path].dist;
                    context.sensor_next = context.steps[context.path].data.sensor;
                } else {
                    context.sensor_next = req.one.int_value;
                    td_goto(&context,
                            context.sensor_stop,
                            context.path_past_end,
                            time);
                }
            } else {
                result = get_sensor_from(req.one.int_value,
                                         &context.dist_next,
                                         &context.sensor_next);
                if (context.dist_next < 0) {
                    td_reverse_direction(&context);
                }
                assert(result == 0, "failed to get next sensor");
            }
            const int velocity = velocity_for_speed(&context);
            context.sensor_next_estim = context.dist_next / velocity;

            Reply(tid,
                  (char*)&context.sensor_next,
                  sizeof(context.sensor_next));
            continue;
        }
        case TRAIN_EXPECTED_SENSOR: {
            assert(req.one.int_value == context.sensor_next,
                   "Bad Expected Sensor %d", req.one.int_value);
            
            const int expected = context.sensor_next_estim;
            const int actual   = time - context.time_last;
            const int delta    = actual - expected;

            // TODO: this should be a function of acceleration and not a
            //       constant value
            if (time - context.acceleration_last > 500) {
                velocity_feedback(&context, actual, delta);
                td_update_ui_speed(&context);
            }
            
            context.time_last   = req.two.int_value;
            context.dist_last   = context.dist_next;
            context.sensor_last = context.sensor_next;
            const int velocity  = velocity_for_speed(&context);
            context.reversing   = false;

            // the path info comes into play here
            const path_node* step = &context.steps[context.path];
            // if path finding and expected sensor is next on the path
            if (context.path > 0 && step->data.sensor == context.sensor_last) {
                do {
                    step = &context.steps[--context.path];
                } while (step->type != PATH_SENSOR);
                context.sensor_next = step->data.sensor;
                context.dist_next   = step->dist;
                const sensor_name next = sensornum_to_name(context.sensor_next);
                
                log("Next sensor %c%d is at %d mm. We need to stop at %d mm.",
                    next.bank, next.num,
                    context.dist_next / 1000,
                    context.stopping_point / 1000);

                if (context.dist_next >= context.stopping_point) {
                    const int curr_time = Time();
                    const int step_dist = context.dist_next - context.dist_last;

                    const int delay_time = curr_time + 
                        (step_dist - (context.dist_next - context.stopping_point)) /
                        velocity;
           
                    if (delay_time > curr_time) { 
                        log("Delaying %d ticks until stop at %d",
                            delay_time, context.stopping_point / 1000);
                            DelayUntil(delay_time);
                    }

                    td_update_train_speed(&context, 0);

                    //const int dist = velocity * (time - context.time_last);
                    //const sensor_name target =
                    //    sensornum_to_name(context.sensor_stop);
                    //log("[Train%d] Arriving at %c%d + %d mm",
                    //    context.num, target.bank, target.num, dist / 1000);

                    // TODO: error checking
                    context.path = -1; // kill pathing
                    get_sensor_from(context.sensor_last,
                                    &context.dist_next,
                                    &context.sensor_next);
                }
            } else {
                // TODO: error checking
                get_sensor_from(context.sensor_last,
                                &context.dist_next,
                                &context.sensor_next);
            }

            context.sensor_next_estim = context.dist_next / velocity;
            td_update_train_delta(&context, delta);

            Reply(tid,
                  (char*)&context.sensor_next,
                  sizeof(context.sensor_next));
            continue;
        }

        case TRAIN_WHERE_ARE_YOU:
            td_where(&context, time);
            break;

        case TRAIN_GO_TO:
            td_goto(&context,
                    sensorname_to_num(req.one.go.bank, req.one.go.num),
                    req.one.go.offset,
                    time);
            break;

        case TRAIN_DUMP:
            td_train_dump(&context);
            break;

        case TRAIN_THRESHOLD:
            td_update_threshold(&context, req.one.int_value);
            break;

        case TRAIN_ALPHA:
            td_update_alpha(&context, req.one.int_value);
            break;

        case TRAIN_REQUEST_COUNT:
            ABORT("[Train%d] got request count request from %d",
                  context.num, tid);
        }

        assert(tid == command_tid,
               "TRAIN %d traing to reset invlaid command courier task %d",
               context.num, tid);

        result = Reply(tid, (char*)&callin, sizeof(callin));
        if (result) ABORT("Failed responding to command courier (%d)", result);
    }
}

static void td_reset_train(train_context* const ctxt) {
    log("[TRAIN%d]\tResetting...", ctxt->num);

    if (ctxt->horn)   td_toggle_horn(ctxt);
    if (!ctxt->light) td_toggle_light(ctxt);
    td_update_train_speed(ctxt, 0);
}
