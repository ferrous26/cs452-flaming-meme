#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <kernel.h>
#include <kernel/arch/arm920/ts7200.h>

void clock_init(void) TEXT_COLD;
void clock_deinit(void) TEXT_COLD;

void clock_t4enable(void) TEXT_COLD;

uint __attribute__ ((pure)) clock_t4tick(void);
void irq_clock(void) TEXT_HOT;

#endif
