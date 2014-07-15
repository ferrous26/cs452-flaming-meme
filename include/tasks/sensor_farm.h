
#ifndef __SENSOR_FARM_H__
#define __SENSOR_FARM_H__

#define SENSOR_FARM_NAME "ENRIQUE"
#define SENSOR_POLL_NAME "SR_POLL"

typedef enum {
    SF_U_SENSOR,
    SF_D_SENSOR,
    SF_D_SENSOR_ANY
} sf_type;

typedef struct {
    sf_type type;
    int     sensor;
} sf_req;

void __attribute__ ((noreturn)) sensor_farm();

int delay_sensor_any(void);
int delay_sensor(const int sensor_bank, const int sensor_num);

#endif

