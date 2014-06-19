
#ifndef __CLOCK_SERVER_H__
#define __CLOCK_SERVER_H__

#include <std.h>

void __attribute__ ((noreturn)) clock_server(void);

int Time(void);
int Delay(int ticks);
int DelayUntil(int ticks);

#endif
