
#ifndef __TRAIN_DRIVER_TYPES_H__
#define __TRAIN_DRIVER_TYPES_H__

#include <std.h>
#include <train.h>

#define MAX_PATH_COMMANDS 32

typedef enum {
    SENSOR_COMMAND,
    REVERSE_COMMAND,
    TURNOUT_COMMAND
} command_type;

typedef struct {
    int sensor;
    int dist; // distance to next sensor
} sensor_command;

typedef struct {
    int distance; // offset from previous location
    int reserved; // crap space
} reverse_command;

typedef struct {
    int number;
    int direction;
} turnout_command;

typedef union {
    sensor_command  sensor;
    reverse_command reverse;
    turnout_command turnout;
} command_body;

typedef struct {
    command_type type;
    command_body data;
} command;

typedef struct {
    int count;
    int dist_total;
    command cmd[MAX_PATH_COMMANDS];
} path;

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

    bool pathing;
    path path;
} train_context;

#endif
