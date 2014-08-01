
#ifndef __TRAIN_BLASTER_PHYSICS_H__
#define __TRAIN_BLASTER_PHYSICS_H__

static inline track_type __attribute__ ((const))
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
    case 53: return TRACK_BUTT;
    case 54: return TRACK_BUTT;
    case 55: return TRACK_OUTER_CURVE1;
    case 56: return TRACK_OUTER_CURVE1;
    case 57: return TRACK_BUTT;
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
    case 72: return TRACK_BUTT;
    case 73: return TRACK_INNER_CURVE;
    case 74: return TRACK_OUTER_CURVE1;
    case 75: return TRACK_BACK_CONNECT;
    case 76: return TRACK_INNER_CURVE;
    case 77: return TRACK_INNER_CURVE;
    case 78: return TRACK_INNER_CURVE;
    case 79: return TRACK_INNER_CURVE;
    default:
        return TRACK_STRAIGHT;
    }
}

static inline int
physics_velocity(const blaster* const ctxt,
                 const int speed,
                 const track_type type) {

    if (speed < 10) return 0;
    return ctxt->vmap[(speed / 10) - 1][type];
}

static inline int
physics_stopping_distance(const blaster* const ctxt, const int speed) {

    if (speed < 10) return 0;
    return
        (ctxt->stop_dist_map.slope * speed) +
        ctxt->stop_dist_map.offset +
        ctxt->stopping_distance_offset;
}

static inline int
physics_stopping_time(const blaster* const ctxt, const int stop_dist) {

    if (stop_dist == 0) return 0;

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

static inline int
physics_starting_distance(const blaster* const ctxt, const int speed) {

    if (speed < 10) return 0;

    const cubic* const map = &ctxt->start_dist_map;

    int sum =
        (((map->terms[3].factor * speed * speed) /
          map->terms[3].scale) * speed) / map->mega_scale;

    sum += (map->terms[2].factor * speed * speed) / map->terms[2].scale;

    sum += (map->terms[1].factor * speed) / map->terms[1].scale;

    sum += map->terms[0].factor / map->terms[0].scale;

    return sum;
}

static inline int
physics_starting_time(const blaster* const ctxt, const int start_dist) {

    if (start_dist == 0) return 0;

    const int dist = start_dist / 1000;
    const cubic* const map = &ctxt->amap;

    int sum =
        (((map->terms[3].factor * dist * dist) /
          map->terms[3].scale) * dist) / map->mega_scale;

    sum += (map->terms[2].factor * dist * dist) / map->terms[2].scale;

    sum += (map->terms[1].factor * dist) / map->terms[1].scale;

    sum += map->terms[0].factor / map->terms[0].scale;

    return sum + ctxt->acceleration_time_fudge_factor;
}

#endif
