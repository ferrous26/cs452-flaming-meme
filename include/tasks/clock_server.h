
#ifndef __CLOCK_SERVER_H__
#define __CLOCK_SERVER_H__

#include <std.h>

typedef enum {
    CLOCK_NOTIFY = 1,
    CLOCK_DELAY = 2,
    CLOCK_TIME = 3,
    CLOCK_DELAY_UNTIL = 4
} clock_req_type;

typedef struct {
    clock_req_type type;
    uint ticks;
} clock_req;

void clock_server();

#endif
