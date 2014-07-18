#ifndef __TRAIN_CONTROL_H__
#define __TRAIN_CONTROL_H__

#include <std.h>
#include <normalize.h>

#define TRAIN_CONTROL_NAME (char*)"LENIN"

void __attribute__ ((noreturn)) train_control(void);

// Blaster API
typedef enum {
    BLASTER_CONTROL_REQUEST_COMMAND,
    MASTER_CONTROL_REQUEST_COMMAND,

    CONTROL_SET_SPEED,
    CONTROL_REVERSE,
    CONTROL_WHERE_ARE_YOU,
    CONTROL_STOP_AT_SENSOR,
    CONTROL_GOTO_LOCATION,
    CONTROL_SHORT_MOVE,

    CONTROL_DUMP_VELOCITY_TABLE,
    CONTROL_UPDATE_FEEDBACK_THRESHOLD,
    CONTROL_UPDATE_FEEDBACK_ALPHA,
    CONTROL_UPDATE_STOP_OFFSET,
    CONTROL_UPDATE_CLEARANCE_OFFSET,
    CONTROL_UPDATE_FUDGE_FACTOR,

    CONTROL_TOGGLE_HORN
} control_req_type;

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

int train_goto_location(const int train,
                        const int bank,
                        const int num,
                        const int off);

int train_short_move(const int train, const int off);

int train_dump_velocity_table(const int train);
int train_update_feedback_threshold(const int train, const int threshold);
int train_update_feedback_alpha(const int train, const int alpha);
int train_update_stop_offset(const int train, const int offset);
int train_update_clearance_offset(const int train, const int offset);
int train_update_reverse_time_fudge(const int train, const int offset);
int train_toggle_horn(const int train);

#endif
