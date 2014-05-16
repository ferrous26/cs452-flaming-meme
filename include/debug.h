#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <std.h>
#include <clock.h>

#define BENCHMARK 1

typedef struct {
    uint prev;
    uint curr;
    uint worst;
    uint total;
    uint count;
} benchmark;

#if BENCHMARK

/**
 * Put this somewhere in global space to setup the variables needed.
 */
#define DEBUG_TIME(name) static benchmark name;

/**
 * Put this in the library init function to initialize the state.
 */
#define DEBUG_TIME_INIT(name)			\
    name.prev  = clock_t4tick();		\
    name.curr  = 0;				\
    name.worst = 0;				\
    name.total = 0;				\
    name.count = 0;

#define DEBUG_TIME_PRINT_WORST(name)			\
    debug_message(#name ".worst: %u", name.worst);

#define DEBUG_TIME_PRINT_AVERAGE(name)				\
    debug_message(#name ".avg: %u", name.total / name.count);

/* (╯°□°）╯︵ ┻━┻ */
#define DEBUG_TIME_LAP(name, threshold)					\
    {									\
        name.curr = clock_t4tick();					\
        uint name ## _diff = name.curr - name.prev;			\
        if (name ## _diff > threshold) {				\
	    name.prev = name.curr;					\
	    name.total += name ## _diff;				\
	    name.count++;						\
	    if (name ## _diff > name.worst)				\
		name.worst = name ## _diff;				\
        }								\
    }


#else
#define DEBUG_TIME(name)
#define DEBUG_TIME_INIT(name)
#define DEBUG_TIME_PRINT_WORST(name)
#define DEBUG_TIME_PRINT_AVERAGE(name)
#define DEBUG_TIME_LAP(name, threshold)
#endif

#endif
