#ifndef __TRAIN_CONTROL_H__
#define __TRAIN_CONTROL_H__

#include <std.h>
#include <normalize.h>

#define TRAIN_CONTROL_NAME (char*)"LENIN"

void __attribute__ ((noreturn)) train_control(void);

typedef enum {
    TWEAK_FEEDBACK_THRESHOLD,
    TWEAK_FEEDBACK_RATIO,
    TWEAK_STOP_OFFSET,
    TWEAK_TURNOUT_CLEARANCE,
    TWEAK_REVERSE_STOP_TIME_PADDING,
    TWEAK_ACCELERATION_TIME_FUDGE,
    TWEAK_COUNT
} train_tweakable;

// Blaster API
typedef enum {
    BLASTER_CONTROL_REQUEST_COMMAND,
    MASTER_CONTROL_REQUEST_COMMAND,

    CONTROL_SET_SPEED,
    CONTROL_REVERSE,
    CONTROL_WHERE_ARE_YOU,
    CONTROL_STOP_AT_SENSOR,
    CONTROL_BLOCK_CALLER,
    CONTROL_GOTO_LOCATION,
    CONTROL_SHORT_MOVE,
    CONTROL_CHASE,

    CONTROL_DUMP_VELOCITY_TABLE,
    CONTROL_UPDATE_TWEAK,

    CONTROL_TOGGLE_HORN
} control_req_type;

typedef struct block_until_sensor {
    int tid;
    int sensor;
} sensor_block;

typedef struct {
    control_req_type type;
    int  arg1;
    int  arg2;
    int  arg3;
} control_req;

typedef struct {
    int sensor;
    int offset;
} track_location;

// Human API
int train_set_speed(const int train, const int speed);
int train_reverse(const int train); // this is effectively a macro command

track_location train_where_are_you(const int train);
int train_stop_at_sensor(const int train, const int bank, const int num);
int train_block_until(const int train, const int bank, const int num);

int train_goto_location(const int train,
                        const int bank,
                        const int num,
                        const int off);

int train_short_move(const int train, const int off);

int train_dump_velocity_table(const int train);
int train_update_tweak(const int train,
                       const train_tweakable tweak,
                       const int value);
int train_toggle_horn(const int train);
int train_chase(const int chaser, const int chasee);

#endif
