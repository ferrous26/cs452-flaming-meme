#include <physics.h>
#include <syscall.h>
#include <train.h>
#include <debug.h>
#include <tasks/mission_control.h>
#include <tasks/term_server.h>
#include <vt100.h>

#define TRAIN_SPEEDS 14

typedef struct {
    int velocity[TRAIN_SPEEDS];
    int stopping_slope;
    int stopping_offset;
} speed_data;

static speed_data train_data[NUM_TRAINS];
static int feedback_threshold;

#define INITIAL_FEEDBACK_THRESHOLD 50

void physics_init() {

    feedback_threshold = INITIAL_FEEDBACK_THRESHOLD;

    /* Velocities for train offset 0 */
    train_data[0].velocity[0] = 0;
    train_data[0].velocity[1] = 0;
    train_data[0].velocity[2] = 0;
    train_data[0].velocity[3] = 0;
    train_data[0].velocity[4] = 0;
    train_data[0].velocity[5] = 0;
    train_data[0].velocity[6] = 0;
    train_data[0].velocity[7] = 0;
    train_data[0].velocity[8] = 0;
    train_data[0].velocity[9] = 0;
    train_data[0].velocity[10] = 0;
    train_data[0].velocity[11] = 0;
    train_data[0].velocity[12] = 0;
    train_data[0].velocity[13] = 0;

    /* Velocities for train offset 1 */
    train_data[1].velocity[0] = 0;
    train_data[1].velocity[1] = 209;
    train_data[1].velocity[2] = 757;
    train_data[1].velocity[3] = 1249;
    train_data[1].velocity[4] = 1726;
    train_data[1].velocity[5] = 2283;
    train_data[1].velocity[6] = 2826;
    train_data[1].velocity[7] = 3441;
    train_data[1].velocity[8] = 3790;
    train_data[1].velocity[9] = 4212;
    train_data[1].velocity[10] = 4570;
    train_data[1].velocity[11] = 4994;
    train_data[1].velocity[12] = 4869;
    train_data[1].velocity[13] = 4747;

    /* Velocities for train offset 2 */
    train_data[2].velocity[0] = 0;
    train_data[2].velocity[1] = 253;
    train_data[2].velocity[2] = 755;
    train_data[2].velocity[3] = 1284;
    train_data[2].velocity[4] = 1767;
    train_data[2].velocity[5] = 2373;
    train_data[2].velocity[6] = 2921;
    train_data[2].velocity[7] = 3488;
    train_data[2].velocity[8] = 3621;
    train_data[2].velocity[9] = 4250;
    train_data[2].velocity[10] = 4661;
    train_data[2].velocity[11] = 5025;
    train_data[2].velocity[12] = 5249;
    train_data[2].velocity[13] = 5276;

    /* Velocities for train offset 3 */
    train_data[3].velocity[0] = 0;
    train_data[3].velocity[1] = 244;
    train_data[3].velocity[2] = 750;
    train_data[3].velocity[3] = 1250;
    train_data[3].velocity[4] = 1749;
    train_data[3].velocity[5] = 2319;
    train_data[3].velocity[6] = 2846;
    train_data[3].velocity[7] = 3347;
    train_data[3].velocity[8] = 3748;
    train_data[3].velocity[9] = 4116;
    train_data[3].velocity[10] = 4597;
    train_data[3].velocity[11] = 5060;
    train_data[3].velocity[12] = 5161;
    train_data[3].velocity[13] = 5044;

    /* Velocities for train offset 4 */
    train_data[4].velocity[0] = 0;
    train_data[4].velocity[1] = 250;
    train_data[4].velocity[2] = 1298;
    train_data[4].velocity[3] = 1810;
    train_data[4].velocity[4] = 2272;
    train_data[4].velocity[5] = 2824;
    train_data[4].velocity[6] = 3378;
    train_data[4].velocity[7] = 3966;
    train_data[4].velocity[8] = 4512;
    train_data[4].velocity[9] = 4913;
    train_data[4].velocity[10] = 5535;
    train_data[4].velocity[11] = 5718;
    train_data[4].velocity[12] = 5472;
    train_data[4].velocity[13] = 5733;

    /* Velocities for train offset 5 */
    train_data[5].velocity[0] = 0;
    train_data[5].velocity[1] = 183;
    train_data[5].velocity[2] = 731;
    train_data[5].velocity[3] = 1272;
    train_data[5].velocity[4] = 1720;
    train_data[5].velocity[5] = 2193;
    train_data[5].velocity[6] = 2701;
    train_data[5].velocity[7] = 3262;
    train_data[5].velocity[8] = 3758;
    train_data[5].velocity[9] = 4265;
    train_data[5].velocity[10] = 4762;
    train_data[5].velocity[11] = 5179;
    train_data[5].velocity[12] = 5608;
    train_data[5].velocity[13] = 5231;

    /* Velocities for train offset 6 */
    train_data[6].velocity[0] = 0;
    train_data[6].velocity[1] = 142;
    train_data[6].velocity[2] = 205;
    train_data[6].velocity[3] = 588;
    train_data[6].velocity[4] = 1006;
    train_data[6].velocity[5] = 1006;
    train_data[6].velocity[6] = 1351;
    train_data[6].velocity[7] = 1732;
    train_data[6].velocity[8] = 2028;
    train_data[6].velocity[9] = 2429;
    train_data[6].velocity[10] = 2995;
    train_data[6].velocity[11] = 3676;
    train_data[6].velocity[12] = 4584;
    train_data[6].velocity[13] = 5864;


    train_data[0].stopping_slope  = 0;
    train_data[0].stopping_offset = 0;
    train_data[1].stopping_slope  =  63827;
    train_data[1].stopping_offset = -42353;
    train_data[2].stopping_slope  =  67029;
    train_data[2].stopping_offset = -38400;
    train_data[3].stopping_slope  =  73574;
    train_data[3].stopping_offset = -59043;
    train_data[4].stopping_slope  =  63905;
    train_data[4].stopping_offset = -41611;
    train_data[5].stopping_slope  = 0;
    train_data[5].stopping_offset = 0;
    train_data[6].stopping_slope  = 0;
    train_data[6].stopping_offset = 0;
}



