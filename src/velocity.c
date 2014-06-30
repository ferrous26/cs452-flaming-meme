#include <velocity.h>
#include <syscall.h>
#include <train.h>
#include <debug.h>
#include <tasks/mission_control.h>

#define TRAIN_SPEEDS 14

static int speeds[NUM_TRAINS][TRAIN_SPEEDS];

void velocity_init() {

    memset(speeds, 0, sizeof(int) * TRAIN_SPEEDS);

    speeds[1][0]  = 10897;
    speeds[1][1]  = 78842;
    speeds[1][2]  = 130162;
    speeds[1][3]  = 179817;
    speeds[1][4]  = 237927;
    speeds[1][5]  = 294432;
    speeds[1][6]  = 358585;
    speeds[1][7]  = 394923;
    speeds[1][8]  = 439065;
    speeds[1][9]  = 476394;
    speeds[1][10] = 520408;
    speeds[1][11] = 507685; // hmmm
    speeds[1][12] = 494057;
    speeds[1][13] = 461907;

    speeds[2][0]  = 13190;
    speeds[2][1]  = 78704;
    speeds[2][2]  = 133797;
    speeds[2][3]  = 184110;
    speeds[2][4]  = 247349;
    speeds[2][5]  = 304444;
    speeds[2][6]  = 363228;
    speeds[2][7]  = 397143;
    speeds[2][8]  = 452802;
    speeds[2][9]  = 486792;
    speeds[2][10] = 513119;
    speeds[2][11] = 533712;
    speeds[2][12] = 513119; // hmmm
    speeds[2][13] = 500259;

    speeds[3][0]  = 12699;
    speeds[3][1]  = 78187;
    speeds[3][2]  = 130274;
    speeds[3][3]  = 182284;
    speeds[3][4]  = 241653;
    speeds[3][5]  = 296455;
    speeds[3][6]  = 348567;
    speeds[3][7]  = 390603;
    speeds[3][8]  = 428900;
    speeds[3][9]  = 478546;
    speeds[3][10] = 526699;
    speeds[3][11] = 537730;
    speeds[3][12] = 524945; // hmmm
    speeds[3][13] = 498687;

    speeds[4][0]  = 13046;
    speeds[4][1]  = 82976;
    speeds[4][2]  = 137165;
    speeds[4][3]  = 189621;
    speeds[4][4]  = 242780;
    speeds[4][5]  = 298227;
    speeds[4][6]  = 358324;
    speeds[4][7]  = 413495;
    speeds[4][8]  = 466854;
    speeds[4][9]  = 534946;
    speeds[4][10] = 579125;
    speeds[4][11] = 583739;
    speeds[4][12] = 554942; // hmmm
    speeds[4][13] = 530278;

    speeds[5][0]  = 9531;
    speeds[5][1]  = 76229;
    speeds[5][2]  = 132496;
    speeds[5][3]  = 179319;
    speeds[5][4]  = 228635;
    speeds[5][5]  = 281419;
    speeds[5][6]  = 339891;
    speeds[5][7]  = 391628;
    speeds[5][8]  = 444525;
    speeds[5][9]  = 496478;
    speeds[5][10] = 539657;
    speeds[5][11] = 584709;
    speeds[5][12] = 544893; // hmmm
    speeds[5][13] = 502863;

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
