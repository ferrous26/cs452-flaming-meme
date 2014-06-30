#include <velocity.h>
#include <syscall.h>
#include <train.h>
#include <debug.h>
#include <tasks/mission_control.h>

#define TRAIN_SPEEDS 14

static int speeds[NUM_TRAINS][TRAIN_SPEEDS];

void velocity_init() {
    memset(speeds, 0, sizeof(speeds));

    speeds[6][0]  = 7427;
    speeds[6][1]  = 21372;
    speeds[6][2]  = 61325;
    speeds[6][3]  = 104827;
    speeds[6][4]  = 140683;
    speeds[6][5]  = 180537;
    speeds[6][6]  = 211307;
    speeds[6][7]  = 253157;
    speeds[6][8]  = 312243;
    speeds[6][9]  = 382987;
    speeds[6][10] = 477939;
    speeds[6][11] = 570300;
    speeds[6][12] = 610249;
    speeds[6][13] = 640000;
}

int velocity_for_speed(const int train_offset, const int speed) {
    if (speed == 0) return 0;

    assert(train_offset >= 0 && train_offset < NUM_TRAINS,
           "Invalid train %d", train_offset);

    assert(speed > 0 && speed <= TRAIN_SPEEDS,
           "Invalid speed %d", speed);

    return speeds[train_offset][speed - 1];
}
