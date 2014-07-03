
#ifndef __MISSION_CONTROL_H__
#define __MISSION_CONTROL_H__

#define MISSION_CONTROL_NAME "BORIS"
#define SENSOR_POLL_NAME     "SR_POLL"
#define NUM_TRAINS           7

typedef struct {
    short bank;
    short num;
} sensor_name;

void mission_control(void) __attribute__((noreturn));

int reset_train_state(void);
int update_turnout(int num, int state);

int delay_all_sensor(sensor_name* const sensor);
int delay_sensor(int sensor_bank, int sensor_num);

int load_track(int track_value);

int train_reverse(int train);
int train_set_speed(int train, int speed);
int train_toggle_light(int train);
int train_toggle_horn(int train);
int train_stop_at(const int train, const int bank, const int num);

int get_sensor_from(sensor_name* from, int* res_dist, sensor_name* res_name);

#endif
