
#ifndef __TRAIN_DRIVER_TYPES_H__
#define __TRAIN_DRIVER_TYPES_H__

#include <std.h>
#include <train.h>
#include <path.h>

#define PATH_MAX 100

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

    int courier;            // courier used for train delays

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

    int       path;
    int       path_dist;
    int       path_past_end;
    path_node steps[PATH_MAX];

    int stop_offset;
} train_context;

#endif
