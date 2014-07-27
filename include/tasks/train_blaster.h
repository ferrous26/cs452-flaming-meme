
#ifndef __TRAIN_BLASTER_H__
#define __TRAIN_BLASTER_H__

void __attribute__ ((noreturn)) train_blaster(void);

typedef enum {
    EVENT_ACCELERATION,
    EVENT_VIRTUAL,
    EVENT_SENSOR,
    EVENT_UNEXPECTED_SENSOR
} train_event;

typedef enum {
    BLASTER_CHANGE_SPEED,
    BLASTER_MASTER_CHANGE_SPEED,

    BLASTER_MASTER_REVERSE,
    BLASTER_REVERSE,  // step 1
    BLASTER_REVERSE2, // step 2 (used by delay courier)
    BLASTER_REVERSE3, // step 3 (used by delay courier)
    BLASTER_REVERSE4, // step 4 (used by delay courier)

    BLASTER_MASTER_WHERE_ARE_YOU,
    BLASTER_MASTER_CONTEXT,

    BLASTER_DUMP_VELOCITY_TABLE,
    BLASTER_UPDATE_TWEAK,

    // these are the event types we care about for position simulation
    BLASTER_ACCELERATION_COMPLETE,
    BLASTER_NEXT_NODE_ESTIMATE,
    BLASTER_SENSOR_FEEDBACK,
    BLASTER_UNEXPECTED_SENSOR_FEEDBACK,

    BLASTER_SENSOR_TIMEOUT, // lol wut?
    BLASTER_CONSOLE_LOST,

    BLASTER_REQ_TYPE_COUNT
} blaster_req_type;

typedef struct {
    blaster_req_type type;
    int  arg1;
    int  arg2;
    int  arg3;
} blaster_req;

#endif
