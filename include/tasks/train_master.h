
#ifndef __TRAIN_MASTER_H__
#define __TRAIN_MASTER_H__

void __attribute__ ((noreturn)) train_master();

typedef enum {
    MASTER_CHANGE_SPEED,
    MASTER_REVERSE,
    MASTER_WHERE_ARE_YOU,
    MASTER_STOP_AT_SENSOR,
    MASTER_GOTO_LOCATION,
    MASTER_DUMP_VELOCITY_TABLE,

    MASTER_UPDATE_FEEDBACK_THRESHOLD,
    MASTER_UPDATE_FEEDBACK_ALPHA,
    MASTER_UPDATE_STOP_OFFSET,
    MASTER_UPDATE_CLEARANCE_OFFSET,

    MASTER_ACCELERATION_COMPLETE,
    MASTER_NEXT_NODE_ESTIMATE,

    MASTER_SENSOR_FEEDBACK,
    MASTER_UNEXPECTED_SENSOR_FEEDBACK
} master_req_type;

typedef struct {
    master_req_type type;
    int  arg1;
    int  arg2;
    int  arg3;
} master_req;

#endif
