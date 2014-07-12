
#ifndef __MASTER_T__
#define __MASTER_T__

#include <std.h>

#define FEEDBACK_THRESHOLD_DEFAULT        10000 // um/s
#define STOPPING_DISTANCE_OFFSET_DEFAULT  0     // um
#define TURNOUT_CLEARANCE_OFFSET_DEFAULT  50000 // um
#define STOPPING_TIME_FUDGE_FACTOR        10    // cs

typedef enum {
    TRACK_STRAIGHT,
    TRACK_STRAIGHT_TURNOUT,
    TRACK_CURVED,
    TRACK_CURVED_TURNOUT,
    TRACK_TYPE_COUNT
} track_type;

typedef enum {
    LM_SENSOR,
    LM_BRANCH,
    LM_MERGE,
    LM_EXIT
} landmark;

typedef struct {
    int slope;
    int offset;
    int delta;
} velocity;

typedef struct {
    int slope;
    int offset;
} stop;

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

typedef struct {
    int       train_id;      // internal index
    int       train_gid;     // global identifier
    char      name[8];

    int       acceleration_courier; // tid of the acceleration delay courier
    int       sensor_courier;       // tid of the blaster courier

    ratio     feedback_ratio;
    int       feedback_threshold;
    int       stopping_distance_offset;
    int       turnout_clearance_offset;
    int       stopping_time_fudge_factor;

    train_dim measurements;
    cubic     amap;
    velocity  vmap[TRACK_TYPE_COUNT];
    stop      smap;

    // these will usually be based on actual track feedback, unless we have to
    // go a while without track feedback, in which case this will be estimates
    landmark  last_landmark;
    int       last_sensor;   // not necessarily the same thing
    bool      last_state_accelerating;
    int       last_distance; // from last landmark to expected next landmark
    int       last_time;     // time at which the last landmark was hit
    int       last_velocity;
    int       last_speed;

    landmark  current_landmark;
    int       current_sensor;
    bool      current_state_accelerating;
    int       current_distance;
    int       current_time;
    int       current_velocity;
    int       current_speed;

    // these are estimates
    landmark  next_landmark; // expected next landmark
    int       next_sensor;
    int       next_distance; // expected dist. to next landmark from last
    int       next_time;     // estimated time of arrival at next landmark
    int       next_velocity;
} master;

#endif
