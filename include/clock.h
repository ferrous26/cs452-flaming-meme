#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <std.h>
#include <kernel.h>
#include <ts7200.h>

void clock_enable(void);
void clock_t4enable(void);

uint __attribute__ ((pure)) clock_t4tick(void);
void irq_clock(void) TEXT_HOT;

#endif
