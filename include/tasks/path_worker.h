
#ifndef __PATH_WORKER_H__
#define __PATH_WORKER_H__

#include <path.h>

#define OPTS_TRAIN_NUM_MASK     0xFF
#define PATH_SHORT_MOVE_OFF_MASK     0x100
#define PATH_START_REVERSE_OFF_MASK  0x200
#define PATH_BACK_APPROACH_OFF_MASK  0x400
#define PATH_NO_REVERSE_MASK        (PATH_SHORT_MOVE_OFF_MASK       \
                                    |PATH_START_REVERSE_OFF_MASK    \
                                    |PATH_BACK_APPROACH_OFF_MASK)

typedef struct {
    int   requestor;
    int   header;
    short sensor_to;
    short sensor_from;
    short opts;
    short reserve;
} path_request;

typedef struct {
    int        response;
    int        size;
    int        reserved;
    path_node* path;
} path_response;

void __attribute__((noreturn)) path_worker(void);

#endif

