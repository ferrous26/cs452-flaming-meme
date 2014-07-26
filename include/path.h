
#ifndef __PATH_H__
#define __PATH_H__

#include <normalize.h>

typedef enum {
    PATH_SENSOR,
    PATH_TURNOUT,
    PATH_REVERSE,
} path_type;

typedef union {
    turnout turnout;
    int     sensor;
    int     int_value;
} path_data;

typedef struct {
    path_type type;
    path_data data;
    int       dist;
    int       pre_dist;
} path_node;

#endif
