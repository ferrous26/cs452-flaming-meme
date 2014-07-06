
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
    case 0: return TRACK_STRAIGHT;
    case 1: return TRACK_STRAIGHT;
    case 2: return TRACK_CURVED;
    case 3: return TRACK_COMBO;
    case 4: return TRACK_STRAIGHT;
    case 5: return TRACK_STRAIGHT;
    case 6: return TRACK_STRAIGHT;
    case 7: return TRACK_CURVED;
    case 8: return TRACK_STRAIGHT;
    case 9: return TRACK_COMBO;
    case 10: return TRACK_COMBO;
    case 11: return TRACK_COMBO;
    case 12: return TRACK_COMBO;
    case 13: return TRACK_STRAIGHT;
    case 14: return TRACK_COMBO;
    case 15: return TRACK_CURVED;
        // Bank B
    case 16: return TRACK_STRAIGHT;
    case 17: return TRACK_STRAIGHT;
    case 18: return TRACK_CURVED;
    case 19: return TRACK_CURVED;
    case 20: return TRACK_STRAIGHT;
    case 21: return TRACK_STRAIGHT;
    case 22: return TRACK_STRAIGHT;
    case 23: return TRACK_STRAIGHT;
    case 24: return TRACK_STRAIGHT;
    case 25: return TRACK_STRAIGHT;
    case 26: return TRACK_STRAIGHT;
    case 27: return TRACK_STRAIGHT;
    case 28: return TRACK_CURVED;
    case 29: return TRACK_CURVED;
    case 30: return TRACK_COMBO;
    case 31: return TRACK_CURVED;
        // Bank C
    case 32: return TRACK_CURVED;
    case 33: return TRACK_CURVED;
    case 34: return TRACK_STRAIGHT;
    case 35: return TRACK_COMBO;
    case 36: return TRACK_CURVED;
    case 37: return TRACK_CURVED;
    case 38: return TRACK_CURVED;
    case 39: return TRACK_CURVED;
    case 40: return TRACK_CURVED;
    case 41: return TRACK_COMBO;
    case 42: return TRACK_COMBO;
    case 43: return TRACK_CURVED;
    case 44: return TRACK_STRAIGHT;
    case 45: return TRACK_CURVED;
    case 46: return TRACK_STRAIGHT;
    case 47: return TRACK_STRAIGHT;
        // Bank D
    case 48: return TRACK_CURVED;
    case 49: return TRACK_CURVED;
    case 50: return TRACK_STRAIGHT;
    case 51: return TRACK_STRAIGHT;
    case 52: return TRACK_CURVED;
    case 53: return TRACK_CURVED;
    case 54: return TRACK_CURVED;
    case 55: return TRACK_CURVED;
    case 56: return TRACK_CURVED;
    case 57: return TRACK_CURVED;
    case 58: return TRACK_STRAIGHT;
    case 59: return TRACK_STRAIGHT;
    case 60: return TRACK_STRAIGHT;
    case 61: return TRACK_COMBO;
    case 62: return TRACK_CURVED;
    case 63: return TRACK_CURVED;
        // Bank E
    case 64: return TRACK_CURVED;
    case 65: return TRACK_CURVED;
    case 66: return TRACK_CURVED;
    case 67: return TRACK_CURVED;
    case 68: return TRACK_CURVED;
    case 69: return TRACK_CURVED;
    case 70: return TRACK_CURVED;
    case 71: return TRACK_STRAIGHT;
    case 72: return TRACK_CURVED;
    case 73: return TRACK_CURVED;
    case 74: return TRACK_CURVED;
    case 75: return TRACK_CURVED;
    case 76: return TRACK_CURVED;
    case 77: return TRACK_CURVED;
    case 78: return TRACK_CURVED;
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

    if (delta > ctxt->feedback_threshold) {
        log("Feedback is off by too much (%d) (%d) (%d). I suspect foul play!",
            delta, ctxt->feedback_threshold, ctxt->feedback_alpha);
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
}

#endif
