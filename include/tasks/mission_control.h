
#ifndef __MISSION_CONTROL_H__
#define __MISSION_CONTROL_H__

#include <normalize.h>
#include <track_node.h>

#define MISSION_CONTROL_NAME "BORIS"

void mission_control(void) __attribute__((noreturn));
int reset_train_state(void);

int load_track(const int track_value);
int update_turnout(const int num, const int state);
int get_sensor_from(int from, int* const res_dist, int* const res_name);

#endif
