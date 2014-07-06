
#ifndef __PATH_H__
#define __PATH_H__

typedef enum {
    PATH_SENSOR,
    PATH_TURNOUT,
    PATH_REVERSE,
} path_type;

typedef struct {
    int num;
    int dir;
} path_turn;

typedef union {
    path_turn turnout;
    int       int_value;
} path_data;

typedef struct {
    path_type type;
    int       dist;
    path_data data;
} path_node;

#endif

