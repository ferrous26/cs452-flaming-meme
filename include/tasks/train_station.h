
#ifndef __TRAIN_STATION_H__
#define __TRAIN_STATION_H__

#define TRAIN_STATION_NAME (char*)"TRAINS"

#define TRAIN_COUNT 7
#define TRAIN_REVERSE_DELAY_FACTOR 40

typedef enum {
    TRAIN_WHOAMI,
    TRAIN_REQUEST,
    TRAIN_CHANGE_SPEED,
    TRAIN_REVERSE_DIRECTION,
    TRAIN_TOGGLE_LIGHT,
    TRAIN_HORN_SOUND
} train_req_type;

typedef struct {
    train_req_type type;
    int train;
    int arg;
} train_req;

void train_reverse(int train);
void train_set_speed(int train, int speed);
void train_toggle_light(int train);
void train_sound_horn(int train);

void __attribute__ ((noreturn)) train_station(void);

#endif
