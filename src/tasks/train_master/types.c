
#ifndef __MASTER_T__
#define __MASTER_T__

#include <std.h>
#include <track_node.h>

#define FEEDBACK_THRESHOLD_DEFAULT        10000 // um/s
#define STOPPING_DISTANCE_OFFSET_DEFAULT  0     // um
#define TURNOUT_CLEARANCE_OFFSET_DEFAULT  50000 // um
#define REVERSE_TIME_FUDGE_FACTOR         10    // cs
#define STARTING_DISTANCE_OFFSET_DEFAULT  0     // um
#define ACCELERATION_TIME_FUDGE_DEFAULT   20    // cs

typedef enum {
    TRACK_THREE_WAY,
    TRACK_INNER_CURVE,
    TRACK_INNER_STRAIGHT,
    TRACK_OUTER_CURVE1,
    TRACK_OUTER_CURVE2,
    TRACK_STRAIGHT,
    TRACK_LONG_STRAIGHT,
    TRACK_BACK_CONNECT,
    TRACK_OUTER_STRAIGHT,
    TRACK_TYPE_COUNT
} track_type;

typedef enum {
    DIRECTION_BACKWARD = -1,
    DIRECTION_UNKNOWN  = 0,
    DIRECTION_FORWARD  = 1,
} train_dir;

typedef struct {
    int slope;
    int offset;
} line;

typedef struct {
    int front;
    int pickup;
    int back;
} train_dim;

typedef struct {
    int factor;
    int scale;
} term;

typedef struct {
    term terms[4];
    int mega_scale;
} cubic;

typedef struct master_context {
    int       train_id;      // internal index
    int       train_gid;     // global identifier
    char      name[8];

    int       acceleration_courier; // acceleration delay courier
    int       checkpoint_courier;   // courier used to wake at checkpoints
    int       sensor_courier;       // tid of the blaster courier
    int       sensor_to_stop_at;    // special case for handling ss command

    ratio     feedback_ratio;
    int       feedback_threshold;
    int       stopping_distance_offset;
    int       turnout_clearance_offset;
    int       reverse_time_fudge_factor;
    int       starting_distance_offset;
    int       acceleration_time_fudge_factor;

    train_dim measurements;
    cubic     dmap; // map decceleration time to stopping distance
    cubic     amap; // map acceleration time to a starting distance

    int       vmap[TRAIN_SPEEDS][TRACK_TYPE_COUNT];
    line      stop_dist_map;
    cubic     start_dist_map;

    bool      simulating;
    int       path_finding_steps;
    int       short_moving_distance;

    // these will usually be based on actual track feedback, unless we have to
    // go a while without track feedback, in which case this will be estimates

    int       last_sensor;      // previous-previous sensor hit
    int       last_distance;    // distance from previous-previous sensor to current_sensor

    int       last_offset;      // offset of last event between last_sensor and current_sensor
    int       last_time;        // time at last event between

    int       current_sensor;   // sensor we hit last
    int       current_distance; // distance from current_sensor to next_sensor

    int       current_offset;   // offset from last sensor when last event occurred
    int       current_time;     // time when event happened (at offset)

    int       last_speed;       // speed of train at last event
    int       current_speed;

    train_dir direction;
    int       reversing;         // reverse step counter
    int       accelerating;      // accelerating or deccelerating state

    // these are estimates
    int       next_sensor;       // expected next sensor
    int       next_time;         // estimated time of arrival at next landmark

    const track_node* const track;
} master;

#endif
