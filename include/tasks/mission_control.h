
#ifndef __MISSION_CONTROL_H__
#define __MISSION_CONTROL_H__

#include <normalize.h>
#include <track_node.h>

#define MISSION_CONTROL_NAME "BORIS"
#define SENSOR_POLL_NAME     "SR_POLL"

void mission_control(void) __attribute__((noreturn));
int reset_train_state(void);

int load_track(const int track_value);
int update_turnout(const int num, const int state);

int delay_all_sensor(void);
int delay_sensor(const int sensor_bank, const int sensor_num);
int get_sensor_from(int from, int* const res_dist, int* const res_name);

#endif
