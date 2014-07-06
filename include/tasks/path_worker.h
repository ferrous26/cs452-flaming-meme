
#ifndef __PATH_WORKER_H__
#define __PATH_WORKER_H__

#include <path.h>

typedef struct {
    int requestor;
    int header;
    int sensor_to;
    int sensor_from;
} path_request;

typedef struct {
    int        response;
    int        size;
    path_node* path;
} path_response;

void path_worker(void);

#endif

