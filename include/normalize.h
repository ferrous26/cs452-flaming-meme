
#ifndef __SENSOR_NAME_H__
#define __SENSOR_NAME_H__

#include <std.h>
#include <train.h>

typedef enum {
    INVALID_TRAIN          = -100,
    INVALID_SPEED          = -101,
    INVALID_SENSOR         = -102,
    INVALID_TURNOUT        = -103,
    INVALID_FEEDBACK_ALPHA = -104,
} blaster_err;

typedef enum {
    HALF_AND_HALF,
    NINTY_TEN
} feedback_level;

typedef struct {
    int16 bank;
    int16 num;
} sensor;

typedef struct {
    int16 num;
    int16 state;
} turnout;


static inline int __attribute__ ((const))
sensor_to_pos(const int bank, const int num) {
    return ((bank - 'A') << 4) + (num - 1);
}

static inline sensor __attribute__ ((const))
pos_to_sensor(const int num) {
    sensor name = {
        .bank = 'A' + (num >> 4),
        .num  = mod2(num, 16) + 1
    };
    return name;
}


static inline int __attribute__ ((const))
train_to_pos(const int train) {
    switch (train) {
    case 45: return 0;
    case 47: return 1;
    case 48: return 2;
    case 49: return 3;
    case 51: return 4;
    default: return INVALID_TRAIN;
    }
}

static inline int __attribute__ ((const))
pos_to_train(const int pos) {
    switch (pos) {
    case 0: return 45;
    case 1: return 47;
    case 2: return 48;
    case 3: return 49;
    case 4: return 51;
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
    case 0: return HALF_AND_HALF;
    case 1: return NINTY_TEN;
    default: return INVALID_FEEDBACK_ALPHA;
    }
}

static inline int __attribute__ ((const))
pos_to_alpha(const int alpha) {
    return alpha;
}

#endif
