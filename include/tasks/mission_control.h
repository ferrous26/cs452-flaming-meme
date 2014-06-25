
#ifndef __MISSION_CONTROL_H__
#define __MISSION_CONTROL_H__

#define MISSION_CONTROL_NAME "BORIS"
#define SENSOR_POLL_NAME     "SR_POLL"

void mission_control(void) __attribute__((noreturn));

int reset_train_state(void);
int toggle_train_light(int train);
int toggle_train_horn(int train);
int update_turnout(int num, int state);
int update_train_speed(int train, int speed);

int delay_all_sensor(void);
int delay_sensor(int sensor_bank, int sensor_num);

int load_track(int track_value);

#endif
