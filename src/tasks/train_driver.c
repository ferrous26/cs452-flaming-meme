#include <std.h>
#include <train.h>
#include <debug.h>
#include <ui.h>

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

/* Path finding crap */

#define MAX_PATH_COMMANDS 32

typedef enum {
    SENSOR_COMMAND,
    REVERSE_COMMAND,
    TURNOUT_COMMAND
} command_type;

typedef struct {
    int sensor;
    int dist; // distance to next sensor
} sensor_command;

typedef struct {
    int distance; // offset from previous location
    int reserved; // crap space
} reverse_command;

typedef struct {
    int number;
    int direction;
} turnout_command;

typedef union {
    sensor_command  sensor;
    reverse_command reverse;
    turnout_command turnout;
} command_body;

typedef struct {
    command_type type;
    command_body data;
} command;

typedef struct {
    int count;
    int dist_total;
    command cmd[MAX_PATH_COMMANDS];
} path;

/* End of path finding crap */

#define INITIAL_FEEDBACK_THRESHOLD 50

typedef struct {
    const int num;
    const int off;

    char name[8];

    int  velocity[TRACK_TYPE_COUNT][TRAIN_PRACTICAL_SPEEDS];
    int  stopping_slope;
    int  stopping_offset;
    int  feedback_threshold;
    feedback_level feedback_alpha;

    int  speed;
    int  speed_last;

    int  light;
    int  horn;
    int  direction;

    int  acceleration_last; // last time speed was changed

    int  dist_last;
    int  dist_next;

    int  time_last;         // time last sensor was hit
    int  time_next_estim;   // estimated time to next sensor

    int  sensor_last;
    int  sensor_next;
    int  sensor_next_estim;
    int  sensor_stop;       // from an ss command

    bool pathing;
    path path;
} train_context;

/* PHYSICS LAND */

static inline int __attribute__ ((pure))
velocity_for_speed(const train_context* const ctxt) {
    if (ctxt->speed == 0) return 0;

    return ctxt->
        velocity[TRACK_STRAIGHT][ctxt->speed - TRAIN_PRACTICAL_MIN_SPEED];
}

static void velocity_feedback(train_context* const ctxt,
                              const int time,
                              const int delta) {

    if (delta > ctxt->feedback_threshold) {
        log("Feedback is off by too much (%d) (%d). I suspect foul play!",
            delta, ctxt->feedback_threshold);
        return;
    }

    const int old_speed = velocity_for_speed(ctxt);
    if (old_speed == 0) return;

    const int new_speed = ctxt->dist_last / time;
    if (new_speed == 0) return;

    switch (ctxt->feedback_alpha) {
    case HALF_AND_HALF:
        ctxt->velocity[TRACK_STRAIGHT][ctxt->speed - TRAIN_PRACTICAL_MIN_SPEED] =
            (old_speed + new_speed) >> 1;
        break;
    case EIGHTY_TWENTY:
        ctxt->velocity[TRACK_STRAIGHT][ctxt->speed - TRAIN_PRACTICAL_MIN_SPEED] =
            ((old_speed << 2) + new_speed) / 5;
        break;
    case NINTY_TEN:
        ctxt->velocity[TRACK_STRAIGHT][ctxt->speed - TRAIN_PRACTICAL_MIN_SPEED] =
            ((old_speed * 9) + new_speed) / 10;
        break;
    }
}

static inline int __attribute__ ((const))
stopping_distance(const train_context* const ctxt) {
    return (ctxt->stopping_slope * ctxt->speed) + ctxt->stopping_offset;
}

static void td_update_threshold(train_context* const ctxt, int threshold) {
    ctxt->feedback_threshold = threshold;
}

static void td_update_alpha(train_context* const ctxt, int alpha) {
    ctxt->feedback_alpha = (feedback_level)alpha;
}

/* END OF PHYSICS LAND */


static void td_reset_train(train_context* const ctxt) __attribute__((unused));

static inline char __attribute__((always_inline))
make_speed_cmd(const train_context* const ctxt) {
    return (ctxt->speed & 0xf) | (ctxt->light & 0x10);
}

static inline sensor_name __attribute__ ((const))
sensornum_to_name(const int num) {
    sensor_name name = {
        .bank = 'A' + (num >>4),
        .num  = (num & 15) + 1
    };
    return name;
}

