
#ifndef __SENSOR_NAME_H__
#define __SENSOR_NAME_H__

typedef struct {
    short bank;
    short num;
} sensor_name;

static inline int __attribute__ ((const, always_inline, used))
sensorname_to_num(const int bank, const int num) {
    return ((bank-'A') << 4) + (num-1);
}

static inline sensor_name __attribute__ ((const, always_inline, used))
sensornum_to_name(const int num) {
    sensor_name name = {
        .bank = 'A' + (num >>4),
        .num  = (num & 15) + 1
    };
    return name;
}

#endif

