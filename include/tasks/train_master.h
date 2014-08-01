#ifndef __TRAIN_MASTER_H__
#define __TRAIN_MASTER_H__

#include <std.h>

typedef enum {
    MASTER_GOTO_LOCATION,     // new destination from control
    MASTER_STOP_AT_SENSOR,
    MASTER_BLOCK_UNTIL_SENSOR,
    MASTER_WHERE_ARE_YOU,
    MASTER_CHASE,

    // master to master communication
    MASTER_MASTER_WHERE_ARE_YOU,
    MASTER_MASTER_HERE_I_AM,

    MASTER_SHORT_MOVE,

    MASTER_PATH_DATA,         // track route from path worker
    MASTER_BLASTER_LOCATION,  // current location from blaster

    MASTER_UPDATE_TWEAK

} master_req_type;

typedef struct {
    master_req_type type;
    int arg1;
    int arg2;
    int arg3;
} master_req;

void __attribute__ ((noreturn)) train_master(void);

#endif
