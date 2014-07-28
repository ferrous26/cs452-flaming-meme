
#ifndef __SENSOR_FARM_H__
#define __SENSOR_FARM_H__

#define SENSOR_FARM_NAME "ENRIQUE"
#define SENSOR_POLL_NAME "SR_POLL"

typedef enum {
    SF_U_SENSOR,
    SF_D_SENSOR,
    SF_D_SENSOR_ANY,
    SF_W_SENSORS,

    SF_REQ_TYPE_COUNT
} sf_type;

typedef struct sf_req {
    sf_type type;
    union {
        int sensor;
        
        struct update {
            int sensor;
            int time;
        } update;
        
        struct rev_list {
            int size;
            struct rev_list_ele {
                int sensor;
                int tid;
            } ele[8];
        } rev_list;

    } body;
} sf_req;

void __attribute__ ((noreturn)) sensor_farm(void);

int delay_sensor_any(void);
int delay_sensor(const int sensor_bank, const int sensor_num);

#endif