static inline void tr_setup_physics(train_context* const ctxt) {
    switch (ctxt->off) {
    case 0:
        ctxt->velocity[TRACK_STRAIGHT][0] = 0;
        ctxt->velocity[TRACK_COMBO][0]    = 0;
        ctxt->velocity[TRACK_CURVED][0]   = 0;
        ctxt->velocity[TRACK_STRAIGHT][1] = 0;
        ctxt->velocity[TRACK_COMBO][1]    = 0;
        ctxt->velocity[TRACK_CURVED][1]   = 0;
        ctxt->velocity[TRACK_STRAIGHT][2] = 0;
        ctxt->velocity[TRACK_COMBO][2]    = 0;
        ctxt->velocity[TRACK_CURVED][2]   = 0;
        ctxt->velocity[TRACK_STRAIGHT][3] = 0;
        ctxt->velocity[TRACK_COMBO][3]    = 0;
        ctxt->velocity[TRACK_CURVED][3]   = 0;
        ctxt->velocity[TRACK_STRAIGHT][4] = 0;
        ctxt->velocity[TRACK_COMBO][4]    = 0;
        ctxt->velocity[TRACK_CURVED][4]   = 0;
        ctxt->velocity[TRACK_STRAIGHT][5] = 0;
        ctxt->velocity[TRACK_COMBO][5]    = 0;
        ctxt->velocity[TRACK_CURVED][5]   = 0;
        ctxt->velocity[TRACK_STRAIGHT][6] = 0;
        ctxt->velocity[TRACK_COMBO][6]    = 0;
        ctxt->velocity[TRACK_CURVED][6]   = 0;
        break;
    case 1:
        ctxt->velocity[TRACK_STRAIGHT][0] = 2826;
        ctxt->velocity[TRACK_COMBO][0]    = 2826;
        ctxt->velocity[TRACK_CURVED][0]   = 2826;
        ctxt->velocity[TRACK_STRAIGHT][1] = 3441;
        ctxt->velocity[TRACK_COMBO][1]    = 3441;
        ctxt->velocity[TRACK_CURVED][1]   = 3441;
        ctxt->velocity[TRACK_STRAIGHT][2] = 3790;
        ctxt->velocity[TRACK_COMBO][2]    = 3790;
        ctxt->velocity[TRACK_CURVED][2]   = 3790;
        ctxt->velocity[TRACK_STRAIGHT][3] = 4212;
        ctxt->velocity[TRACK_COMBO][3]    = 4212;
        ctxt->velocity[TRACK_CURVED][3]   = 4212;
        ctxt->velocity[TRACK_STRAIGHT][4] = 4570;
        ctxt->velocity[TRACK_COMBO][4]    = 4570;
        ctxt->velocity[TRACK_CURVED][4]   = 4570;
        ctxt->velocity[TRACK_STRAIGHT][5] = 4994;
        ctxt->velocity[TRACK_COMBO][5]    = 4994;
        ctxt->velocity[TRACK_CURVED][5]   = 4994;
        ctxt->velocity[TRACK_STRAIGHT][6] = 4869;
        ctxt->velocity[TRACK_COMBO][6]    = 4869;
        ctxt->velocity[TRACK_CURVED][6]   = 4869;
        break;
    case 2:
        ctxt->velocity[TRACK_STRAIGHT][0] = 3488;
        ctxt->velocity[TRACK_COMBO][0]    = 3488;
        ctxt->velocity[TRACK_CURVED][0]   = 3488;
        ctxt->velocity[TRACK_STRAIGHT][1] = 3621;
        ctxt->velocity[TRACK_COMBO][1]    = 3621;
        ctxt->velocity[TRACK_CURVED][1]   = 3621;
        ctxt->velocity[TRACK_STRAIGHT][2] = 4250;
        ctxt->velocity[TRACK_COMBO][2]    = 4250;
        ctxt->velocity[TRACK_CURVED][2]   = 4250;
        ctxt->velocity[TRACK_STRAIGHT][3] = 4661;
        ctxt->velocity[TRACK_COMBO][3]    = 4661;
        ctxt->velocity[TRACK_CURVED][3]   = 4661;
        ctxt->velocity[TRACK_STRAIGHT][4] = 5025;
        ctxt->velocity[TRACK_COMBO][4]    = 5025;
        ctxt->velocity[TRACK_CURVED][4]   = 5025;
        ctxt->velocity[TRACK_STRAIGHT][5] = 5249;
        ctxt->velocity[TRACK_COMBO][5]    = 5249;
        ctxt->velocity[TRACK_CURVED][5]   = 5249;
        ctxt->velocity[TRACK_STRAIGHT][6] = 5276;
        ctxt->velocity[TRACK_COMBO][6]    = 5276;
        ctxt->velocity[TRACK_CURVED][6]   = 5276;
        break;
    case 3:
        ctxt->velocity[TRACK_STRAIGHT][0] = 3347;
        ctxt->velocity[TRACK_COMBO][0]    = 3347;
        ctxt->velocity[TRACK_CURVED][0]   = 3347;
        ctxt->velocity[TRACK_STRAIGHT][1] = 3748;
        ctxt->velocity[TRACK_COMBO][1]    = 3748;
        ctxt->velocity[TRACK_CURVED][1]   = 3748;
        ctxt->velocity[TRACK_STRAIGHT][2] = 4116;
        ctxt->velocity[TRACK_COMBO][2]    = 4116;
        ctxt->velocity[TRACK_CURVED][2]   = 4116;
        ctxt->velocity[TRACK_STRAIGHT][3] = 4597;
        ctxt->velocity[TRACK_COMBO][3]    = 4597;
        ctxt->velocity[TRACK_CURVED][3]   = 4597;
        ctxt->velocity[TRACK_STRAIGHT][4] = 5060;
        ctxt->velocity[TRACK_COMBO][4]    = 5060;
        ctxt->velocity[TRACK_CURVED][4]   = 5060;
        ctxt->velocity[TRACK_STRAIGHT][5] = 5161;
        ctxt->velocity[TRACK_COMBO][5]    = 5161;
        ctxt->velocity[TRACK_CURVED][5]   = 5161;
        ctxt->velocity[TRACK_STRAIGHT][6] = 5044;
        ctxt->velocity[TRACK_COMBO][6]    = 5044;
        ctxt->velocity[TRACK_CURVED][6]   = 5044;
        break;
    case 4:
        ctxt->velocity[TRACK_STRAIGHT][0] = 3966;
        ctxt->velocity[TRACK_COMBO][0]    = 3966;
        ctxt->velocity[TRACK_CURVED][0]   = 3966;
        ctxt->velocity[TRACK_STRAIGHT][1] = 4512;
        ctxt->velocity[TRACK_COMBO][1]    = 4512;
        ctxt->velocity[TRACK_CURVED][1]   = 4512;
        ctxt->velocity[TRACK_STRAIGHT][2] = 4913;
        ctxt->velocity[TRACK_COMBO][2]    = 4913;
        ctxt->velocity[TRACK_CURVED][2]   = 4913;
        ctxt->velocity[TRACK_STRAIGHT][3] = 5535;
        ctxt->velocity[TRACK_COMBO][3]    = 5535;
        ctxt->velocity[TRACK_CURVED][3]   = 5535;
        ctxt->velocity[TRACK_STRAIGHT][4] = 5718;
        ctxt->velocity[TRACK_COMBO][4]    = 5718;
        ctxt->velocity[TRACK_CURVED][4]   = 5718;
        ctxt->velocity[TRACK_STRAIGHT][5] = 5472;
        ctxt->velocity[TRACK_COMBO][5]    = 5472;
        ctxt->velocity[TRACK_CURVED][5]   = 5472;
        ctxt->velocity[TRACK_STRAIGHT][6] = 5733;
        ctxt->velocity[TRACK_COMBO][6]    = 5733;
        ctxt->velocity[TRACK_CURVED][6]   = 5733;
        break;
    case 5:
        ctxt->velocity[TRACK_STRAIGHT][0] = 3262;
        ctxt->velocity[TRACK_COMBO][0]    = 3262;
        ctxt->velocity[TRACK_CURVED][0]   = 3262;
        ctxt->velocity[TRACK_STRAIGHT][1] = 3758;
        ctxt->velocity[TRACK_COMBO][1]    = 3758;
        ctxt->velocity[TRACK_CURVED][1]   = 3758;
        ctxt->velocity[TRACK_STRAIGHT][2] = 4265;
        ctxt->velocity[TRACK_COMBO][2]    = 4265;
        ctxt->velocity[TRACK_CURVED][2]   = 4265;
        ctxt->velocity[TRACK_STRAIGHT][3] = 4762;
        ctxt->velocity[TRACK_COMBO][3]    = 4762;
        ctxt->velocity[TRACK_CURVED][3]   = 4762;
        ctxt->velocity[TRACK_STRAIGHT][4] = 5179;
        ctxt->velocity[TRACK_COMBO][4]    = 5179;
        ctxt->velocity[TRACK_CURVED][4]   = 5179;
        ctxt->velocity[TRACK_STRAIGHT][5] = 5608;
        ctxt->velocity[TRACK_COMBO][5]    = 5608;
        ctxt->velocity[TRACK_CURVED][5]   = 5608;
        ctxt->velocity[TRACK_STRAIGHT][6] = 5231;
        ctxt->velocity[TRACK_COMBO][6]    = 5231;
        ctxt->velocity[TRACK_CURVED][6]   = 5231;
        break;
    case 6:
        ctxt->velocity[TRACK_STRAIGHT][0] = 1732;
        ctxt->velocity[TRACK_COMBO][0]    = 1732;
        ctxt->velocity[TRACK_CURVED][0]   = 1732;
        ctxt->velocity[TRACK_STRAIGHT][1] = 2028;
        ctxt->velocity[TRACK_COMBO][1]    = 2028;
        ctxt->velocity[TRACK_CURVED][1]   = 2028;
        ctxt->velocity[TRACK_STRAIGHT][2] = 2429;
        ctxt->velocity[TRACK_COMBO][2]    = 2429;
        ctxt->velocity[TRACK_CURVED][2]   = 2429;
        ctxt->velocity[TRACK_STRAIGHT][3] = 2995;
        ctxt->velocity[TRACK_COMBO][3]    = 2995;
        ctxt->velocity[TRACK_CURVED][3]   = 2995;
        ctxt->velocity[TRACK_STRAIGHT][4] = 3676;
        ctxt->velocity[TRACK_COMBO][4]    = 3676;
        ctxt->velocity[TRACK_CURVED][4]   = 3676;
        ctxt->velocity[TRACK_STRAIGHT][5] = 4584;
        ctxt->velocity[TRACK_COMBO][5]    = 4584;
        ctxt->velocity[TRACK_CURVED][5]   = 4584;
        ctxt->velocity[TRACK_STRAIGHT][6] = 5864;
        ctxt->velocity[TRACK_COMBO][6]    = 5864;
        ctxt->velocity[TRACK_CURVED][6]   = 5864;
        break;
    }

    switch (ctxt->off) {
    case 0:
        ctxt->stopping_slope  = 0;
        ctxt->stopping_offset = 0;
        break;
    case 1:
        ctxt->stopping_slope  =  63827;
        ctxt->stopping_offset = -42353;
        break;
    case 2:
        ctxt->stopping_slope  =  67029;
        ctxt->stopping_offset = -38400;
        break;
    case 3:
        ctxt->stopping_slope  =  73574;
        ctxt->stopping_offset = -59043;
        break;
    case 4:
        ctxt->stopping_slope  =  63905;
        ctxt->stopping_offset = -41611;
        break;
    case 5:
        ctxt->stopping_slope  =  63905;
        ctxt->stopping_offset = -41611;
        break;
    case 6:
        ctxt->stopping_slope  = 0;
        ctxt->stopping_offset = 0;
        break;
    }

    ctxt->feedback_threshold = INITIAL_FEEDBACK_THRESHOLD;
    ctxt->feedback_alpha     = EIGHTY_TWENTY;
    log("yo %d %d", ctxt->feedback_threshold, ctxt->feedback_alpha);
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

static inline void td_update_ui_speed(train_context* const ctxt) {
    // TODO: this should update based on the velocity estimate
    //       and not the exact mapping; but is not important until
    //       we factor in acceleration

    // NOTE: we currently display in units of (integer) rounded off mm/s
    char  buffer[16];
    char* ptr      = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_SPEED_COL);
    ptr            = sprintf(ptr,
                             ctxt->speed && ctxt->speed < 15 ? "%d " : "- ",
                             velocity_for_speed(ctxt->off, ctxt->speed) / 10);
    Puts(buffer, ptr - buffer);
}

