
#ifndef __TRAIN_CONTROL_H__
#define __TRAIN_CONTROL_H__

#define TRAIN_CONTROL_NAME "LENIN"

typedef struct {
    short num;
    short speed;
} train_speed;

typedef struct {
    short train;
    short sensor;
} train_stop;

typedef enum {
    TC_U_TRAIN_SPEED,
    TC_U_TRAIN_REVERSE,
    
    TC_T_TRAIN_LIGHT,
    TC_T_TRAIN_HORN,
    TC_T_TRAIN_STOP,
    
    TC_REQ_WORK,
    
    TC_TYPE_COUNT
} tc_type;

typedef union {
    train_speed train_speed;
    train_stop  train_stop;
    int         int_value;
} tc_payload;

typedef struct {
    tc_type    type;
    tc_payload payload;
} tc_req;

void __attribute__((noreturn)) train_control(void);

int train_reverse(int train);
int train_set_speed(int train, int speed);
int train_toggle_light(int train);
int train_toggle_horn(int train);
int train_stop_at(const int train, const int bank, const int num);

#endif
