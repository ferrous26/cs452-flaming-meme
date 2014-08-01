
#ifndef __UI_H__
#define __UI_H__

#include <std.h>

#define TRAIN_BANDWIDTH_ROW   1
#define TRAIN_BANDWIDTH_COL   20
#define TRAIN_BANDWIDTH_UP    27
#define TRAIN_BANDWIDTH_WIDTH 3


#define TRAIN_ROW               4
#define TRAIN_COL(id)          (20 * id)

#define TRAIN_NUMBER_ROW          TRAIN_ROW
#define TRAIN_NUMBER_COL(id)     (8 + TRAIN_COL(id))
#define TRAIN_SPEED_ROW          (TRAIN_ROW + 2)
#define TRAIN_SPEED_COL(id)      (TRAIN_COL(id) + 1)
#define TRAIN_LOCATION_ROW       (TRAIN_SPEED_ROW + 1)
#define TRAIN_LOCATION_COL(id)    TRAIN_SPEED_COL(id)
#define TRAIN_SENSOR_ROW         (TRAIN_LOCATION_ROW + 1)
#define TRAIN_SENSOR_COL(id)      TRAIN_LOCATION_COL(id)
#define TRAIN_PREDICTION_ROW     (TRAIN_SENSOR_ROW + 1)
#define TRAIN_PREDICTION_COL(id)  TRAIN_SENSOR_COL(id)
#define TRAIN_PATH_ROW           (TRAIN_PREDICTION_ROW + 2)
#define TRAIN_PATH_COL(id)        TRAIN_PREDICTION_COL(id)
#define TRAIN_RESERVE_ROW        (TRAIN_PATH_ROW + 6)
#define TRAIN_RESERVE_COL(id)     TRAIN_SENSOR_COL(id)

#define TRAIN_CONSOLE_ROW         TRAIN_LOCATION_ROW
#define TRAIN_CONSOLE_COL(id)    (TRAIN_LOCATION_COL(id) + 14)

#define TURNOUT_ROW (TRAIN_ROW + 21)
#define TURNOUT_COL 5

#define SENSOR_ROW TURNOUT_ROW
#define SENSOR_COL 60

char* ui_pad(char* ptr, const int input_width, const int total_width);
char  ui_twirler(const char prev);

#endif