static void td_update_train_speed(train_context* const ctxt,
                                  const int new_speed) {

    ctxt->speed        = new_speed & 0xF;
    put_train_cmd((char)ctxt->num, make_speed_cmd(ctxt));
    ctxt->accelerating = Time(); // TEMPORARY
    td_update_ui_speed(ctxt);
}

static void td_toggle_light(train_context* const ctxt) {

    ctxt->light ^= -1;
    put_train_cmd((char)ctxt->num, make_speed_cmd(ctxt));

    char  buffer[32];
    char* ptr    = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_LIGHT_COL);

    if (ctxt->light)
        ptr      = sprintf_string(ptr, COLOUR(YELLOW) "L" COLOUR_RESET);
    else
        *(ptr++) = ' ';

    Puts(buffer, ptr-buffer);
}

static void td_toggle_horn(train_context* const ctxt) {

    ctxt->horn ^= -1;
    const char horn_cmd = ctxt->horn ? TRAIN_HORN : TRAIN_FUNCTION_OFF;
    put_train_cmd((char) ctxt->num, horn_cmd);

    char  buffer[32];
    char* ptr   = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_HORN_COL);

    if (ctxt->horn)
        ptr      = sprintf_string(ptr, COLOUR(RED) "H" COLOUR_RESET);
    else
        *(ptr++) = ' ';

    Puts(buffer, ptr-buffer);
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
        case TRAIN_STOP:
        case TRAIN_HIT_SENSOR:
        case TRAIN_WHERE_ARE_YOU:
        case TRAIN_REQUEST_COUNT:
        case TRAIN_GO_TO:
        case TRAIN_EXPECTED_SENSOR:
            ABORT("She's moving when we dont tell her too captain!");
            break;
        }
    }
}

