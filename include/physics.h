
#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include <std.h>
#include <train.h>
#include <track_node.h>
#include <syscall.h>
#include <tasks/train_driver.h>
#include <tasks/train_driver_types.h>


#define INITIAL_FEEDBACK_THRESHOLD 50

static track_type __attribute__ ((pure))
velocity_type(const int sensor) {
    switch (sensor) {
        // Bank A
    case 0: return TRACK_STRAIGHT_TURNOUT;
    case 1: return TRACK_STRAIGHT;
    case 2: return TRACK_CURVED_TURNOUT;
    case 3: return TRACK_STRAIGHT;
    case 4: return TRACK_STRAIGHT_TURNOUT;
    case 5: return TRACK_STRAIGHT;
    case 6: return TRACK_STRAIGHT;
    case 7: return TRACK_CURVED_TURNOUT;
    case 8: return TRACK_STRAIGHT;
    case 9: return TRACK_CURVED_TURNOUT;
    case 10: return TRACK_CURVED_TURNOUT;
    case 11: return TRACK_CURVED;
    case 12: return TRACK_CURVED_TURNOUT;
    case 13: return TRACK_STRAIGHT;
    case 14: return TRACK_CURVED;
    case 15: return TRACK_CURVED_TURNOUT;
        // Bank B
    case 16: return TRACK_STRAIGHT;
    case 17: return TRACK_STRAIGHT_TURNOUT;
    case 18: return TRACK_CURVED;
    case 19: return TRACK_CURVED_TURNOUT;
    case 20: return TRACK_STRAIGHT;
    case 21: return TRACK_STRAIGHT_TURNOUT;
    case 22: return TRACK_STRAIGHT;
    case 23: return TRACK_STRAIGHT;
    case 24: return TRACK_STRAIGHT;
    case 25: return TRACK_STRAIGHT;
    case 26: return TRACK_STRAIGHT;
    case 27: return TRACK_STRAIGHT;
    case 28: return TRACK_CURVED_TURNOUT;
    case 29: return TRACK_CURVED;
    case 30: return TRACK_STRAIGHT;
    case 31: return TRACK_CURVED_TURNOUT;
        // Bank C
    case 32: return TRACK_CURVED;
    case 33: return TRACK_CURVED_TURNOUT;
    case 34: return TRACK_STRAIGHT;
    case 35: return TRACK_STRAIGHT_TURNOUT;
    case 36: return TRACK_STRAIGHT_TURNOUT;
    case 37: return TRACK_CURVED;
    case 38: return TRACK_STRAIGHT_TURNOUT;
    case 39: return TRACK_STRAIGHT_TURNOUT;
    case 40: return TRACK_CURVED_TURNOUT;
    case 41: return TRACK_STRAIGHT;
    case 42: return TRACK_STRAIGHT;
    case 43: return TRACK_CURVED_TURNOUT;
    case 44: return TRACK_STRAIGHT;
    case 45: return TRACK_CURVED_TURNOUT;
    case 46: return TRACK_STRAIGHT;
    case 47: return TRACK_STRAIGHT_TURNOUT;
        // Bank D
    case 48: return TRACK_CURVED_TURNOUT;
    case 49: return TRACK_CURVED;
    case 50: return TRACK_STRAIGHT_TURNOUT;
    case 51: return TRACK_STRAIGHT;
    case 52: return TRACK_CURVED;
    case 53: return TRACK_CURVED_TURNOUT;
    case 54: return TRACK_CURVED_TURNOUT;
    case 55: return TRACK_CURVED;
    case 56: return TRACK_CURVED;
    case 57: return TRACK_CURVED_TURNOUT;
    case 58: return TRACK_STRAIGHT;
    case 59: return TRACK_STRAIGHT_TURNOUT;
    case 60: return TRACK_STRAIGHT;
    case 61: return TRACK_STRAIGHT_TURNOUT;
    case 62: return TRACK_CURVED;
    case 63: return TRACK_CURVED_TURNOUT;
        // Bank E
    case 64: return TRACK_CURVED_TURNOUT;
    case 65: return TRACK_CURVED;
    case 66: return TRACK_CURVED;
    case 67: return TRACK_CURVED_TURNOUT;
    case 68: return TRACK_CURVED;
    case 69: return TRACK_STRAIGHT_TURNOUT;
    case 70: return TRACK_CURVED;
    case 71: return TRACK_STRAIGHT;
    case 72: return TRACK_CURVED_TURNOUT;
    case 73: return TRACK_CURVED;
    case 74: return TRACK_CURVED;
    case 75: return TRACK_STRAIGHT_TURNOUT;
    case 76: return TRACK_CURVED_TURNOUT;
    case 77: return TRACK_CURVED;
    case 78: return TRACK_CURVED_TURNOUT;
    case 79: return TRACK_CURVED;
    default:
        ABORT("No such sensor! (%d)", sensor);
    }
}

