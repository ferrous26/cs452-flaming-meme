
#ifndef __MASTER_P__
#define __MASTER_P__

#include <std.h>
#include <debug.h>
#include <tasks/train_master/types.c>


static void master_dump_velocity_table(master* const ctxt) {

    char buffer[1024];
    char* ptr = log_start(buffer);
    ptr = sprintf(ptr, "[%s] Velocity Table Dump\n", ctxt->name);

    for (int speed = 0; speed < TRAIN_SPEEDS; speed++) {
        ptr = sprintf(ptr, "%d,%d,", ctxt->train_id, speed);

        for (int type = 0; type < TRACK_TYPE_COUNT; type++) {
            ptr = sprintf_int(ptr, ctxt->vmap[speed][type]);

            if (type != TRACK_TYPE_COUNT - 1)
                ptr = sprintf_char(ptr, ',');
        }
        ptr = sprintf_char(ptr, '\n');
    }

    ptr = log_end(ptr);
    Puts(buffer, ptr - buffer);
}

static void master_update_feedback_threshold(master* const ctxt,
                                             const int threshold) {
    log("[%s] Updating feedback threshold to %d um/s (was %d um/s)",
        ctxt->name, threshold, ctxt->feedback_threshold);
    ctxt->feedback_threshold = threshold;
}

static void master_update_feedback_ratio(master* const ctxt,
                                         const ratio alpha) {
    switch (alpha) {
    case HALF_AND_HALF:
        log("[%s] Updating feedback ratio to 50/50", ctxt->name);
        break;
    case EIGHTY_TWENTY:
        log("[%s] Updating feedback ratio to 80/20", ctxt->name);
        break;
    case NINTY_TEN:
        log("[%s] Updating feedback ratio to 90/10", ctxt->name);
        break;
    }
    ctxt->feedback_ratio = alpha;
}

static void master_update_stopping_distance_offset(master* const ctxt,
                                                   const int offset) {
    log("[%s] Updating stop distance offset to %d mm (was %d mm)",
        ctxt->name, offset / 1000, ctxt->stopping_distance_offset / 1000);
    ctxt->stopping_distance_offset = offset;
}

static void master_update_turnout_clearance_offset(master* const ctxt,
                                                   const int offset) {
    log("[%s] Updating turnout clearance offset to %d mm (was %d mm)",
        ctxt->name, offset / 1000, ctxt->turnout_clearance_offset / 1000);
    ctxt->turnout_clearance_offset = offset;
}

static void master_update_reverse_time_fudge(master* const ctxt,
                                             const int fudge) {
    log("[%s] Updating reverse time fudge factor to %d ticks (was %d ticks)",
        ctxt->name, fudge, ctxt->reverse_time_fudge_factor);
    ctxt->reverse_time_fudge_factor = fudge;
}

