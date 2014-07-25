
#ifndef __TRAIN_BLASTER_TYPES_H__
#define __TRAIN_BLASTER_TYPES_H__

#include <std.h>
#include <track_node.h>

#define FEEDBACK_THRESHOLD_DEFAULT        10000 // um/s
#define STOPPING_DISTANCE_OFFSET_DEFAULT  0     // um
#define REVERSE_TIME_FUDGE_FACTOR         10    // cs
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

typedef struct {
    train_event    event;            // event that caused this state
    int            timestamp;

    bool           is_accelerating;  // acceleration going on during this event?
    track_location location;         // location of the state
    int            distance;         // distance from previous event location

    int            speed;            // speed setting of the train
    train_dir      direction;        // direction of the train during event

    track_location next_location;    // expected next location for same event type
    int            next_distance;    // total distance from current location to next
    int            next_timestamp;
    int            next_speed;
} train_state;

typedef struct blaster_context {
    int       train_id;      // internal index
    int       train_gid;     // global identifier
    char      name[32];

    bool      master_message;       // position update available for master
    int       master_courier;       // courier to the master for pos. update
    int       acceleration_courier; // acceleration delay courier
    int       reverse_courier;      // reverse step delay courier
    int       checkpoint_courier;   // courier used to wake at checkpoints

    ratio     feedback_ratio;
    int       feedback_threshold;
    int       stopping_distance_offset;
    int       reverse_time_fudge_factor;
    int       acceleration_time_fudge_factor;

    train_dim measurements;
    cubic     dmap; // map decceleration time to stopping distance
    cubic     amap; // map acceleration time to a starting distance

    int       vmap[TRAIN_SPEEDS][TRACK_TYPE_COUNT];
    line      stop_dist_map;
    cubic     start_dist_map;

    // these will usually be based on actual track feedback, unless we have to
    // go a while without track feedback, in which case this will be estimates

#define last_checkpoint      ctxt->last_checkpoint_event
#define last_sensor          ctxt->last_sensor_event
#define last_acceleration    ctxt->last_acceleration_event
#define last_accel           last_acceleration
#define current_checkpoint   ctxt->current_checkpoint_event
#define current_sensor       ctxt->current_sensor_event
#define current_acceleration ctxt->current_acceleration_event
#define current_accel        current_acceleration
#define truth                ctxt->truth_event

    train_state last_acceleration_event;
    train_state current_acceleration_event;
    train_state last_sensor_event;
    train_state current_sensor_event;
    train_state last_checkpoint_event;
    train_state current_checkpoint_event;
    train_state truth_event;

    // TODO: maybe these should be encoded into train_state?
    //       or maybe not, they are h4x
    int         reversing;         // reverse step counter
    int         reverse_speed;     // speed to pick up when finished reversing

    const track_node* const track;
} blaster;

#endif
