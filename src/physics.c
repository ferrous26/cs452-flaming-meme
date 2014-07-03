#include <physics.h>
#include <syscall.h>
#include <train.h>
#include <debug.h>
#include <tasks/mission_control.h>
#include <vt100.h>

#define TRAIN_SPEEDS 14

static int speeds[NUM_TRAINS][TRAIN_SPEEDS];

void physics_init() {

    memset(speeds, 0, sizeof(int) * TRAIN_SPEEDS);

    speeds[1][0]  = 209;
    speeds[1][1]  = 757;
    speeds[1][2]  = 1249;
    speeds[1][3]  = 1726;
    speeds[1][4]  = 2283;
    speeds[1][5]  = 2826;
    speeds[1][6]  = 3441;
    speeds[1][7]  = 3790;
    speeds[1][8]  = 4212;
    speeds[1][9]  = 4570;
    speeds[1][10] = 4994;
    speeds[1][11] = 4869; // hmmm
    speeds[1][12] = 4747;
    speeds[1][13] = 4440;

    speeds[2][0]  = 253;
    speeds[2][1]  = 755;
    speeds[2][2]  = 1284;
    speeds[2][3]  = 1767;
    speeds[2][4]  = 2373;
    speeds[2][5]  = 2921;
    speeds[2][6]  = 3488;
    speeds[2][7]  = 3813;
    speeds[2][8]  = 4341;
    speeds[2][9]  = 4673;
    speeds[2][10] = 4931;
    speeds[2][11] = 5129;
    speeds[2][12] = 4931; // hmmm
    speeds[2][13] = 4808;

    speeds[3][0]  = 244;
    speeds[3][1]  = 750;
    speeds[3][2]  = 1250;
    speeds[3][3]  = 1749;
    speeds[3][4]  = 2319;
    speeds[3][5]  = 2846;
    speeds[3][6]  = 3347;
    speeds[3][7]  = 3748;
    speeds[3][8]  = 4116;
    speeds[3][9]  = 4597;
    speeds[3][10] = 5060;
    speeds[3][11] = 5161;
    speeds[3][12] = 5044; // hmmm
    speeds[3][13] = 4793;

    speeds[4][0]  = 250;
    speeds[4][1]  = 796;
    speeds[4][2]  = 1316;
    speeds[4][3]  = 1820;
    speeds[4][4]  = 2330;
    speeds[4][5]  = 2861;
    speeds[4][6]  = 3438;
    speeds[4][7]  = 3967;
    speeds[4][8]  = 4482;
    speeds[4][9]  = 5130;
    speeds[4][10] = 5557;
    speeds[4][11] = 5598;
    speeds[4][12] = 5324; // hmmm
    speeds[4][13] = 5093;

    speeds[5][0]  = 183;
    speeds[5][1]  = 731;
    speeds[5][2]  = 1272;
    speeds[5][3]  = 1720;
    speeds[5][4]  = 2193;
    speeds[5][5]  = 2701;
    speeds[5][6]  = 3262;
    speeds[5][7]  = 3758;
    speeds[5][8]  = 4265;
    speeds[5][9]  = 4762;
    speeds[5][10] = 5179;
    speeds[5][11] = 5608;
    speeds[5][12] = 5231; // hmmm
    speeds[5][13] = 4828;

    speeds[6][0]  = 206;
    speeds[6][1]  = 142;
    speeds[6][2]  = 205;
    speeds[6][3]  = 588;
    speeds[6][4]  = 1006;
    speeds[6][5]  = 1351;
    speeds[6][6]  = 1732;
    speeds[6][7]  = 2028;
    speeds[6][8]  = 2429;
    speeds[6][9]  = 2995;
    speeds[6][10] = 3676;
    speeds[6][11] = 4584;
    speeds[6][12] = 5864;
    speeds[6][13] = 6137;
}



void physics_dump() {
    char buffer[1024];
    char* ptr = log_start(buffer);

    ptr = sprintf_char(ptr, '\n');

    for (int i = 0; i < NUM_TRAINS; i++)
        ptr = sprintf(ptr, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                      i,
                      speeds[i][0],
                      speeds[i][1],
                      speeds[i][2],
                      speeds[i][3],
                      speeds[i][4],
                      speeds[i][5],
                      speeds[i][6],
                      speeds[i][7],
                      speeds[i][8],
                      speeds[i][9],
                      speeds[i][10],
                      speeds[i][11],
                      speeds[i][12],
                      speeds[i][13]);

    ptr = log_end(ptr);
    Puts(buffer, ptr - buffer);
}

int velocity_for_speed(const int train_offset, const int speed) {
    if (speed == 0) return 0;

    assert(train_offset >= 0 && train_offset < NUM_TRAINS,
           "Invalid train %d", train_offset);

    assert(speed > 0 && speed <= TRAIN_SPEEDS,
           "Invalid speed %d", speed);

    return speeds[train_offset][speed - 1];
}

void update_velocity_for_speed(const int train_offset,
                               const int speed,
                               const int distance,
                               const int time) {
    //log("%d %d", distance, time);
    //log("%d %d %d", speeds[train_offset][speed - 1], (distance / time),
    //(speeds[train_offset][speed - 1] + (distance / time)) >> 2);

    speeds[train_offset][speed - 1] =
        (speeds[train_offset][speed - 1] + (distance / time)) >> 1;

}
