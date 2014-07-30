
#ifndef __MISSION_CONTROL_H__
#define __MISSION_CONTROL_H__

#include <normalize.h>
#include <track_node.h>
#include <tasks/train_control.h>

#define MISSION_CONTROL_NAME "BORIS"

void mission_control(void) __attribute__((noreturn));
int reset_train_state(void);

int load_track(const int track_value);
int update_turnout(const int num, const int state);

int get_sensor_from(int from, int* const res_dist, int* const res_name);
int get_position_from(const track_location from,
                      track_location* to,
                      const int travel_dist);


int disable_sensor_node(const int sensor_num);
int disable_sensor_name(const int bank, const int num);

int revive_sensor_node(const int sensor_num);
int revive_sensor_name(const int bank, const int num);

int query_turnout_state(const int query_sensor_state);
#endif
