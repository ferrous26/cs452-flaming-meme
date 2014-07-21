
#ifndef __TRAIN_BLASTER_H__
#define __TRAIN_BLASTER_H__

void __attribute__ ((noreturn)) train_blaster(void);

typedef enum {
    BLASTER_CHANGE_SPEED,

    BLASTER_REVERSE,  // step 1
    BLASTER_REVERSE2, // step 2 (used by delay courier)
    BLASTER_REVERSE3, // step 3 (used by delay courier)
    BLASTER_REVERSE4, // step 4 (used by delay courier)

    MASTER_BLASTER_WHERE_ARE_YOU, // special version for master

    // TODO: some of these need to move to train_master
    BLASTER_DUMP_VELOCITY_TABLE,
    BLASTER_UPDATE_FEEDBACK_THRESHOLD,
    BLASTER_UPDATE_FEEDBACK_ALPHA,
    BLASTER_UPDATE_STOP_OFFSET,
    BLASTER_UPDATE_CLEARANCE_OFFSET,
    BLASTER_UPDATE_FUDGE_FACTOR,

    // TODO: move these to train_master (need to share some physics)
    BLASTER_SHORT_MOVE,
    BLASTER_FINISH_SHORT_MOVE,

    // these are the event types we care about for position simulation
    BLASTER_ACCELERATION_COMPLETE,
    BLASTER_NEXT_NODE_ESTIMATE,
    BLASTER_SENSOR_FEEDBACK,
    BLASTER_UNEXPECTED_SENSOR_FEEDBACK,

    BLASTER_SENSOR_TIMEOUT, // lol wut?

    BLASTER_REQ_TYPE_COUNT
} blaster_req_type;

typedef struct {
    blaster_req_type type;
    int  arg1;
    int  arg2;
    int  arg3;
} blaster_req;

#endif
