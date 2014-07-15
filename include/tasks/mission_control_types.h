
#ifndef __MISSION_CONTROL_TYPES_H__
#define __MISSION_CONTROL_TYPES_H__

#include <normalize.h>

typedef enum {
    // load
    MC_L_TRACK,
    // update
    MC_U_TURNOUT,
    // reset
    MC_R_TRACK,
    // delay
    MC_TD_GET_NEXT_SENSOR,
    // misc
    MC_TYPE_COUNT
} mc_type;

typedef union {
    turnout turn;
    int     int_value;
} mc_payload;

typedef struct {
    mc_type    type;
    mc_payload payload;
} mc_req;

#endif
