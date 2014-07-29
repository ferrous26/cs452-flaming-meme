
#ifndef __MISSION_CONTROL_TYPES_H__
#define __MISSION_CONTROL_TYPES_H__

#include <normalize.h>
#include <tasks/train_control.h>

typedef enum {
    // load
    MC_L_TRACK,
    // update
    MC_U_TURNOUT,
    MC_Q_TURNOUT,

    MC_KILL_SENSOR,
    MC_REVIVE_SENSOR,

    // reset
    MC_R_TRACK,
    
    MC_TD_GET_NEXT_SENSOR,
    MC_TD_GET_NEXT_POSITION,
    
    // misc
    MC_TYPE_COUNT
} mc_type;

typedef struct {
    track_location  from;
    int             travel_dist;
    track_location* to; // this is an array
} position_req;

typedef union {
    position_req position;
    turnout      turn;
    int          int_value;
} mc_payload;

typedef struct {
    mc_type    type;
    mc_payload payload;
} mc_req;

#endif
