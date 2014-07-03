#ifndef __VELOCITY_H__
#define __VELOCITY_H__

#include <std.h>

void physics_init(void);

void physics_dump(void);

int  velocity_for_speed(const int train_offset, const int speed);
void update_velocity_for_speed(const int train_offset,
                               const int speed,
                               const int distance,
                               const int time);

#endif
