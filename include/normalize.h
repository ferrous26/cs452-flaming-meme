
#ifndef __SENSOR_NAME_H__
#define __SENSOR_NAME_H__

#include <std.h>
#include <train.h>
#include <vt100.h>

typedef enum {
    INVALID_TRAIN          = -100,
    INVALID_SPEED          = -101,
    INVALID_SENSOR         = -102,
    INVALID_TURNOUT        = -103,
    INVALID_FEEDBACK_ALPHA = -104,
    INVALID_TRAIN_TWEAK    = -105
} control_err;

typedef enum {
    HALF_AND_HALF,
    EIGHTY_TWENTY,
    NINTY_TEN
} ratio;

typedef struct {
    int16 bank;
    int16 num;
} sensor;

typedef struct {
    int16 num;
    int16 state;
} turnout;


static inline int reverse_sensor(const int s) {
    if (mod2_int(s, 2) == 1) // if it is odd (remember, 0 indexing)
        return s - 1; // then the reverse sensor is + 1
    return s + 1; // otherwise it is - 1
}

static inline int __attribute__ ((const))
sensor_to_pos(const int bank, const int num) {
    return ((bank - 'A') << 4) + (num - 1);
}

static inline sensor __attribute__ ((const))
pos_to_sensor(const int num) {
    sensor name = {
        .bank = (short) 'A' + (short) (num >> 4),
        .num  = (short) mod2_int(num, 16) + 1
    };
    return name;
}

static inline const char* __attribute__ ((const))
train_to_colour(const int train) {
    switch (train) {
    case 45: return MAGENTA;
    case 48: return LIGHT_MAGENTA;  // need a better colour?
    case 56: return RED;
    case 54: return LIGHT_BLUE;
    case 58: return BLUE;
    default: return WHITE;
    }
}

static inline int __attribute__ ((const))
train_to_pos(const int train) {
    switch (train) {
    case 45: return 0;
    case 48: return 1;
    case 56: return 2;
    case 54: return 3;
    case 58: return 4;
    default: return INVALID_TRAIN;
    }
}

static inline int __attribute__ ((const))
pos_to_train(const int pos) {
    switch (pos) {
    case 0: return 45;
    case 1: return 48;
    case 2: return 56;
    case 3: return 54;
    case 4: return 58;
    default: return INVALID_TRAIN;
    }
}

static inline int __attribute__ ((const))
turnout_to_pos(const int turn) {
    if (turn <  1)   return INVALID_TURNOUT;
    if (turn <= 18)  return turn - 1;
    if (turn <  153) return INVALID_TURNOUT;
    if (turn <= 156) return turn - (153-18);
    return INVALID_TURNOUT;
}

static inline int __attribute__ ((const))
pos_to_turnout(const int pos) {
    if (pos < 0)  return INVALID_TURNOUT;
    if (pos < 18) return pos + 1;
    if (pos < 22) return pos + (153-18);
    return INVALID_TURNOUT;
}

static inline int __attribute__ ((const))
alpha_to_pos(const int alpha) {
    switch (alpha) {
    case 0:  return HALF_AND_HALF;
    case 1:  return NINTY_TEN;
    default: return INVALID_FEEDBACK_ALPHA;
    }
}

static inline int __attribute__ ((const))
pos_to_alpha(const int alpha) {
    return alpha;
}

#endif