static track_type __attribute__ ((const))
velocity_type(const int sensor_idx) {
    switch (sensor_idx) {
        // Bank A
    case 0: return TRACK_STRAIGHT;
    case 1: return TRACK_STRAIGHT;
    case 2: return TRACK_INNER_CURVE;
    case 3: return TRACK_INNER_STRAIGHT;
    case 4: return TRACK_STRAIGHT;
    case 5: return TRACK_STRAIGHT;
    case 6: return TRACK_STRAIGHT;
    case 7: return TRACK_BACK_CONNECT;
    case 8: return TRACK_STRAIGHT;
    case 9: return TRACK_BACK_CONNECT;
    case 10: return TRACK_BACK_CONNECT;
    case 11: return TRACK_OUTER_STRAIGHT;
    case 12: return TRACK_OUTER_CURVE2;
    case 13: return TRACK_STRAIGHT;
    case 14: return TRACK_OUTER_STRAIGHT;
    case 15: return TRACK_OUTER_CURVE2;
        // Bank B
    case 16: return TRACK_INNER_STRAIGHT;
    case 17: return TRACK_INNER_CURVE;
    case 18: return TRACK_INNER_CURVE;
    case 19: return TRACK_INNER_CURVE;
    case 20: return TRACK_INNER_STRAIGHT;
    case 21: return TRACK_INNER_STRAIGHT;
    case 22: return TRACK_STRAIGHT;
    case 23: return TRACK_STRAIGHT;
    case 24: return TRACK_STRAIGHT;
    case 25: return TRACK_STRAIGHT;
    case 26: return TRACK_STRAIGHT;
    case 27: return TRACK_STRAIGHT;
    case 28: return TRACK_THREE_WAY;
    case 29: return TRACK_INNER_CURVE;
    case 30: return TRACK_INNER_STRAIGHT;
    case 31: return TRACK_INNER_CURVE;
        // Bank C
    case 32: return TRACK_INNER_CURVE;
    case 33: return TRACK_THREE_WAY;
    case 34: return TRACK_STRAIGHT;
    case 35: return TRACK_BACK_CONNECT;
    case 36: return TRACK_BACK_CONNECT;
    case 37: return TRACK_OUTER_CURVE1;
    case 38: return TRACK_BACK_CONNECT;
    case 39: return TRACK_STRAIGHT;
    case 40: return TRACK_INNER_CURVE;
    case 41: return TRACK_INNER_CURVE;
    case 42: return TRACK_INNER_STRAIGHT;
    case 43: return TRACK_INNER_CURVE;
    case 44: return TRACK_LONG_STRAIGHT;
    case 45: return TRACK_OUTER_CURVE2;
    case 46: return TRACK_STRAIGHT;
    case 47: return TRACK_OUTER_CURVE1;
        // Bank D
    case 48: return TRACK_THREE_WAY;
    case 49: return TRACK_INNER_CURVE;
    case 50: return TRACK_INNER_STRAIGHT;
    case 51: return TRACK_INNER_STRAIGHT;
    case 52: return TRACK_INNER_CURVE;
    case 53: return TRACK_INNER_CURVE;
    case 54: return TRACK_OUTER_CURVE1;
    case 55: return TRACK_OUTER_CURVE1;
    case 56: return TRACK_OUTER_CURVE1;
    case 57: return TRACK_OUTER_CURVE1;
    case 58: return TRACK_STRAIGHT;
    case 59: return TRACK_OUTER_CURVE1;
    case 60: return TRACK_INNER_STRAIGHT;
    case 61: return TRACK_INNER_CURVE;
    case 62: return TRACK_INNER_CURVE;
    case 63: return TRACK_INNER_CURVE;
        // Bank E
    case 64: return TRACK_THREE_WAY;
    case 65: return TRACK_INNER_CURVE;
    case 66: return TRACK_INNER_CURVE;
    case 67: return TRACK_INNER_CURVE;
    case 68: return TRACK_INNER_CURVE;
    case 69: return TRACK_INNER_CURVE;
    case 70: return TRACK_OUTER_CURVE1;
    case 71: return TRACK_LONG_STRAIGHT;
    case 72: return TRACK_INNER_CURVE;
    case 73: return TRACK_INNER_CURVE;
    case 74: return TRACK_OUTER_CURVE1;
    case 75: return TRACK_BACK_CONNECT;
    case 76: return TRACK_INNER_CURVE;
    case 77: return TRACK_INNER_CURVE;
    case 78: return TRACK_INNER_CURVE;
    case 79: return TRACK_INNER_CURVE;
    default:
        ABORT("No such sensor! (%d)", sensor_idx);
    }
}

static inline int
physics_velocity(const master* const ctxt,
                 const int speed,
                 const track_type type) {
    return ctxt->vmap[(speed / 10) - 1][type];
}

static char* sprintf_sensor(char* ptr, const sensor* const s) {
    if (s->num < 10)
        return sprintf(ptr, "%c0%d ", s->bank, s->num);
    else
        return sprintf(ptr, "%c%d ",  s->bank, s->num);
}

static void
physics_update_tracking_ui(master* const ctxt, const int delta_v) {

    char  buffer[32];
    char* ptr = vt_goto(buffer,
                        TRAIN_ROW + ctxt->train_id,
                        TRAIN_SENSORS_COL);

    const sensor last = pos_to_sensor(ctxt->last_sensor);
    const sensor curr = pos_to_sensor(ctxt->current_sensor);
    const sensor next = pos_to_sensor(ctxt->next_sensor);

    ptr = sprintf_int(ptr, delta_v / 10);
    ptr = ui_pad(ptr, log10(abs(delta_v)) + (delta_v < 0 ? 1 : 0), 5);

    ptr = sprintf_sensor(ptr, &last);
    ptr = sprintf_sensor(ptr, &curr);
    ptr = sprintf_sensor(ptr, &next);

    // if previous delta was an order of larger than it should have been
    ptr = sprintf_char(ptr, ' ');

    Puts(buffer, ptr-buffer);
}