void physics_dump() {
    char buffer[1024];
    char* ptr = log_start(buffer);

    ptr = sprintf_char(ptr, '\n');

    for (int i = 0; i < NUM_TRAINS; i++)
        ptr = sprintf(ptr, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                      i,
                      train_data[i].velocity[0],
                      train_data[i].velocity[1],
                      train_data[i].velocity[2],
                      train_data[i].velocity[3],
                      train_data[i].velocity[4],
                      train_data[i].velocity[5],
                      train_data[i].velocity[6],
                      train_data[i].velocity[7],
                      train_data[i].velocity[8],
                      train_data[i].velocity[9],
                      train_data[i].velocity[10],
                      train_data[i].velocity[11],
                      train_data[i].velocity[12],
                      train_data[i].velocity[13]);

    ptr = log_end(ptr);
    Puts(buffer, ptr - buffer);
}

int velocity_for_speed(const int train_offset, const int speed) {
    if (speed == 0) return 0;

    assert(train_offset >= 0 && train_offset < NUM_TRAINS,
           "Invalid train %d", train_offset);

    assert(speed > 0 && speed <= TRAIN_SPEEDS,
           "Invalid speed %d", speed);

    return train_data[train_offset].velocity[speed - 1];
}


void update_velocity_for_speed(const int train_offset,
                               const int speed,
                               const int distance,
                               const int time,
                               const int delta) {

    //log("%d %d", distance, time);
    //log("%d %d %d", train_data[train_offset][speed - 1], (distance / time),
    //(train_data[train_offset][speed - 1] + (distance / time)) >> 2);

    if (delta > feedback_threshold) {
        log("Feedback is off by too much (%d). I suspect foul play!", delta);
        return;
    }

    const int old_speed = train_data[train_offset].velocity[speed - 1];
    const int new_speed = distance / time;

    // the ratio is 9:1 right now
    train_data[train_offset].velocity[speed - 1] =
        ((old_speed << 3) + (new_speed << 1)) / 10;
}

int stopping_distance_for_speed(const int train_offset, const int speed) {
    return (train_data[train_offset].stopping_slope * speed) +
        train_data[train_offset].stopping_offset;
}

void physics_change_feedback_threshold(const int threshold) {
    feedback_threshold = threshold;
}
