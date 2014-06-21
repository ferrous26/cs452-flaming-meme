
#ifndef __MISSION_CONTROL_H__
#define __MISSION_CONTROL_H__

#define MISSION_CONTROL_NAME "BORIS"
#define SENSOR_POLL_NAME     "SR_POLL"

void mission_control(void) __attribute__((noreturn));

int  update_turnout(short num, short state);
int  update_train_speed(int train, int speed);

#endif

