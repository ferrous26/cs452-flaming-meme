
#ifndef __TRAIN_STATION_H__
#define __TRAIN_STATION_H__

#include <tasks/mission_control.h>

#define TRAIN_STATION_NAME (char*)"TRAINS"

#define TRAIN_COUNT         7
#define TRAIN_REVERSE_DELAY 40

typedef enum {
    TRAIN_CHANGE_SPEED,
    TRAIN_REVERSE_DIRECTION,
    TRAIN_TOGGLE_LIGHT,
    TRAIN_HORN_SOUND,
    TRAIN_STOP,
    TRAIN_WHERE_ARE_YOU,

    TRAIN_HIT_SENSOR,
    TRAIN_EXPECTED_SENSOR,

    TRAIN_REQUEST_COUNT
} train_req_type;

typedef union {
    sensor_name sensor;
    int         int_value;
} thing;

typedef struct {
    train_req_type type;
    thing          one;
    thing          two;
} train_req;

void __attribute__ ((noreturn)) train_driver(void);

#endif