static inline void
physics_feedback(master* const ctxt,
                 const int actual_v,
                 const int expected_v) {

    // do not feedback while accelerating
    if (ctxt->current_speed == 0) return;

    const int delta_v = actual_v - expected_v;

    if (abs(delta_v) > ctxt->feedback_threshold) {
        const sensor s = pos_to_sensor(ctxt->current_sensor);
        log("[%s] Feedback is off by too much %d / %d (%c%d)."
            " I suspect foul play!",
            ctxt->name,
            delta_v, ctxt->feedback_threshold,
            s.bank, s.num);
        return;
    }

    const int type = velocity_type(ctxt->last_sensor);
    const int speed_idx = (ctxt->current_speed / 10) - 1;

    switch (ctxt->feedback_ratio) {
    case HALF_AND_HALF:
        ctxt->vmap[speed_idx][type] = (actual_v + expected_v) >> 1;
        break;
    case EIGHTY_TWENTY:
        ctxt->vmap[speed_idx][type] = ((expected_v << 2) + actual_v) / 5;
        break;
    case NINTY_TEN:
        ctxt->vmap[speed_idx][type] = ((expected_v * 9) + actual_v) / 10;
        break;
    }
}

static inline int
physics_stopping_distance(const master* const ctxt) {
    return
        (ctxt->smap.slope * ctxt->current_speed) +
        ctxt->smap.offset +
        ctxt->stopping_distance_offset;
}

static inline int
physics_turnout_stopping_distance(const master* const ctxt) {
    return physics_stopping_distance(ctxt) +
        ctxt->turnout_clearance_offset;
}

static inline int
physics_stopping_time(const master* const ctxt, const int stop_dist) {

    const int dist = stop_dist / 1000;

    int sum =
        (((ctxt->amap.terms[3].factor * dist * dist) /
          ctxt->amap.terms[3].scale) * dist) / ctxt->amap.mega_scale;

    sum += (ctxt->amap.terms[2].factor * dist * dist) /
        ctxt->amap.terms[2].scale;

    sum += (ctxt->amap.terms[1].factor * dist) /
        ctxt->amap.terms[1].scale;

    sum += ctxt->amap.terms[0].factor /
        ctxt->amap.terms[0].scale;

    return sum;
}

