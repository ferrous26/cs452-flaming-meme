
#ifndef __MISSION_CONTROL_TYPES_H__
#define __MISSION_CONTROL_TYPES_H__

typedef struct {
    short num;
    short state;
} turnout_state;

typedef struct {
    short num;
    short speed;
} train_speed;

typedef enum {
    // load
    MC_L_TRACK,
    // update
    MC_U_SENSOR,
    MC_U_TURNOUT,
    MC_U_TRAIN_SPEED,
    MC_U_TRAIN_REVERSE,
    // toggle
    MC_T_TRAIN_LIGHT,
    MC_T_TRAIN_HORN,
    // reset
    MC_R_TRACK,
    // delay
    MC_D_SENSOR,
    MC_D_SENSOR_ANY,
    // workers
    MC_TD_CALL,
    MC_TD_GET_NEXT_SENSOR,
    // misc
    MC_TYPE_COUNT
} mc_type;

typedef union {
    sensor_name   sensor;
    turnout_state turnout;
    train_speed   train_speed;
    int           int_value;
} mc_payload;

typedef struct {
    mc_type    type;
    mc_payload payload;
} mc_req;

#endif
