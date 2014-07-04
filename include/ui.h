
#ifndef __UI_H__
#define __UI_H__

#include <std.h>

#define TRAIN_BANDWIDTH_ROW   1
#define TRAIN_BANDWIDTH_COL   20
#define TRAIN_BANDWIDTH_UP    27
#define TRAIN_BANDWIDTH_WIDTH 3

#define TRAIN_ROW           6
#define TRAIN_COL           1
#define TRAIN_SPEED_COL     (TRAIN_COL + 8)
#define TRAIN_LIGHT_COL     (TRAIN_SPEED_COL + 6)
#define TRAIN_HORN_COL      (TRAIN_LIGHT_COL + 4)

#define TURNOUT_ROW TRAIN_ROW
#define TURNOUT_COL 30

#define SENSOR_ROW (TRAIN_ROW - 1)
#define SENSOR_COL 58

char* ui_pad(char* ptr, const int input_width, const int total_width);
char  ui_twirler(const char prev);

#endif