static inline void master_init_physics(master* const ctxt) {

    // Common settings (can be overridden below)
    ctxt->feedback_ratio             = HALF_AND_HALF;
    ctxt->feedback_threshold         = FEEDBACK_THRESHOLD_DEFAULT;
    ctxt->stopping_distance_offset   = STOPPING_DISTANCE_OFFSET_DEFAULT;
    ctxt->reverse_time_fudge_factor  = REVERSE_TIME_FUDGE_FACTOR;
    ctxt->turnout_clearance_offset   = TURNOUT_CLEARANCE_OFFSET_DEFAULT;

    switch (ctxt->train_id) {
    case 0: // train 45
        ctxt->measurements.front  = 19000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        ctxt->amap.mega_scale       = 10000;
        ctxt->amap.terms[3].factor  = 77;
        ctxt->amap.terms[3].scale   = 10000;
        ctxt->amap.terms[2].factor  = -15;
        ctxt->amap.terms[2].scale   = 10000;
        ctxt->amap.terms[1].factor  = 11863;
        ctxt->amap.terms[1].scale   = 10000;
        ctxt->amap.terms[0].factor  = 34;
        ctxt->amap.terms[0].scale   = 1;

        ctxt->smap.slope  =   6383;
        ctxt->smap.offset = -42353;
        break;

    case 1: // train 47
        ctxt->measurements.front  = 16000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        ctxt->amap.mega_scale       = 10000;
        ctxt->amap.terms[3].factor  = 77;
        ctxt->amap.terms[3].scale   = 10000;
        ctxt->amap.terms[2].factor  = -15;
        ctxt->amap.terms[2].scale   = 10000;
        ctxt->amap.terms[1].factor  = 11863;
        ctxt->amap.terms[1].scale   = 10000;
        ctxt->amap.terms[0].factor  = 34;
        ctxt->amap.terms[0].scale   = 1;

        ctxt->smap.slope  =   6703;
        ctxt->smap.offset = -38400;

        ctxt->velocity[0][0] = 0;
        ctxt->velocity[0][1] = 0;
        ctxt->velocity[0][2] = 0;
        ctxt->velocity[0][3] = 0;
        ctxt->velocity[0][4] = 0;
        ctxt->velocity[0][5] = 0;
        ctxt->velocity[0][6] = 0;
        ctxt->velocity[0][7] = 0;
        ctxt->velocity[0][8] = 0;
        ctxt->velocity[1][0] = 0;
        ctxt->velocity[1][1] = 0;
        ctxt->velocity[1][2] = 0;
        ctxt->velocity[1][3] = 0;
        ctxt->velocity[1][4] = 0;
        ctxt->velocity[1][5] = 0;
        ctxt->velocity[1][6] = 0;
        ctxt->velocity[1][7] = 0;
        ctxt->velocity[1][8] = 0;
        ctxt->velocity[2][0] = 0;
        ctxt->velocity[2][1] = 0;
        ctxt->velocity[2][2] = 0;
        ctxt->velocity[2][3] = 0;
        ctxt->velocity[2][4] = 0;
        ctxt->velocity[2][5] = 0;
        ctxt->velocity[2][6] = 0;
        ctxt->velocity[2][7] = 0;
        ctxt->velocity[2][8] = 0;
        ctxt->velocity[3][0] = 1679;
        ctxt->velocity[3][1] = 1558;
        ctxt->velocity[3][2] = 1454;
        ctxt->velocity[3][3] = 865;
        ctxt->velocity[3][4] = 1562;
        ctxt->velocity[3][5] = 1560;
        ctxt->velocity[3][6] = 1649;
        ctxt->velocity[3][7] = 1508;
        ctxt->velocity[3][8] = 1596;
        ctxt->velocity[4][0] = 0;
        ctxt->velocity[4][1] = 0;
        ctxt->velocity[4][2] = 0;
        ctxt->velocity[4][3] = 0;
        ctxt->velocity[4][4] = 0;
        ctxt->velocity[4][5] = 0;
        ctxt->velocity[4][6] = 0;
        ctxt->velocity[4][7] = 0;
        ctxt->velocity[4][8] = 0;
        ctxt->velocity[5][0] = 0;
        ctxt->velocity[5][1] = 0;
        ctxt->velocity[5][2] = 0;
        ctxt->velocity[5][3] = 0;
        ctxt->velocity[5][4] = 0;
        ctxt->velocity[5][5] = 0;
        ctxt->velocity[5][6] = 0;
        ctxt->velocity[5][7] = 0;
        ctxt->velocity[5][8] = 0;
        ctxt->velocity[6][0] = 0;
        ctxt->velocity[6][1] = 0;
        ctxt->velocity[6][2] = 0;
        ctxt->velocity[6][3] = 0;
        ctxt->velocity[6][4] = 0;
        ctxt->velocity[6][5] = 0;
        ctxt->velocity[6][6] = 0;
        ctxt->velocity[6][7] = 0;
        ctxt->velocity[6][8] = 0;
        ctxt->velocity[7][0] = 0;
        ctxt->velocity[7][1] = 0;
        ctxt->velocity[7][2] = 0;
        ctxt->velocity[7][3] = 3695;
        ctxt->velocity[7][4] = 3776;
        ctxt->velocity[7][5] = 3696;
        ctxt->velocity[7][6] = 3696;
        ctxt->velocity[7][7] = 3656;
        ctxt->velocity[7][8] = 3681;
        ctxt->velocity[8][0] = 0;
        ctxt->velocity[8][1] = 0;
        ctxt->velocity[8][2] = 0;
        ctxt->velocity[8][3] = 0;
        ctxt->velocity[8][4] = 0;
        ctxt->velocity[8][5] = 0;
        ctxt->velocity[8][6] = 0;
        ctxt->velocity[8][7] = 0;
        ctxt->velocity[8][8] = 0;
        ctxt->velocity[9][0] = 0;
        ctxt->velocity[9][1] = 0;
        ctxt->velocity[9][2] = 0;
        ctxt->velocity[9][3] = 0;
        ctxt->velocity[9][4] = 0;
        ctxt->velocity[9][5] = 0;
        ctxt->velocity[9][6] = 0;
        ctxt->velocity[9][7] = 0;
        ctxt->velocity[9][8] = 0;
        ctxt->velocity[10][0] = 0;
        ctxt->velocity[10][1] = 0;
        ctxt->velocity[10][2] = 0;
        ctxt->velocity[10][3] = 0;
        ctxt->velocity[10][4] = 0;
        ctxt->velocity[10][5] = 0;
        ctxt->velocity[10][6] = 0;
        ctxt->velocity[10][7] = 0;
        ctxt->velocity[10][8] = 0;
        ctxt->velocity[11][0] = 5258;
        ctxt->velocity[11][1] = 6093;
        ctxt->velocity[11][2] = 5954;
        ctxt->velocity[11][3] = 5185;
        ctxt->velocity[11][4] = 5073;
        ctxt->velocity[11][5] = 5595;
        ctxt->velocity[11][6] = 5089;
        ctxt->velocity[11][7] = 5501;
        ctxt->velocity[11][8] = 5472;
        ctxt->velocity[12][0] = 0;
        ctxt->velocity[12][1] = 0;
        ctxt->velocity[12][2] = 0;
        ctxt->velocity[12][3] = 0;
        ctxt->velocity[12][4] = 0;
        ctxt->velocity[12][5] = 0;
        ctxt->velocity[12][6] = 0;
        ctxt->velocity[12][7] = 0;
        ctxt->velocity[12][8] = 0;
        ctxt->velocity[13][0] = 0;
        ctxt->velocity[13][1] = 0;
        ctxt->velocity[13][2] = 0;
        ctxt->velocity[13][3] = 0;
        ctxt->velocity[13][4] = 0;
        ctxt->velocity[13][5] = 0;
        ctxt->velocity[13][6] = 0;
        ctxt->velocity[13][7] = 0;
        ctxt->velocity[13][8] = 0;
        break;

    case 2: // train 48
        ctxt->measurements.front  = 22000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        ctxt->amap.mega_scale       = 10000;
        ctxt->amap.terms[3].factor  = 77;
        ctxt->amap.terms[3].scale   = 10000;
        ctxt->amap.terms[2].factor  = -15;
        ctxt->amap.terms[2].scale   = 10000;
        ctxt->amap.terms[1].factor  = 11863;
        ctxt->amap.terms[1].scale   = 10000;
        ctxt->amap.terms[0].factor  = 34;
        ctxt->amap.terms[0].scale   = 1;

        ctxt->smap.slope  =   7357;
        ctxt->smap.offset = -59043;
        break;

    case 3: // Train 49
        ctxt->measurements.front  = 23000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        ctxt->amap.mega_scale       = 10000;
        ctxt->amap.terms[3].factor  = 77;
        ctxt->amap.terms[3].scale   = 10000;
        ctxt->amap.terms[2].factor  = -15;
        ctxt->amap.terms[2].scale   = 10000;
        ctxt->amap.terms[1].factor  = 11863;
        ctxt->amap.terms[1].scale   = 10000;
        ctxt->amap.terms[0].factor  = 34;
        ctxt->amap.terms[0].scale   = 1;

        ctxt->smap.slope  =   6391;
        ctxt->smap.offset = -41611;
        break;

    case 4: // Train 51
        ctxt->measurements.front  = 7000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 114000;

        ctxt->amap.mega_scale       = 10000;
        ctxt->amap.terms[3].factor  = 77;
        ctxt->amap.terms[3].scale   = 10000;
        ctxt->amap.terms[2].factor  = -15;
        ctxt->amap.terms[2].scale   = 10000;
        ctxt->amap.terms[1].factor  = 11863;
        ctxt->amap.terms[1].scale   = 10000;
        ctxt->amap.terms[0].factor  = 34;
        ctxt->amap.terms[0].scale   = 1;

        ctxt->smap.slope  =   6391;
        ctxt->smap.offset = -41611;
        break;

    default: // just to get us started on new trains
        ctxt->measurements.front  = 23000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        // stopping distance vs. stopping time
        ctxt->amap.mega_scale       = 10000;
        ctxt->amap.terms[3].factor  = 77;
        ctxt->amap.terms[3].scale   = 10000;
        ctxt->amap.terms[2].factor  = -15;
        ctxt->amap.terms[2].scale   = 10000;
        ctxt->amap.terms[1].factor  = 11863;
        ctxt->amap.terms[1].scale   = 10000;
        ctxt->amap.terms[0].factor  = 34;
        ctxt->amap.terms[0].scale   = 1;

        // stopping distance function
        ctxt->smap.slope  =   6391;
        ctxt->smap.offset = -41611;
        break;
    }
}

#endif
