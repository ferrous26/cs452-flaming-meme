#ifndef __TRAIN_BLASTER_H__
#define __TRAIN_BLASTER_H__

#include <std.h>
#include <normalize.h>

#define TRAIN_BLASTER_NAME (char*)"BLASTER"

void __attribute__ ((noreturn)) train_blaster(void);

// Master API
typedef enum {
    MASTER_BLASTER_REQUEST_COMMAND,

    BLASTER_SET_SPEED,
    BLASTER_REVERSE,
    BLASTER_WHERE_ARE_YOU,
    BLASTER_STOP_AT_SENSOR,
    BLASTER_GOTO_LOCATION,
    BLASTER_SHORT_MOVE,

    BLASTER_DUMP_VELOCITY_TABLE,
    BLASTER_UPDATE_FEEDBACK_THRESHOLD,
    BLASTER_UPDATE_FEEDBACK_ALPHA,
    BLASTER_UPDATE_STOP_OFFSET,
    BLASTER_UPDATE_CLEARANCE_OFFSET,
    BLASTER_UPDATE_FUDGE_FACTOR,

    BLASTER_TOGGLE_HORN
} blaster_req_type;

typedef struct {
    blaster_req_type type;
    int  arg1;
    int  arg2;
    int  arg3;
} blaster_req;


// Human API
int train_set_speed(const int train, const int speed);
int train_reverse(const int train); // this is effectively a macro command

int train_where_are_you(const int train);
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
