
#ifndef __MISSION_CONTROL_H__
#define __MISSION_CONTROL_H__

#include <track_node.h>

#define MISSION_CONTROL_NAME "BORIS"
#define SENSOR_POLL_NAME     "SR_POLL"
#define NUM_TRAINS           7

typedef struct {
    short bank;
    short num;
} sensor_name;

extern track_node* train_track;

void mission_control(void) __attribute__((noreturn));

int reset_train_state(void);
int load_track(int track_value);
int update_turnout(int num, int state);

int delay_all_sensor(void);
int delay_sensor(int sensor_bank, int sensor_num);

int get_sensor_from(int from, int* const res_dist, int* const res_name);

#endif
