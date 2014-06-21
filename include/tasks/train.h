
#ifndef __TRAIN_STATION_H__
#define __TRAIN_STATION_H__

#define TRAIN_STATION_NAME (char*)"TRAINS"

#define TRAIN_COUNT 7
#define TRAIN_REVERSE_DELAY_FACTOR 35

typedef enum {
    WHOAMI,
    REQUEST,
    SPEED,
    REVERSE,
    LIGHT,
    HORN
} train_req_type;

typedef struct {
    train_req_type type;
    int train;
    int arg;
} train_req;

void __attribute__ ((noreturn)) train_station(void);

#endif
