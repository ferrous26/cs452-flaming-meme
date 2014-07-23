#ifndef __TRAIN_MASTER_H__
#define __TRAIN_MASTER_H__

#include <std.h>

typedef enum {
    MASTER_GOTO_LOCATION,     // new destination from control
    MASTER_STOP_AT_SENSOR,
    MASTER_WHERE_ARE_YOU,
    MASTER_PATH_DATA,         // track route from path worker
    MASTER_BLASTER_LOCATION,  // current location from blaster

    MASTER_FLIP_TURNOUT,
    MASTER_STOP_TRAIN

} master_req_type;

typedef struct {
    master_req_type type;
    int arg1;
    int arg2;
    int arg3;
    int arg4;
    int arg5;
    int arg6;
    int arg7;
} master_req;

void __attribute__ ((noreturn)) train_master(void);

#endif
