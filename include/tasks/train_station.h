
#ifndef __TRAIN_STATION_H__
#define __TRAIN_STATION_H__

#define TRAIN_STATION_NAME (char*)"TRAINS"

#define TRAIN_COUNT         7
#define TRAIN_REVERSE_DELAY 40

typedef enum {
    TRAIN_CHANGE_SPEED,
    TRAIN_REVERSE_DIRECTION,
    TRAIN_TOGGLE_LIGHT,
    TRAIN_HORN_SOUND
} train_req_type;

typedef struct {
    train_req_type type;
    int            arg;
} train_req;

void __attribute__ ((noreturn)) train_driver(void);

#endif
