#include <std.h>
#include <train.h>
#include <debug.h>
#include <ui.h>
#include <physics.h>

#include <tasks/term_server.h>
#include <tasks/train_server.h>

#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

#include <tasks/courier.h>
#include <tasks/train_console.h>
#include <tasks/train_station.h>

typedef struct {
    const int num;
    const int off;

    char name[8];
    int  speed;
    int  light;
    int  horn;
    int  direction;

    sensor_name last;
    int         dist_last;
    int         time_last;
    int         estim_next;

    sensor_name next;
} train_context;

static void td_reset_train(train_context* const ctxt) __attribute__((unused));

static inline char __attribute__((always_inline))
make_speed_cmd(const train_context* const ctxt) {
    return (ctxt->speed & 0xf) | (ctxt->light & 0x10);
}

static void tr_setup(train_context* const ctxt) {
    int tid;
    int values[2];

    memset(ctxt,               0, sizeof(train_context));

    int result = Receive(&tid, (char*)&values, sizeof(values));
    assert(result == sizeof(values), "Train Recived invalid statup data");
    UNUSED(result);

    // Only place these should get set
    *((int*)&ctxt->num) = values[0];
    *((int*)&ctxt->off) = values[1];

    assert(ctxt->num > 0 && ctxt->num < 80,
           "Bad Train Number Passed to driver task %d", myTid());

    sprintf(ctxt->name, "TRAIN%d", ctxt->num);
    ctxt->name[7] = '\0';

    if (RegisterAs(ctxt->name)) ABORT("Failed to register train %d", ctxt->num);

    Reply(tid, NULL, 0);
}

static void td_update_train_direction(train_context* const ctxt, int dir) {
    char  buffer[16];
    char* ptr      = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_SPEED_COL+3);

    char               printout = 'F';
    if (dir < 0)       printout = 'B';
    else if (dir == 0) printout = ' ';

    *(ptr++)        = printout;
    ctxt->direction = dir;
    Puts(buffer, ptr-buffer);
}

static void td_update_train_speed(train_context* const ctxt,
                                  const int new_speed) {
    char  buffer[16];
    char* ptr      = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_SPEED_COL);
    ctxt->speed    = new_speed & 0xF;
    ptr            = sprintf(ptr, ctxt->speed ? "%d " : "- ", ctxt->speed);

    put_train_cmd((char)ctxt->num, make_speed_cmd(ctxt));
    Puts(buffer, ptr-buffer);

    // TEMPORARY
    if (new_speed < 15) {
        log("Speed of %d is %d mm/s",
            ctxt->num, velocity_for_speed(ctxt->off, new_speed) / 10);
    }
}

static void td_toggle_light(train_context* const ctxt) {
    char  buffer[32];
    char* ptr    = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_LIGHT_COL);
    ctxt->light ^= -1;

    if (ctxt->light) {
        ptr = sprintf_string(ptr, COLOUR(YELLOW) "L" COLOUR_RESET);
    } else {
        *(ptr++) = ' ';
    }

    put_train_cmd((char)ctxt->num, make_speed_cmd(ctxt));
    Puts(buffer, ptr-buffer);
}

static void td_toggle_horn(train_context* const ctxt) {
    char  horn_cmd;
    char  buffer[32];
    ctxt->horn ^= -1;
    char* ptr   = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_HORN_COL);

    if (ctxt->horn) {
        horn_cmd = TRAIN_HORN;
        ptr      = sprintf_string(ptr, COLOUR(RED) "H" COLOUR_RESET);
    } else {
        horn_cmd = TRAIN_FUNCTION_OFF;
        *(ptr++) = ' ';
    }

    put_train_cmd((char) ctxt->num, horn_cmd);
    Puts(buffer, ptr-buffer);
}

static inline void train_wait_use(train_context* const ctxt,
                                  const int train_tid,
                                  const mc_req* const callin) {
    train_req req;

    FOREVER {
        int result = Send(train_tid,
                          (char*)callin, sizeof(*callin),
                          (char*)&req,    sizeof(req));
        assert(result == sizeof(req), "Bad train init");
        UNUSED(result);

        switch (req.type) {
        case TRAIN_CHANGE_SPEED:
            td_update_train_speed(ctxt, req.one.int_value);
            if (req.one.int_value > 0) return;
            break;
        case TRAIN_REVERSE_DIRECTION:
            td_update_train_speed(ctxt, 15);
            Delay(2);
            td_update_train_speed(ctxt, 0);
            break;
        case TRAIN_TOGGLE_LIGHT:
            td_toggle_light(ctxt);
            break;
        case TRAIN_HORN_SOUND:
            td_toggle_horn(ctxt);
            break;
        case TRAIN_HIT_SENSOR:
        case TRAIN_NEXT_SENSOR:
        case TRAIN_REQUEST_COUNT:
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

    tr_setup(&context);

    const int train_tid = myParentTid();
    const mc_req callin = {
        .type              = MC_TD_CALL,
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

        int time = Time();

        switch (req.type) {
        case TRAIN_CHANGE_SPEED:
            td_update_train_speed(&context, req.one.int_value);
            break;
        case TRAIN_REVERSE_DIRECTION: {
            int old_speed = context.speed;

            td_update_train_speed(&context, 0);
            Delay(old_speed * 40);
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
        case TRAIN_NEXT_SENSOR: {
            context.last = req.two.sensor;
            continue;
        }
        case TRAIN_HIT_SENSOR: {
            context.last = req.one.sensor;

            log("(%d) TRAIN HIT %c %d",
                time, context.last.bank, context.last.num);

            result = get_sensor_from(&req.one.sensor, &context.dist_last, &context.next);
            assert(result >= 0, "failed");

            context.time_last = time;
            int velocity = velocity_for_speed(context.off, context.speed);
            context.estim_next = context.dist_last / velocity;

            log("NEXT IS %d %c %d", context.dist_last, context.next.bank, context.next.num);

            Reply(tid, (char*)&context.next, sizeof(context.next));
            continue;
        }
        case TRAIN_EXPECTED_SENSOR: {

            int expected = context.time_last + context.estim_next;
            int actual   = time - context.time_last;
            update_velocity_for_speed(context.off,
                                      context.speed,
                                      context.dist_last,
                                      actual);

            log("%d\t%c%d -> %c%d\tETA: %d\tTA: %d\tDelta: %d",
                context.num,
                context.last.bank, context.last.num,
                context.next.bank, context.next.num,
                expected, time,
                time - (context.time_last + context.estim_next));

            context.last = context.next;
            result = get_sensor_from(&context.last, &context.dist_last, &context.next);

            context.time_last = time;
            int velocity = velocity_for_speed(context.off, context.speed);
            context.estim_next = context.dist_last / velocity;

            Reply(tid, (char*)&context.next, sizeof(context.next));
            continue;
        }
        case TRAIN_REQUEST_COUNT:
            ABORT("Train %d got request count request from %d",
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
    log("Resetting Train %d", ctxt->num);

    if (ctxt->horn)   td_toggle_horn(ctxt);
    if (!ctxt->light) td_toggle_light(ctxt);
    td_update_train_speed(ctxt, 0);
}
