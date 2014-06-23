
#ifndef __CLOCK_SERVER_H__
#define __CLOCK_SERVER_H__

#include <std.h>

#define CLOCK_SERVER_NAME   "CLOCK"
#define CLOCK_UI_NAME       "CL_UI"
#define CLOCK_NOTIFIER_NAME "CL_NOTE"

void __attribute__ ((noreturn)) clock_server(void);

typedef enum {
    INVALID_DELAY = -60
} clock_err;

int Time(void);
int Delay(int ticks);
int DelayUntil(int ticks);

#endif