void train_driver() {
    int           tid;
    train_req     req;
    train_context context;

    // TEMPORARY
            path g = {
                .count      = 1,
                .dist_total = 738000
            };
            g.cmd[1].type                = SENSOR_COMMAND;
            g.cmd[1].data.sensor.sensor  = 44;
            g.cmd[1].data.sensor.dist    = 67000;

            g.cmd[0].type                = SENSOR_COMMAND;
            g.cmd[0].data.sensor.sensor  = 70;
            g.cmd[0].data.sensor.dist    = 783000;

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

    td_update_train_direction(&context, 1);

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

        int time = Time();

        switch (req.type) {
        case TRAIN_CHANGE_SPEED:
            td_update_train_speed(&context, req.one.int_value);
            break;
        case TRAIN_REVERSE_DIRECTION: {
            int old_speed = context.speed;

            td_update_train_speed(&context, 0);
            Delay(old_speed * 40); // TODO: use acceleration function
            td_update_train_speed(&context, 15);
            Delay(2);
            td_update_train_direction(&context, -context.direction);
            td_update_train_speed(&context, old_speed);

            break;
        }
        case TRAIN_TOGGLE_LIGHT:
            td_toggle_light(&context);
            break;
        case TRAIN_HORN_SOUND:
            td_toggle_horn(&context);
            break;
        case TRAIN_STOP:
            context.sensor_stop = req.one.int_value;
            break;

        case TRAIN_HIT_SENSOR: {
            context.dist_last   = context.dist_next;
            context.time_last   = time;
            context.sensor_last = req.one.int_value;

            result = get_sensor_from(req.one.int_value,
                                     &context.dist_next,
                                     &context.sensor_next);
            assert(result >= 0, "failed to get next sensor");

            const sensor_name last = sensornum_to_name(context.sensor_last);
            const sensor_name next = sensornum_to_name(context.sensor_next);
            log("[Train%d] Hit unexpected sensor %c%d, "
                "next expected sensor is %c%d",
                context.num,
                last.bank, last.num,
                next.bank, next.num);

            const int velocity = velocity_for_speed(context.off,
                                                    context.speed);
            context.sensor_next_estim = context.dist_next / velocity;

            if (context.pathing) {
                context.pathing = false;
                log("Aborted path finding...fix me to re-route");
            }

            Reply(tid,
                  (char*)&context.sensor_next,
                  sizeof(context.sensor_next));
            continue;
        }
        case TRAIN_EXPECTED_SENSOR: {
            if (context.sensor_next == context.sensor_stop) {
                td_update_train_speed(&context, 0);
                context.sensor_stop = -1;
            }

            const int expected = context.time_last + context.sensor_next_estim;
            const int actual   = time - context.time_last;
            const int delta    = actual > context.sensor_next_estim ?
                  actual - context.sensor_next_estim :
                -(actual - context.sensor_next_estim);

            // TODO: this should be a function of acceleration and not a
            //       constant value
            if (time - context.accelerating > 500) {
                update_velocity_for_speed(context.off,
                                          context.speed,
                                          context.dist_last,
                                          actual,
                                          delta);
                td_update_ui_speed(&context);
            }

            // TODO: move this block of code out and make it more efficient
            {
                const sensor_name last =
                    sensornum_to_name(context.sensor_last);
                const sensor_name next =
                    sensornum_to_name(context.sensor_next);

                char buffer[128];
                char* ptr = log_start(buffer);
                ptr = sprintf(ptr, "[Train%d] %c%d",
                              context.num, last.bank, last.num);
                if (last.num < 10)
                    *(ptr++) = ' ';
                ptr = sprintf(ptr, "-> %c%d", next.bank, next.num);
                if (next.num < 10)
                    *(ptr++) = ' ';
                ptr = sprintf(ptr,
                              " ETA: %d\tTA:%d\tDelta: %d",
                              expected, time, delta);
                ptr = log_end(ptr);
                Puts(buffer, ptr-buffer);
            }

            context.dist_last   = context.dist_next;
            context.time_last   = time;
            context.sensor_last = context.sensor_next;
            result = get_sensor_from(context.sensor_last,
                                     &context.dist_last,
                                     &context.sensor_next);

            const int velocity = velocity_for_speed(context.off,
                                                    context.speed);
            context.sensor_next_estim = context.dist_last / velocity;

            Reply(tid,
                  (char*)&context.sensor_next,
                  sizeof(context.sensor_next));


            // the path info comes into play here
            if (context.pathing &&
                context.path.cmd[context.path.count].data.sensor.sensor == context.sensor_last) {
                log("pathing");

                // is this the next sensor in the path?
                context.path.dist_total -=
                    context.path.cmd[context.path.count].data.sensor.dist;

                const int stop_dist =
                    stopping_distance_for_speed(context.off, context.speed);

                if (context.path.dist_total <= stop_dist) {
                    const int dist_until_stop =
                        context.path.cmd[context.path.count].data.sensor.dist -
                        (stop_dist - context.path.dist_total);
                    const int delay_time = dist_until_stop / velocity;
                    Delay(delay_time);
                    td_update_train_speed(&context, 0);
                }

                if (context.path.count == 0) {
                    context.pathing = false;
                }
                else {
                    do {
                        context.path.count--;
                    } while (context.path.cmd[context.path.count].type
                             != SENSOR_COMMAND);
                }
            }

            continue;
        }

        case TRAIN_WHERE_ARE_YOU: {
            // TODO: this will be more complicated when we have acceleration
            const sensor_name last = sensornum_to_name(context.sensor_last);
            const     int velocity = velocity_for_speed(context.off,
                                                        context.speed);
            const         int dist = velocity * (time - context.time_last);
            log("[TRAIN%d] %c%d + %d mm",
                context.num, last.bank, last.num, dist / 1000);
            break;
        }

        case TRAIN_GO_TO: {
            context.pathing = true;

            // assume that we just got g as the path
            memcpy(&context.path, &g, sizeof(path));

            // TEMP HACK
            for (int i = context.path.count; i; i--) {
                if (context.path.cmd[i].type == TURNOUT_COMMAND) {
                    const turnout_command* const t =
                        &context.path.cmd[i].data.turnout;
                    update_turnout(t->number, t->direction);
                }
            }
            break;
        }

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
