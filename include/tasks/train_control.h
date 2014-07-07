
#ifndef __TRAIN_CONTROL_H__
#define __TRAIN_CONTROL_H__

#define TRAIN_CONTROL_NAME "LENIN"

typedef struct {
    int8  train;
    int8  offset;
    int8  bank;
    int8  num;
} train_go;

typedef struct {
    short num;
    short speed;
} train_speed;

typedef struct {
    short train;
    short sensor;
} train_stop;

typedef struct {
    short train;
    short tweak;
} train_tweakable;

typedef enum {
    TC_U_TRAIN_SPEED,
    TC_U_TRAIN_REVERSE,
    TC_U_GOTO,
    TC_U_THRESHOLD,
    TC_U_ALPHA,
    TC_U_STOP_OFFSET,

    TC_T_TRAIN_LIGHT,
    TC_T_TRAIN_HORN,
    TC_T_TRAIN_STOP,

    TC_Q_TRAIN_WHEREIS,
    TC_Q_DUMP,

    TC_REQ_WORK,

    TC_TYPE_COUNT
} tc_type;

typedef union {
    train_go        train_go;
    train_tweakable train_tweak;
    train_speed train_speed;
    train_stop  train_stop;
    int         int_value;
} tc_payload;

typedef struct {
    tc_type    type;
    tc_payload payload;
} tc_req;

void __attribute__((noreturn)) train_control(void);

int train_reverse(const int train);
int train_set_speed(const int train, int speed);
int train_toggle_light(const int train);
int train_toggle_horn(const int train);
int train_stop_at(const int train, const int bank, const int num);
int train_where_are_you(const int train);
int train_goto(const int train, const int bank, const int num, const int off);
int train_dump(const int train);
int train_update_threshold(const int train, const int threshold);
int train_update_alpha(const int train, const int alpha);
int train_set_stop_offset(const int train, const int offset);

#endif
