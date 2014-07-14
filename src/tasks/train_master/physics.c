
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
        (ctxt->stop_dist_map.slope * ctxt->current_speed) +
        ctxt->stop_dist_map.offset +
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
        (((ctxt->dmap.terms[3].factor * dist * dist) /
          ctxt->dmap.terms[3].scale) * dist) / ctxt->dmap.mega_scale;

    sum += (ctxt->dmap.terms[2].factor * dist * dist) /
        ctxt->dmap.terms[2].scale;

    sum += (ctxt->dmap.terms[1].factor * dist) /
        ctxt->dmap.terms[1].scale;

    sum += ctxt->dmap.terms[0].factor /
        ctxt->dmap.terms[0].scale;

    return sum;
}

static inline void master_init_physics(master* const ctxt) {

    // Common settings (can be overridden below)
    ctxt->feedback_ratio             = NINTY_TEN;
    ctxt->feedback_threshold         = FEEDBACK_THRESHOLD_DEFAULT;
    ctxt->stopping_distance_offset   = STOPPING_DISTANCE_OFFSET_DEFAULT;
    ctxt->reverse_time_fudge_factor  = REVERSE_TIME_FUDGE_FACTOR;
    ctxt->turnout_clearance_offset   = TURNOUT_CLEARANCE_OFFSET_DEFAULT;

    switch (ctxt->train_id) {
    case 0: // train 45
        ctxt->measurements.front  = 19000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        ctxt->dmap.mega_scale       = 10000;
        ctxt->dmap.terms[3].factor  = 77;
        ctxt->dmap.terms[3].scale   = 10000;
        ctxt->dmap.terms[2].factor  = -15;
        ctxt->dmap.terms[2].scale   = 10000;
        ctxt->dmap.terms[1].factor  = 11863;
        ctxt->dmap.terms[1].scale   = 10000;
        ctxt->dmap.terms[0].factor  = 34;
        ctxt->dmap.terms[0].scale   = 1;

        ctxt->stop_dist_map.slope  =   6383;
        ctxt->stop_dist_map.offset = -42353;

        ctxt->start_dist_map.mega_scale      = 1;
        ctxt->start_dist_map.terms[3].factor = 5;
        ctxt->start_dist_map.terms[3].scale  = 10000;
        ctxt->start_dist_map.terms[2].factor = 1281;
        ctxt->start_dist_map.terms[2].scale  = 10000;
        ctxt->start_dist_map.terms[1].factor = 15428;
        ctxt->start_dist_map.terms[1].scale  = 1000;
        ctxt->start_dist_map.terms[0].factor = 241;
        ctxt->start_dist_map.terms[0].scale  = 1;

        ctxt->dmap.mega_scale      = 10000;
        ctxt->dmap.terms[3].factor = 113;
        ctxt->dmap.terms[3].scale  = 10000;
        ctxt->dmap.terms[2].factor = 19;
        ctxt->dmap.terms[2].scale  = 10000;
        ctxt->dmap.terms[1].factor = 12062;
        ctxt->dmap.terms[1].scale  = 10000;
        ctxt->dmap.terms[0].factor = 53;
        ctxt->dmap.terms[0].scale  = 1;

        ctxt->vmap[0][0] = 209;
        ctxt->vmap[0][1] = 209;
        ctxt->vmap[0][2] = 209;
        ctxt->vmap[0][3] = 209;
        ctxt->vmap[0][4] = 209;
        ctxt->vmap[0][5] = 209;
        ctxt->vmap[0][6] = 209;
        ctxt->vmap[0][7] = 209;
        ctxt->vmap[0][8] = 209;
        ctxt->vmap[1][0] = 757;
        ctxt->vmap[1][1] = 757;
        ctxt->vmap[1][2] = 757;
        ctxt->vmap[1][3] = 757;
        ctxt->vmap[1][4] = 757;
        ctxt->vmap[1][5] = 757;
        ctxt->vmap[1][6] = 757;
        ctxt->vmap[1][7] = 757;
        ctxt->vmap[1][8] = 757;
        ctxt->vmap[2][0] = 1249;
        ctxt->vmap[2][1] = 1249;
        ctxt->vmap[2][2] = 1249;
        ctxt->vmap[2][3] = 1249;
        ctxt->vmap[2][4] = 1249;
        ctxt->vmap[2][5] = 1249;
        ctxt->vmap[2][6] = 1249;
        ctxt->vmap[2][7] = 1249;
        ctxt->vmap[2][8] = 1249;
        ctxt->vmap[3][0] = 1726;
        ctxt->vmap[3][1] = 1726;
        ctxt->vmap[3][2] = 1726;
        ctxt->vmap[3][3] = 1726;
        ctxt->vmap[3][4] = 1726;
        ctxt->vmap[3][5] = 1726;
        ctxt->vmap[3][6] = 1726;
        ctxt->vmap[3][7] = 1726;
        ctxt->vmap[3][8] = 1726;
        ctxt->vmap[4][0] = 2283;
        ctxt->vmap[4][1] = 2283;
        ctxt->vmap[4][2] = 2283;
        ctxt->vmap[4][3] = 2283;
        ctxt->vmap[4][4] = 2283;
        ctxt->vmap[4][5] = 2283;
        ctxt->vmap[4][6] = 2283;
        ctxt->vmap[4][7] = 2283;
        ctxt->vmap[4][8] = 2283;
        ctxt->vmap[5][0] = 2826;
        ctxt->vmap[5][1] = 2826;
        ctxt->vmap[5][2] = 2826;
        ctxt->vmap[5][3] = 2826;
        ctxt->vmap[5][4] = 2826;
        ctxt->vmap[5][5] = 2826;
        ctxt->vmap[5][6] = 2826;
        ctxt->vmap[5][7] = 2826;
        ctxt->vmap[5][8] = 2826;
        ctxt->vmap[6][0] = 3441;
        ctxt->vmap[6][1] = 3441;
        ctxt->vmap[6][2] = 3441;
        ctxt->vmap[6][3] = 3441;
        ctxt->vmap[6][4] = 3441;
        ctxt->vmap[6][5] = 3441;
        ctxt->vmap[6][6] = 3441;
        ctxt->vmap[6][7] = 3441;
        ctxt->vmap[6][8] = 3441;
        ctxt->vmap[7][0] = 3790;
        ctxt->vmap[7][1] = 3790;
        ctxt->vmap[7][2] = 3790;
        ctxt->vmap[7][3] = 3790;
        ctxt->vmap[7][4] = 3790;
        ctxt->vmap[7][5] = 3790;
        ctxt->vmap[7][6] = 3790;
        ctxt->vmap[7][7] = 3790;
        ctxt->vmap[7][8] = 3790;
        ctxt->vmap[8][0] = 4212;
        ctxt->vmap[8][1] = 4212;
        ctxt->vmap[8][2] = 4212;
        ctxt->vmap[8][3] = 4212;
        ctxt->vmap[8][4] = 4212;
        ctxt->vmap[8][5] = 4212;
        ctxt->vmap[8][6] = 4212;
        ctxt->vmap[8][7] = 4212;
        ctxt->vmap[8][8] = 4212;
        ctxt->vmap[9][0] = 4570;
        ctxt->vmap[9][1] = 4570;
        ctxt->vmap[9][2] = 4570;
        ctxt->vmap[9][3] = 4570;
        ctxt->vmap[9][4] = 4570;
        ctxt->vmap[9][5] = 4570;
        ctxt->vmap[9][6] = 4570;
        ctxt->vmap[9][7] = 4570;
        ctxt->vmap[9][8] = 4570;
        ctxt->vmap[10][0] = 4994;
        ctxt->vmap[10][1] = 4994;
        ctxt->vmap[10][2] = 4994;
        ctxt->vmap[10][3] = 4994;
        ctxt->vmap[10][4] = 4994;
        ctxt->vmap[10][5] = 4994;
        ctxt->vmap[10][6] = 4994;
        ctxt->vmap[10][7] = 4994;
        ctxt->vmap[10][8] = 4994;
        ctxt->vmap[11][0] = 4869;
        ctxt->vmap[11][1] = 4869;
        ctxt->vmap[11][2] = 4869;
        ctxt->vmap[11][3] = 4869;
        ctxt->vmap[11][4] = 4869;
        ctxt->vmap[11][5] = 4869;
        ctxt->vmap[11][6] = 4869;
        ctxt->vmap[11][7] = 4869;
        ctxt->vmap[11][8] = 4869;
        ctxt->vmap[12][0] = 4747;
        ctxt->vmap[12][1] = 4747;
        ctxt->vmap[12][2] = 4747;
        ctxt->vmap[12][3] = 4747;
        ctxt->vmap[12][4] = 4747;
        ctxt->vmap[12][5] = 4747;
        ctxt->vmap[12][6] = 4747;
        ctxt->vmap[12][7] = 4747;
        ctxt->vmap[12][8] = 4747;
        ctxt->vmap[13][0] = 4440;
        ctxt->vmap[13][1] = 4440;
        ctxt->vmap[13][2] = 4440;
        ctxt->vmap[13][3] = 4440;
        ctxt->vmap[13][4] = 4440;
        ctxt->vmap[13][5] = 4440;
        ctxt->vmap[13][6] = 4440;
        ctxt->vmap[13][7] = 4440;
        ctxt->vmap[13][8] = 4440;
        break;

    case 1: // train 47
        ctxt->measurements.front  = 16000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        ctxt->dmap.mega_scale       = 10000;
        ctxt->dmap.terms[3].factor  = 77;
        ctxt->dmap.terms[3].scale   = 10000;
        ctxt->dmap.terms[2].factor  = -15;
        ctxt->dmap.terms[2].scale   = 10000;
        ctxt->dmap.terms[1].factor  = 11863;
        ctxt->dmap.terms[1].scale   = 10000;
        ctxt->dmap.terms[0].factor  = 34;
        ctxt->dmap.terms[0].scale   = 1;

        ctxt->stop_dist_map.slope  =   6703;
        ctxt->stop_dist_map.offset = -38400;

        ctxt->start_dist_map.mega_scale      = 1;
        ctxt->start_dist_map.terms[3].factor = 5;
        ctxt->start_dist_map.terms[3].scale  = 10000;
        ctxt->start_dist_map.terms[2].factor = 1281;
        ctxt->start_dist_map.terms[2].scale  = 10000;
        ctxt->start_dist_map.terms[1].factor = 15428;
        ctxt->start_dist_map.terms[1].scale  = 1000;
        ctxt->start_dist_map.terms[0].factor = 241;
        ctxt->start_dist_map.terms[0].scale  = 1;

        ctxt->dmap.mega_scale      = 10000;
        ctxt->dmap.terms[3].factor = 113;
        ctxt->dmap.terms[3].scale  = 10000;
        ctxt->dmap.terms[2].factor = 19;
        ctxt->dmap.terms[2].scale  = 10000;
        ctxt->dmap.terms[1].factor = 12062;
        ctxt->dmap.terms[1].scale  = 10000;
        ctxt->dmap.terms[0].factor = 53;
        ctxt->dmap.terms[0].scale  = 1;

        ctxt->vmap[0][0] = 253;
        ctxt->vmap[0][1] = 253;
        ctxt->vmap[0][2] = 253;
        ctxt->vmap[0][3] = 253;
        ctxt->vmap[0][4] = 253;
        ctxt->vmap[0][5] = 253;
        ctxt->vmap[0][6] = 253;
        ctxt->vmap[0][7] = 253;
        ctxt->vmap[0][8] = 253;
        ctxt->vmap[1][0] = 755;
        ctxt->vmap[1][1] = 755;
        ctxt->vmap[1][2] = 755;
        ctxt->vmap[1][3] = 758;
        ctxt->vmap[1][4] = 757;
        ctxt->vmap[1][5] = 755;
        ctxt->vmap[1][6] = 757;
        ctxt->vmap[1][7] = 753;
        ctxt->vmap[1][8] = 759;
        ctxt->vmap[2][0] = 1284;
        ctxt->vmap[2][1] = 1284;
        ctxt->vmap[2][2] = 1284;
        ctxt->vmap[2][3] = 1284;
        ctxt->vmap[2][4] = 1284;
        ctxt->vmap[2][5] = 1284;
        ctxt->vmap[2][6] = 1284;
        ctxt->vmap[2][7] = 1284;
        ctxt->vmap[2][8] = 1284;
        ctxt->vmap[3][0] = 1690;
        ctxt->vmap[3][1] = 1659;
        ctxt->vmap[3][2] = 1454;
        ctxt->vmap[3][3] = 1682;
        ctxt->vmap[3][4] = 1456;
        ctxt->vmap[3][5] = 1692;
        ctxt->vmap[3][6] = 1700;
        ctxt->vmap[3][7] = 1667;
        ctxt->vmap[3][8] = 1729;
        ctxt->vmap[4][0] = 2370;
        ctxt->vmap[4][1] = 2364;
        ctxt->vmap[4][2] = 2364;
        ctxt->vmap[4][3] = 2366;
        ctxt->vmap[4][4] = 2358;
        ctxt->vmap[4][5] = 2364;
        ctxt->vmap[4][6] = 2367;
        ctxt->vmap[4][7] = 2369;
        ctxt->vmap[4][8] = 2373;
        ctxt->vmap[5][0] = 2921;
        ctxt->vmap[5][1] = 2921;
        ctxt->vmap[5][2] = 2921;
        ctxt->vmap[5][3] = 2907;
        ctxt->vmap[5][4] = 2906;
        ctxt->vmap[5][5] = 2922;
        ctxt->vmap[5][6] = 2919;
        ctxt->vmap[5][7] = 2892;
        ctxt->vmap[5][8] = 2907;
        ctxt->vmap[6][0] = 3488;
        ctxt->vmap[6][1] = 3488;
        ctxt->vmap[6][2] = 3488;
        ctxt->vmap[6][3] = 3488;
        ctxt->vmap[6][4] = 3488;
        ctxt->vmap[6][5] = 3488;
        ctxt->vmap[6][6] = 3488;
        ctxt->vmap[6][7] = 3488;
        ctxt->vmap[6][8] = 3488;
        ctxt->vmap[7][0] = 3666;
        ctxt->vmap[7][1] = 3696;
        ctxt->vmap[7][2] = 3697;
        ctxt->vmap[7][3] = 3723;
        ctxt->vmap[7][4] = 3726;
        ctxt->vmap[7][5] = 3736;
        ctxt->vmap[7][6] = 3735;
        ctxt->vmap[7][7] = 3725;
        ctxt->vmap[7][8] = 3765;
        ctxt->vmap[8][0] = 4341;
        ctxt->vmap[8][1] = 4341;
        ctxt->vmap[8][2] = 4341;
        ctxt->vmap[8][3] = 4341;
        ctxt->vmap[8][4] = 4341;
        ctxt->vmap[8][5] = 4341;
        ctxt->vmap[8][6] = 4341;
        ctxt->vmap[8][7] = 4341;
        ctxt->vmap[8][8] = 4341;
        ctxt->vmap[9][0] = 4673;
        ctxt->vmap[9][1] = 4673;
        ctxt->vmap[9][2] = 4673;
        ctxt->vmap[9][3] = 4643;
        ctxt->vmap[9][4] = 4603;
        ctxt->vmap[9][5] = 4619;
        ctxt->vmap[9][6] = 4594;
        ctxt->vmap[9][7] = 4634;
        ctxt->vmap[9][8] = 4665;
        ctxt->vmap[10][0] = 4931;
        ctxt->vmap[10][1] = 4817;
        ctxt->vmap[10][2] = 4787;
        ctxt->vmap[10][3] = 4746;
        ctxt->vmap[10][4] = 4931;
        ctxt->vmap[10][5] = 4901;
        ctxt->vmap[10][6] = 4792;
        ctxt->vmap[10][7] = 4761;
        ctxt->vmap[10][8] = 4931;
        ctxt->vmap[11][0] = 4967;
        ctxt->vmap[11][1] = 5109;
        ctxt->vmap[11][2] = 5053;
        ctxt->vmap[11][3] = 4761;
        ctxt->vmap[11][4] = 4915;
        ctxt->vmap[11][5] = 4796;
        ctxt->vmap[11][6] = 4699;
        ctxt->vmap[11][7] = 4722;
        ctxt->vmap[11][8] = 4819;
        ctxt->vmap[12][0] = 4931;
        ctxt->vmap[12][1] = 4931;
        ctxt->vmap[12][2] = 4931;
        ctxt->vmap[12][3] = 4931;
        ctxt->vmap[12][4] = 4931;
        ctxt->vmap[12][5] = 4931;
        ctxt->vmap[12][6] = 4931;
        ctxt->vmap[12][7] = 4931;
        ctxt->vmap[12][8] = 4931;
        ctxt->vmap[13][0] = 4808;
        ctxt->vmap[13][1] = 4808;
        ctxt->vmap[13][2] = 4808;
        ctxt->vmap[13][3] = 4808;
        ctxt->vmap[13][4] = 4808;
        ctxt->vmap[13][5] = 4808;
        ctxt->vmap[13][6] = 4808;
        ctxt->vmap[13][7] = 4787;
        ctxt->vmap[13][8] = 4808;
        break;

    case 2: // train 48
        ctxt->measurements.front  = 22000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        ctxt->dmap.mega_scale       = 10000;
        ctxt->dmap.terms[3].factor  = 77;
        ctxt->dmap.terms[3].scale   = 10000;
        ctxt->dmap.terms[2].factor  = -15;
        ctxt->dmap.terms[2].scale   = 10000;
        ctxt->dmap.terms[1].factor  = 11863;
        ctxt->dmap.terms[1].scale   = 10000;
        ctxt->dmap.terms[0].factor  = 34;
        ctxt->dmap.terms[0].scale   = 1;

        ctxt->stop_dist_map.slope  =   7357;
        ctxt->stop_dist_map.offset = -59043;

        ctxt->start_dist_map.mega_scale      = 1;
        ctxt->start_dist_map.terms[3].factor = 5;
        ctxt->start_dist_map.terms[3].scale  = 10000;
        ctxt->start_dist_map.terms[2].factor = 1281;
        ctxt->start_dist_map.terms[2].scale  = 10000;
        ctxt->start_dist_map.terms[1].factor = 15428;
        ctxt->start_dist_map.terms[1].scale  = 1000;
        ctxt->start_dist_map.terms[0].factor = 241;
        ctxt->start_dist_map.terms[0].scale  = 1;

        ctxt->dmap.mega_scale      = 10000;
        ctxt->dmap.terms[3].factor = 113;
        ctxt->dmap.terms[3].scale  = 10000;
        ctxt->dmap.terms[2].factor = 19;
        ctxt->dmap.terms[2].scale  = 10000;
        ctxt->dmap.terms[1].factor = 12062;
        ctxt->dmap.terms[1].scale  = 10000;
        ctxt->dmap.terms[0].factor = 53;
        ctxt->dmap.terms[0].scale  = 1;

        ctxt->vmap[0][0] = 244;
        ctxt->vmap[0][1] = 244;
        ctxt->vmap[0][2] = 244;
        ctxt->vmap[0][3] = 244;
        ctxt->vmap[0][4] = 244;
        ctxt->vmap[0][5] = 244;
        ctxt->vmap[0][6] = 244;
        ctxt->vmap[0][7] = 244;
        ctxt->vmap[0][8] = 244;
        ctxt->vmap[1][0] = 752;
        ctxt->vmap[1][1] = 763;
        ctxt->vmap[1][2] = 772;
        ctxt->vmap[1][3] = 750;
        ctxt->vmap[1][4] = 750;
        ctxt->vmap[1][5] = 750;
        ctxt->vmap[1][6] = 750;
        ctxt->vmap[1][7] = 750;
        ctxt->vmap[1][8] = 750;
        ctxt->vmap[2][0] = 1250;
        ctxt->vmap[2][1] = 1250;
        ctxt->vmap[2][2] = 1250;
        ctxt->vmap[2][3] = 1250;
        ctxt->vmap[2][4] = 1250;
        ctxt->vmap[2][5] = 1250;
        ctxt->vmap[2][6] = 1250;
        ctxt->vmap[2][7] = 1250;
        ctxt->vmap[2][8] = 1250;
        ctxt->vmap[3][0] = 1758;
        ctxt->vmap[3][1] = 1756;
        ctxt->vmap[3][2] = 1878;
        ctxt->vmap[3][3] = 1749;
        ctxt->vmap[3][4] = 1749;
        ctxt->vmap[3][5] = 1749;
        ctxt->vmap[3][6] = 1749;
        ctxt->vmap[3][7] = 1749;
        ctxt->vmap[3][8] = 1749;
        ctxt->vmap[4][0] = 2319;
        ctxt->vmap[4][1] = 2319;
        ctxt->vmap[4][2] = 2319;
        ctxt->vmap[4][3] = 2319;
        ctxt->vmap[4][4] = 2319;
        ctxt->vmap[4][5] = 2319;
        ctxt->vmap[4][6] = 2319;
        ctxt->vmap[4][7] = 2319;
        ctxt->vmap[4][8] = 2319;
        ctxt->vmap[5][0] = 2824;
        ctxt->vmap[5][1] = 2834;
        ctxt->vmap[5][2] = 2955;
        ctxt->vmap[5][3] = 2846;
        ctxt->vmap[5][4] = 2846;
        ctxt->vmap[5][5] = 2846;
        ctxt->vmap[5][6] = 2846;
        ctxt->vmap[5][7] = 2846;
        ctxt->vmap[5][8] = 2846;
        ctxt->vmap[6][0] = 3347;
        ctxt->vmap[6][1] = 3347;
        ctxt->vmap[6][2] = 3347;
        ctxt->vmap[6][3] = 3347;
        ctxt->vmap[6][4] = 3347;
        ctxt->vmap[6][5] = 3347;
        ctxt->vmap[6][6] = 3347;
        ctxt->vmap[6][7] = 3347;
        ctxt->vmap[6][8] = 3347;
        ctxt->vmap[7][0] = 3761;
        ctxt->vmap[7][1] = 3866;
        ctxt->vmap[7][2] = 3922;
        ctxt->vmap[7][3] = 3748;
        ctxt->vmap[7][4] = 3748;
        ctxt->vmap[7][5] = 3748;
        ctxt->vmap[7][6] = 3748;
        ctxt->vmap[7][7] = 3748;
        ctxt->vmap[7][8] = 3748;
        ctxt->vmap[8][0] = 4116;
        ctxt->vmap[8][1] = 4116;
        ctxt->vmap[8][2] = 4116;
        ctxt->vmap[8][3] = 4116;
        ctxt->vmap[8][4] = 4116;
        ctxt->vmap[8][5] = 4116;
        ctxt->vmap[8][6] = 4116;
        ctxt->vmap[8][7] = 4116;
        ctxt->vmap[8][8] = 4116;
        ctxt->vmap[9][0] = 4579;
        ctxt->vmap[9][1] = 4595;
        ctxt->vmap[9][2] = 4857;
        ctxt->vmap[9][3] = 4597;
        ctxt->vmap[9][4] = 4597;
        ctxt->vmap[9][5] = 4597;
        ctxt->vmap[9][6] = 4597;
        ctxt->vmap[9][7] = 4597;
        ctxt->vmap[9][8] = 4597;
        ctxt->vmap[10][0] = 5060;
        ctxt->vmap[10][1] = 5060;
        ctxt->vmap[10][2] = 5060;
        ctxt->vmap[10][3] = 5060;
        ctxt->vmap[10][4] = 5060;
        ctxt->vmap[10][5] = 5060;
        ctxt->vmap[10][6] = 5060;
        ctxt->vmap[10][7] = 5060;
        ctxt->vmap[10][8] = 5060;
        ctxt->vmap[11][0] = 5193;
        ctxt->vmap[11][1] = 5339;
        ctxt->vmap[11][2] = 5447;
        ctxt->vmap[11][3] = 5161;
        ctxt->vmap[11][4] = 5161;
        ctxt->vmap[11][5] = 5161;
        ctxt->vmap[11][6] = 5161;
        ctxt->vmap[11][7] = 5161;
        ctxt->vmap[11][8] = 5161;
        ctxt->vmap[12][0] = 5044;
        ctxt->vmap[12][1] = 5044;
        ctxt->vmap[12][2] = 5044;
        ctxt->vmap[12][3] = 5044;
        ctxt->vmap[12][4] = 5044;
        ctxt->vmap[12][5] = 5044;
        ctxt->vmap[12][6] = 5044;
        ctxt->vmap[12][7] = 5044;
        ctxt->vmap[12][8] = 5044;
        ctxt->vmap[13][0] = 4793;
        ctxt->vmap[13][1] = 4793;
        ctxt->vmap[13][2] = 4793;
        ctxt->vmap[13][3] = 4793;
        ctxt->vmap[13][4] = 4793;
        ctxt->vmap[13][5] = 4793;
        ctxt->vmap[13][6] = 4793;
        ctxt->vmap[13][7] = 4793;
        ctxt->vmap[13][8] = 4793;
        break;

    case 3: // Train 49
        ctxt->measurements.front  = 23000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        ctxt->dmap.mega_scale       = 10000;
        ctxt->dmap.terms[3].factor  = 77;
        ctxt->dmap.terms[3].scale   = 10000;
        ctxt->dmap.terms[2].factor  = -15;
        ctxt->dmap.terms[2].scale   = 10000;
        ctxt->dmap.terms[1].factor  = 11863;
        ctxt->dmap.terms[1].scale   = 10000;
        ctxt->dmap.terms[0].factor  = 34;
        ctxt->dmap.terms[0].scale   = 1;

        ctxt->stop_dist_map.slope  =   6391;
        ctxt->stop_dist_map.offset = -41611;

        ctxt->start_dist_map.mega_scale      = 1;
        ctxt->start_dist_map.terms[3].factor = 5;
        ctxt->start_dist_map.terms[3].scale  = 10000;
        ctxt->start_dist_map.terms[2].factor = 1281;
        ctxt->start_dist_map.terms[2].scale  = 10000;
        ctxt->start_dist_map.terms[1].factor = 15428;
        ctxt->start_dist_map.terms[1].scale  = 1000;
        ctxt->start_dist_map.terms[0].factor = 241;
        ctxt->start_dist_map.terms[0].scale  = 1;

        ctxt->dmap.mega_scale      = 10000;
        ctxt->dmap.terms[3].factor = 113;
        ctxt->dmap.terms[3].scale  = 10000;
        ctxt->dmap.terms[2].factor = 19;
        ctxt->dmap.terms[2].scale  = 10000;
        ctxt->dmap.terms[1].factor = 12062;
        ctxt->dmap.terms[1].scale  = 10000;
        ctxt->dmap.terms[0].factor = 53;
        ctxt->dmap.terms[0].scale  = 1;

        ctxt->vmap[0][0] = 250;
        ctxt->vmap[0][1] = 250;
        ctxt->vmap[0][2] = 250;
        ctxt->vmap[0][3] = 250;
        ctxt->vmap[0][4] = 250;
        ctxt->vmap[0][5] = 250;
        ctxt->vmap[0][6] = 250;
        ctxt->vmap[0][7] = 250;
        ctxt->vmap[0][8] = 250;
        ctxt->vmap[1][0] = 796;
        ctxt->vmap[1][1] = 796;
        ctxt->vmap[1][2] = 796;
        ctxt->vmap[1][3] = 796;
        ctxt->vmap[1][4] = 796;
        ctxt->vmap[1][5] = 796;
        ctxt->vmap[1][6] = 796;
        ctxt->vmap[1][7] = 796;
        ctxt->vmap[1][8] = 796;
        ctxt->vmap[2][0] = 1316;
        ctxt->vmap[2][1] = 1316;
        ctxt->vmap[2][2] = 1316;
        ctxt->vmap[2][3] = 1316;
        ctxt->vmap[2][4] = 1316;
        ctxt->vmap[2][5] = 1316;
        ctxt->vmap[2][6] = 1316;
        ctxt->vmap[2][7] = 1316;
        ctxt->vmap[2][8] = 1316;
        ctxt->vmap[3][0] = 1820;
        ctxt->vmap[3][1] = 1820;
        ctxt->vmap[3][2] = 1820;
        ctxt->vmap[3][3] = 1820;
        ctxt->vmap[3][4] = 1820;
        ctxt->vmap[3][5] = 1820;
        ctxt->vmap[3][6] = 1820;
        ctxt->vmap[3][7] = 1820;
        ctxt->vmap[3][8] = 1820;
        ctxt->vmap[4][0] = 2330;
        ctxt->vmap[4][1] = 2330;
        ctxt->vmap[4][2] = 2330;
        ctxt->vmap[4][3] = 2330;
        ctxt->vmap[4][4] = 2330;
        ctxt->vmap[4][5] = 2330;
        ctxt->vmap[4][6] = 2330;
        ctxt->vmap[4][7] = 2330;
        ctxt->vmap[4][8] = 2330;
        ctxt->vmap[5][0] = 2861;
        ctxt->vmap[5][1] = 2861;
        ctxt->vmap[5][2] = 2861;
        ctxt->vmap[5][3] = 2861;
        ctxt->vmap[5][4] = 2861;
        ctxt->vmap[5][5] = 2861;
        ctxt->vmap[5][6] = 2861;
        ctxt->vmap[5][7] = 2861;
        ctxt->vmap[5][8] = 2861;
        ctxt->vmap[6][0] = 3438;
        ctxt->vmap[6][1] = 3438;
        ctxt->vmap[6][2] = 3438;
        ctxt->vmap[6][3] = 3438;
        ctxt->vmap[6][4] = 3438;
        ctxt->vmap[6][5] = 3438;
        ctxt->vmap[6][6] = 3438;
        ctxt->vmap[6][7] = 3438;
        ctxt->vmap[6][8] = 3438;
        ctxt->vmap[7][0] = 3967;
        ctxt->vmap[7][1] = 3967;
        ctxt->vmap[7][2] = 3967;
        ctxt->vmap[7][3] = 3967;
        ctxt->vmap[7][4] = 3967;
        ctxt->vmap[7][5] = 3967;
        ctxt->vmap[7][6] = 3967;
        ctxt->vmap[7][7] = 3967;
        ctxt->vmap[7][8] = 3967;
        ctxt->vmap[8][0] = 4482;
        ctxt->vmap[8][1] = 4482;
        ctxt->vmap[8][2] = 4482;
        ctxt->vmap[8][3] = 4482;
        ctxt->vmap[8][4] = 4482;
        ctxt->vmap[8][5] = 4482;
        ctxt->vmap[8][6] = 4482;
        ctxt->vmap[8][7] = 4482;
        ctxt->vmap[8][8] = 4482;
        ctxt->vmap[9][0] = 5130;
        ctxt->vmap[9][1] = 5130;
        ctxt->vmap[9][2] = 5130;
        ctxt->vmap[9][3] = 5130;
        ctxt->vmap[9][4] = 5130;
        ctxt->vmap[9][5] = 5130;
        ctxt->vmap[9][6] = 5130;
        ctxt->vmap[9][7] = 5130;
        ctxt->vmap[9][8] = 5130;
        ctxt->vmap[10][0] = 5556;
        ctxt->vmap[10][1] = 5556;
        ctxt->vmap[10][2] = 5556;
        ctxt->vmap[10][3] = 5556;
        ctxt->vmap[10][4] = 5556;
        ctxt->vmap[10][5] = 5556;
        ctxt->vmap[10][6] = 5556;
        ctxt->vmap[10][7] = 5556;
        ctxt->vmap[10][8] = 5556;
        ctxt->vmap[11][0] = 5598;
        ctxt->vmap[11][1] = 5598;
        ctxt->vmap[11][2] = 5598;
        ctxt->vmap[11][3] = 5598;
        ctxt->vmap[11][4] = 5598;
        ctxt->vmap[11][5] = 5598;
        ctxt->vmap[11][6] = 5598;
        ctxt->vmap[11][7] = 5598;
        ctxt->vmap[11][8] = 5598;
        ctxt->vmap[12][0] = 5324;
        ctxt->vmap[12][1] = 5324;
        ctxt->vmap[12][2] = 5324;
        ctxt->vmap[12][3] = 5324;
        ctxt->vmap[12][4] = 5324;
        ctxt->vmap[12][5] = 5324;
        ctxt->vmap[12][6] = 5324;
        ctxt->vmap[12][7] = 5324;
        ctxt->vmap[12][8] = 5324;
        ctxt->vmap[13][0] = 5093;
        ctxt->vmap[13][1] = 5093;
        ctxt->vmap[13][2] = 5093;
        ctxt->vmap[13][3] = 5093;
        ctxt->vmap[13][4] = 5093;
        ctxt->vmap[13][5] = 5093;
        ctxt->vmap[13][6] = 5093;
        ctxt->vmap[13][7] = 5093;
        ctxt->vmap[13][8] = 5093;
        break;

    default: // just to get us started on new trains
        ctxt->measurements.front  = 23000;
        ctxt->measurements.pickup = 51000;
        ctxt->measurements.back   = 142000;

        // stopping distance vs. stopping time
        ctxt->dmap.mega_scale       = 10000;
        ctxt->dmap.terms[3].factor  = 77;
        ctxt->dmap.terms[3].scale   = 10000;
        ctxt->dmap.terms[2].factor  = -15;
        ctxt->dmap.terms[2].scale   = 10000;
        ctxt->dmap.terms[1].factor  = 11863;
        ctxt->dmap.terms[1].scale   = 10000;
        ctxt->dmap.terms[0].factor  = 34;
        ctxt->dmap.terms[0].scale   = 1;

        // stopping distance function
        ctxt->stop_dist_map.slope  =   6391;
        ctxt->stop_dist_map.offset = -41611;

        ctxt->start_dist_map.mega_scale      = 1;
        ctxt->start_dist_map.terms[3].factor = 5;
        ctxt->start_dist_map.terms[3].scale  = 10000;
        ctxt->start_dist_map.terms[2].factor = 1281;
        ctxt->start_dist_map.terms[2].scale  = 10000;
        ctxt->start_dist_map.terms[1].factor = 15428;
        ctxt->start_dist_map.terms[1].scale  = 1000;
        ctxt->start_dist_map.terms[0].factor = 241;
        ctxt->start_dist_map.terms[0].scale  = 1;

        ctxt->dmap.mega_scale      = 10000;
        ctxt->dmap.terms[3].factor = 113;
        ctxt->dmap.terms[3].scale  = 10000;
        ctxt->dmap.terms[2].factor = 19;
        ctxt->dmap.terms[2].scale  = 10000;
        ctxt->dmap.terms[1].factor = 12062;
        ctxt->dmap.terms[1].scale  = 10000;
        ctxt->dmap.terms[0].factor = 53;
        ctxt->dmap.terms[0].scale  = 1;

        ctxt->vmap[0][0] = 250;
        ctxt->vmap[0][1] = 250;
        ctxt->vmap[0][2] = 250;
        ctxt->vmap[0][3] = 250;
        ctxt->vmap[0][4] = 250;
        ctxt->vmap[0][5] = 250;
        ctxt->vmap[0][6] = 250;
        ctxt->vmap[0][7] = 250;
        ctxt->vmap[0][8] = 250;
        ctxt->vmap[1][0] = 796;
        ctxt->vmap[1][1] = 796;
        ctxt->vmap[1][2] = 796;
        ctxt->vmap[1][3] = 796;
        ctxt->vmap[1][4] = 796;
        ctxt->vmap[1][5] = 796;
        ctxt->vmap[1][6] = 796;
        ctxt->vmap[1][7] = 796;
        ctxt->vmap[1][8] = 796;
        ctxt->vmap[2][0] = 1316;
        ctxt->vmap[2][1] = 1316;
        ctxt->vmap[2][2] = 1316;
        ctxt->vmap[2][3] = 1316;
        ctxt->vmap[2][4] = 1316;
        ctxt->vmap[2][5] = 1316;
        ctxt->vmap[2][6] = 1316;
        ctxt->vmap[2][7] = 1316;
        ctxt->vmap[2][8] = 1316;
        ctxt->vmap[3][0] = 1820;
        ctxt->vmap[3][1] = 1820;
        ctxt->vmap[3][2] = 1820;
        ctxt->vmap[3][3] = 1820;
        ctxt->vmap[3][4] = 1820;
        ctxt->vmap[3][5] = 1820;
        ctxt->vmap[3][6] = 1820;
        ctxt->vmap[3][7] = 1820;
        ctxt->vmap[3][8] = 1820;
        ctxt->vmap[4][0] = 2330;
        ctxt->vmap[4][1] = 2330;
        ctxt->vmap[4][2] = 2330;
        ctxt->vmap[4][3] = 2330;
        ctxt->vmap[4][4] = 2330;
        ctxt->vmap[4][5] = 2330;
        ctxt->vmap[4][6] = 2330;
        ctxt->vmap[4][7] = 2330;
        ctxt->vmap[4][8] = 2330;
        ctxt->vmap[5][0] = 2861;
        ctxt->vmap[5][1] = 2861;
        ctxt->vmap[5][2] = 2861;
        ctxt->vmap[5][3] = 2861;
        ctxt->vmap[5][4] = 2861;
        ctxt->vmap[5][5] = 2861;
        ctxt->vmap[5][6] = 2861;
        ctxt->vmap[5][7] = 2861;
        ctxt->vmap[5][8] = 2861;
        ctxt->vmap[6][0] = 3438;
        ctxt->vmap[6][1] = 3438;
        ctxt->vmap[6][2] = 3438;
        ctxt->vmap[6][3] = 3438;
        ctxt->vmap[6][4] = 3438;
        ctxt->vmap[6][5] = 3438;
        ctxt->vmap[6][6] = 3438;
        ctxt->vmap[6][7] = 3438;
        ctxt->vmap[6][8] = 3438;
        ctxt->vmap[7][0] = 3967;
        ctxt->vmap[7][1] = 3967;
        ctxt->vmap[7][2] = 3967;
        ctxt->vmap[7][3] = 3967;
        ctxt->vmap[7][4] = 3967;
        ctxt->vmap[7][5] = 3967;
        ctxt->vmap[7][6] = 3967;
        ctxt->vmap[7][7] = 3967;
        ctxt->vmap[7][8] = 3967;
        ctxt->vmap[8][0] = 4482;
        ctxt->vmap[8][1] = 4482;
        ctxt->vmap[8][2] = 4482;
        ctxt->vmap[8][3] = 4482;
        ctxt->vmap[8][4] = 4482;
        ctxt->vmap[8][5] = 4482;
        ctxt->vmap[8][6] = 4482;
        ctxt->vmap[8][7] = 4482;
        ctxt->vmap[8][8] = 4482;
        ctxt->vmap[9][0] = 5130;
        ctxt->vmap[9][1] = 5130;
        ctxt->vmap[9][2] = 5130;
        ctxt->vmap[9][3] = 5130;
        ctxt->vmap[9][4] = 5130;
        ctxt->vmap[9][5] = 5130;
        ctxt->vmap[9][6] = 5130;
        ctxt->vmap[9][7] = 5130;
        ctxt->vmap[9][8] = 5130;
        ctxt->vmap[10][0] = 5556;
        ctxt->vmap[10][1] = 5556;
        ctxt->vmap[10][2] = 5556;
        ctxt->vmap[10][3] = 5556;
        ctxt->vmap[10][4] = 5556;
        ctxt->vmap[10][5] = 5556;
        ctxt->vmap[10][6] = 5556;
        ctxt->vmap[10][7] = 5556;
        ctxt->vmap[10][8] = 5556;
        ctxt->vmap[11][0] = 5598;
        ctxt->vmap[11][1] = 5598;
        ctxt->vmap[11][2] = 5598;
        ctxt->vmap[11][3] = 5598;
        ctxt->vmap[11][4] = 5598;
        ctxt->vmap[11][5] = 5598;
        ctxt->vmap[11][6] = 5598;
        ctxt->vmap[11][7] = 5598;
        ctxt->vmap[11][8] = 5598;
        ctxt->vmap[12][0] = 5324;
        ctxt->vmap[12][1] = 5324;
        ctxt->vmap[12][2] = 5324;
        ctxt->vmap[12][3] = 5324;
        ctxt->vmap[12][4] = 5324;
        ctxt->vmap[12][5] = 5324;
        ctxt->vmap[12][6] = 5324;
        ctxt->vmap[12][7] = 5324;
        ctxt->vmap[12][8] = 5324;
        ctxt->vmap[13][0] = 5093;
        ctxt->vmap[13][1] = 5093;
        ctxt->vmap[13][2] = 5093;
        ctxt->vmap[13][3] = 5093;
        ctxt->vmap[13][4] = 5093;
        ctxt->vmap[13][5] = 5093;
        ctxt->vmap[13][6] = 5093;
        ctxt->vmap[13][7] = 5093;
        ctxt->vmap[13][8] = 5093;
        break;
    }
}

#endif
