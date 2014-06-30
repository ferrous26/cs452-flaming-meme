#include <std.h>
#include <train.h>
#include <debug.h>
#include <ui_constants.h>
#include <physics.h>

#include <tasks/term_server.h>
#include <tasks/train_server.h>

#include <tasks/name_server.h>
#include <tasks/clock_server.h>

#include <tasks/mission_control.h>
#include <tasks/mission_control_types.h>

#include <tasks/train_station.h>

typedef struct {
    const int num;
    const int off;

    char      name[8];
    int       speed;
    int       light;
    int       horn;
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
    Receive(&tid, (char*)&values, sizeof(values));

    // Only place these should get set
    *((int*)&ctxt->num) = values[0];
    *((int*)&ctxt->off) = values[1];

    assert(ctxt->num > 0 && ctxt->num < 80,
           "Bad Train Number Passed to driver task %d", myTid());

    sprintf(ctxt->name, "TRAIN%d", ctxt->num);
    ctxt->name[7] = '\0';

    if (RegisterAs(ctxt->name)) {
	ABORT("Failed to register train %d", ctxt->num);
    }

    Reply(tid, NULL, 0);
}

static void td_update_train_speed(train_context* const ctxt,
                                  const int new_speed) {
    char  buffer[16];
    char* ptr      = vt_goto(buffer, TRAIN_ROW + ctxt->off, TRAIN_SPEED_COL);
    ptr            = sprintf(ptr, new_speed ? "%d " : "- ", new_speed);
    ctxt->speed    = new_speed & 0xF;

    put_train_cmd((char)ctxt->num, make_speed_cmd(ctxt));
    Puts(buffer, ptr-buffer);

    // TEMPORARY
    log("Speed of %d is %d mm/s",
        ctxt->off, velocity_for_speed(ctxt->off, new_speed) / 1000);
}

static inline void td_toggle_light(train_context* const ctxt) {
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

static inline void td_toggle_horn(train_context* const ctxt) {
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

void train_driver() {
    train_req     req;
    train_context context;
    tr_setup(&context);

    const int train_tid = myParentTid();
    mc_req callin       = {
        .type              = MC_TD_CALL,
        .payload.int_value = context.off
    };

    td_toggle_light(&context); // turn on lights when we initialize!

    FOREVER {
        int result = Send(train_tid,
                          (char*)&callin, sizeof(callin),
                          (char*)&req,    sizeof(req));
        UNUSED(result);
        assert(result == sizeof(req),
               "received weird request in train %d", context.num);

        int time = Time();
        log("received train command %d at %d", req.type, time);

        switch (req.type) {
        case TRAIN_CHANGE_SPEED:
            td_update_train_speed(&context, req.arg);
            break;
        case TRAIN_REVERSE_DIRECTION: {
            int old_speed = context.speed;

            td_update_train_speed(&context, 0);
            Delay(old_speed * 40);
            put_train_cmd((char)context.num, 15);
            Delay(1);
            td_update_train_speed(&context, old_speed);

            break;
        }
        case TRAIN_TOGGLE_LIGHT:
            td_toggle_light(&context);
            break;
        case TRAIN_HORN_SOUND:
            td_toggle_horn(&context);
            break;
        }
    }
}

static void td_reset_train(train_context* const ctxt) {
    log("Resetting Train %d", ctxt->num);

    if (ctxt->horn)  td_toggle_horn(ctxt);
    if (ctxt->light) td_toggle_light(ctxt);
    td_update_train_speed(ctxt, 0);
}
