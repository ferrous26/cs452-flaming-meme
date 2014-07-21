
#ifndef __BLASTER_T__
#define __BLASTER_T__

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
    TRACK_BUTT,
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

typedef struct blaster_context {
    int       train_id;      // internal index
    int       train_gid;     // global identifier
    char      name[8];

    int       master_courier;       // courier to the master
    int       acceleration_courier; // acceleration delay courier
    int       reverse_courier;
    int       checkpoint_courier;   // courier used to wake at checkpoints

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

    int       short_moving_distance;

    // these will usually be based on actual track feedback, unless we have to
    // go a while without track feedback, in which case this will be estimates

    bool      last_sensor_accelerating;
    int       last_sensor;      // previous-previous sensor hit
    int       last_distance;    // distance from previous-previous sensor to current_sensor
    int       last_time;

    bool      current_sensor_accelerating;
    int       current_sensor;   // sensor we are currently travelling through
    int       current_distance; // distance from current_sensor to next_sensor

    int       current_offset;   // offset from last sensor when last event occurred
    int       current_time;     // time when event happened (at offset)

    int       last_speed;       // speed of train at last event
    int       current_speed;

    train_dir direction;
    int       reversing;         // reverse step counter
    int       accelerating;      // accelerating or deccelerating state

    // these are estimates
    int       next_sensor;       // expected next sensor hit
    int       next_time;         // estimated time of arrival at next landmark

    const track_node* const track;
} blaster;

#endif