static inline int __attribute__ ((pure))
velocity_for_speed(const train_context* const ctxt) {
    if (ctxt->speed == 0) return 0;

    const track_type type = velocity_type(ctxt->sensor_last);
    return ctxt->velocity[type][ctxt->speed - TRAIN_PRACTICAL_MIN_SPEED];
}

static void velocity_feedback(train_context* const ctxt,
                              const int time,
                              const int delta) {

    if (abs(delta) > ctxt->feedback_threshold) {
        log("Feedback is off by too much (%d) (%d) (%d). I suspect foul play!",
            delta, ctxt->feedback_threshold, ctxt->feedback_alpha);
        return;
    }

    if (ctxt->speed == 0) return;

    const int old_speed = velocity_for_speed(ctxt);
    const int new_speed = ctxt->dist_last / time;

    int class = velocity_type(ctxt->sensor_last);
    log("class %d %d %d", old_speed, new_speed, class);
    switch (ctxt->feedback_alpha) {
    case HALF_AND_HALF:
        ctxt->velocity[class][ctxt->speed - TRAIN_PRACTICAL_MIN_SPEED] =
            (old_speed + new_speed) >> 1;
        break;
    case EIGHTY_TWENTY:
        ctxt->velocity[class][ctxt->speed - TRAIN_PRACTICAL_MIN_SPEED] =
            ((old_speed << 2) + new_speed) / 5;
        break;
    case NINTY_TEN:
        ctxt->velocity[class][ctxt->speed - TRAIN_PRACTICAL_MIN_SPEED] =
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


static inline void tr_setup_physics(train_context* const ctxt) {
    switch (ctxt->off) {
    case 0:
        ctxt->velocity[0][0] = 3488;
        ctxt->velocity[0][1] = 3621;
        ctxt->velocity[0][2] = 4299;
        ctxt->velocity[0][3] = 4661;
        ctxt->velocity[0][4] = 6460;
        ctxt->velocity[0][5] = 5251;
        ctxt->velocity[0][6] = 6463;
        ctxt->velocity[1][0] = 3488;
        ctxt->velocity[1][1] = 3621;
        ctxt->velocity[1][2] = 4250;
        ctxt->velocity[1][3] = 4661;
        ctxt->velocity[1][4] = 5025;
        ctxt->velocity[1][5] = 5249;
        ctxt->velocity[1][6] = 5276;
        ctxt->velocity[2][0] = 3488;
        ctxt->velocity[2][1] = 3621;
        ctxt->velocity[2][2] = 4250;
        ctxt->velocity[2][3] = 4661;
        ctxt->velocity[2][4] = 5025;
        ctxt->velocity[2][5] = 5249;
        ctxt->velocity[2][6] = 5276;
        ctxt->velocity[3][0] = 3488;
        ctxt->velocity[3][1] = 3621;
        ctxt->velocity[3][2] = 4250;
        ctxt->velocity[3][3] = 4661;
        ctxt->velocity[3][4] = 5025;
        ctxt->velocity[3][5] = 5249;
        ctxt->velocity[3][6] = 5276;
        break;
    case 1:
        ctxt->velocity[0][0] = 3488;
        ctxt->velocity[0][1] = 3621;
        ctxt->velocity[0][2] = 4299;
        ctxt->velocity[0][3] = 4661;
        ctxt->velocity[0][4] = 6460;
        ctxt->velocity[0][5] = 5251;
        ctxt->velocity[0][6] = 6463;
        ctxt->velocity[1][0] = 3488;
        ctxt->velocity[1][1] = 3621;
        ctxt->velocity[1][2] = 4250;
        ctxt->velocity[1][3] = 4661;
        ctxt->velocity[1][4] = 5025;
        ctxt->velocity[1][5] = 5249;
        ctxt->velocity[1][6] = 5276;
        ctxt->velocity[2][0] = 3488;
        ctxt->velocity[2][1] = 3621;
        ctxt->velocity[2][2] = 4250;
        ctxt->velocity[2][3] = 4661;
        ctxt->velocity[2][4] = 5025;
        ctxt->velocity[2][5] = 5249;
        ctxt->velocity[2][6] = 5276;
        ctxt->velocity[3][0] = 3488;
        ctxt->velocity[3][1] = 3621;
        ctxt->velocity[3][2] = 4250;
        ctxt->velocity[3][3] = 4661;
        ctxt->velocity[3][4] = 5025;
        ctxt->velocity[3][5] = 5249;
        ctxt->velocity[3][6] = 5276;
        break;
    case 2:
        ctxt->velocity[0][0] = 3488;
        ctxt->velocity[0][1] = 3621;
        ctxt->velocity[0][2] = 4299;
        ctxt->velocity[0][3] = 4661;
        ctxt->velocity[0][4] = 6460;
        ctxt->velocity[0][5] = 5251;
        ctxt->velocity[0][6] = 6463;
        ctxt->velocity[1][0] = 3488;
        ctxt->velocity[1][1] = 3621;
        ctxt->velocity[1][2] = 4250;
        ctxt->velocity[1][3] = 4661;
        ctxt->velocity[1][4] = 5025;
        ctxt->velocity[1][5] = 5249;
        ctxt->velocity[1][6] = 5276;
        ctxt->velocity[2][0] = 3488;
        ctxt->velocity[2][1] = 3621;
        ctxt->velocity[2][2] = 4250;
        ctxt->velocity[2][3] = 4661;
        ctxt->velocity[2][4] = 5025;
        ctxt->velocity[2][5] = 5249;
        ctxt->velocity[2][6] = 5276;
        ctxt->velocity[3][0] = 3488;
        ctxt->velocity[3][1] = 3621;
        ctxt->velocity[3][2] = 4250;
        ctxt->velocity[3][3] = 4661;
        ctxt->velocity[3][4] = 5025;
        ctxt->velocity[3][5] = 5249;
        ctxt->velocity[3][6] = 5276;
        break;
    case 3:
        ctxt->velocity[0][0] = 3488;
        ctxt->velocity[0][1] = 3621;
        ctxt->velocity[0][2] = 4299;
        ctxt->velocity[0][3] = 4661;
        ctxt->velocity[0][4] = 6460;
        ctxt->velocity[0][5] = 5251;
        ctxt->velocity[0][6] = 6463;
        ctxt->velocity[1][0] = 3488;
        ctxt->velocity[1][1] = 3621;
        ctxt->velocity[1][2] = 4250;
        ctxt->velocity[1][3] = 4661;
        ctxt->velocity[1][4] = 5025;
        ctxt->velocity[1][5] = 5249;
        ctxt->velocity[1][6] = 5276;
        ctxt->velocity[2][0] = 3488;
        ctxt->velocity[2][1] = 3621;
        ctxt->velocity[2][2] = 4250;
        ctxt->velocity[2][3] = 4661;
        ctxt->velocity[2][4] = 5025;
        ctxt->velocity[2][5] = 5249;
        ctxt->velocity[2][6] = 5276;
        ctxt->velocity[3][0] = 3488;
        ctxt->velocity[3][1] = 3621;
        ctxt->velocity[3][2] = 4250;
        ctxt->velocity[3][3] = 4661;
        ctxt->velocity[3][4] = 5025;
        ctxt->velocity[3][5] = 5249;
        ctxt->velocity[3][6] = 5276;
        break;
    case 4:
        ctxt->velocity[0][0] = 3488;
        ctxt->velocity[0][1] = 3621;
        ctxt->velocity[0][2] = 4299;
        ctxt->velocity[0][3] = 4661;
        ctxt->velocity[0][4] = 4764;
        ctxt->velocity[0][5] = 6988;
        ctxt->velocity[0][6] = 5408;
        ctxt->velocity[0][7] = 3083;
        ctxt->velocity[0][8] = 0;
        ctxt->velocity[0][9] = 4087;
        ctxt->velocity[0][10] = 0;
        ctxt->velocity[0][11] = 5437;
        ctxt->velocity[0][12] = 0;
        ctxt->velocity[0][13] = 0;
        ctxt->velocity[1][0] = 3488;
        ctxt->velocity[1][1] = 3621;
        ctxt->velocity[1][2] = 4250;
        ctxt->velocity[1][3] = 4661;
        ctxt->velocity[1][4] = 5025;
        ctxt->velocity[1][5] = 5249;
        ctxt->velocity[1][6] = 5276;
        ctxt->velocity[1][7] = 3309;
        ctxt->velocity[1][8] = 0;
        ctxt->velocity[1][9] = 7372;
        ctxt->velocity[1][10] = 0;
        ctxt->velocity[1][11] = 8347;
        ctxt->velocity[1][12] = 0;
        ctxt->velocity[1][13] = 0;
        ctxt->velocity[2][0] = 3488;
        ctxt->velocity[2][1] = 3621;
        ctxt->velocity[2][2] = 4250;
        ctxt->velocity[2][3] = 4661;
        ctxt->velocity[2][4] = 5025;
        ctxt->velocity[2][5] = 5249;
        ctxt->velocity[2][6] = 5276;
        ctxt->velocity[2][7] = 7595;
        ctxt->velocity[2][8] = 0;
        ctxt->velocity[2][9] = 16774;
        ctxt->velocity[2][10] = 0;
        ctxt->velocity[2][11] = 11083;
        ctxt->velocity[2][12] = 0;
        ctxt->velocity[2][13] = 0;
        ctxt->velocity[3][0] = 3488;
        ctxt->velocity[3][1] = 3621;
        ctxt->velocity[3][2] = 4250;
        ctxt->velocity[3][3] = 4661;
        ctxt->velocity[3][4] = 5025;
        ctxt->velocity[3][5] = 5249;
        ctxt->velocity[3][6] = 5276;
        ctxt->velocity[3][7] = 2956;
        ctxt->velocity[3][8] = 0;
        ctxt->velocity[3][9] = 7357;
        ctxt->velocity[3][10] = 0;
        ctxt->velocity[3][11] = 5297;
        ctxt->velocity[3][12] = 0;
        ctxt->velocity[3][13] = 0;
        break;
    case 5:
        ctxt->velocity[0][0] = 3488;
        ctxt->velocity[0][1] = 3621;
        ctxt->velocity[0][2] = 4299;
        ctxt->velocity[0][3] = 4661;
        ctxt->velocity[0][4] = 6460;
        ctxt->velocity[0][5] = 5251;
        ctxt->velocity[0][6] = 6463;
        ctxt->velocity[1][0] = 3488;
        ctxt->velocity[1][1] = 3621;
        ctxt->velocity[1][2] = 4250;
        ctxt->velocity[1][3] = 4661;
        ctxt->velocity[1][4] = 5025;
        ctxt->velocity[1][5] = 5249;
        ctxt->velocity[1][6] = 5276;
        ctxt->velocity[2][0] = 3488;
        ctxt->velocity[2][1] = 3621;
        ctxt->velocity[2][2] = 4250;
        ctxt->velocity[2][3] = 4661;
        ctxt->velocity[2][4] = 5025;
        ctxt->velocity[2][5] = 5249;
        ctxt->velocity[2][6] = 5276;
        ctxt->velocity[3][0] = 3488;
        ctxt->velocity[3][1] = 3621;
        ctxt->velocity[3][2] = 4250;
        ctxt->velocity[3][3] = 4661;
        ctxt->velocity[3][4] = 5025;
        ctxt->velocity[3][5] = 5249;
        ctxt->velocity[3][6] = 5276;
        break;
    case 6:
        ctxt->velocity[0][0] = 3488;
        ctxt->velocity[0][1] = 3621;
        ctxt->velocity[0][2] = 4299;
        ctxt->velocity[0][3] = 4661;
        ctxt->velocity[0][4] = 6460;
        ctxt->velocity[0][5] = 5251;
        ctxt->velocity[0][6] = 6463;
        ctxt->velocity[1][0] = 3488;
        ctxt->velocity[1][1] = 3621;
        ctxt->velocity[1][2] = 4250;
        ctxt->velocity[1][3] = 4661;
        ctxt->velocity[1][4] = 5025;
        ctxt->velocity[1][5] = 5249;
        ctxt->velocity[1][6] = 5276;
        ctxt->velocity[2][0] = 3488;
        ctxt->velocity[2][1] = 3621;
        ctxt->velocity[2][2] = 4250;
        ctxt->velocity[2][3] = 4661;
        ctxt->velocity[2][4] = 5025;
        ctxt->velocity[2][5] = 5249;
        ctxt->velocity[2][6] = 5276;
        ctxt->velocity[3][0] = 3488;
        ctxt->velocity[3][1] = 3621;
        ctxt->velocity[3][2] = 4250;
        ctxt->velocity[3][3] = 4661;
        ctxt->velocity[3][4] = 5025;
        ctxt->velocity[3][5] = 5249;
        ctxt->velocity[3][6] = 5276;
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
        ctxt->stopping_slope  = 63905;
        ctxt->stopping_offset = -41611;
        break;
    }

    ctxt->feedback_threshold = INITIAL_FEEDBACK_THRESHOLD;
    ctxt->feedback_alpha     = EIGHTY_TWENTY;
}

#endif
