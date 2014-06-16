
#ifndef __BENCHMARK_H__
#define __BENCHMARK_H__

// only bring things in if they are needed
#ifdef BENCHMARK

#include <std.h>
#include <clock.h>
#include <vt100.h>

struct benchmark {
    uint prev;
    uint curr;
    uint worst;
    uint total;
    uint count;
};

/**
 * Put this somewhere in global space to setup the variables needed.
 */
#define BENCH(name) struct benchmark name
#define BENCH_EXTERN(name) extern struct benchmark name

/**
 * Put this in the library init function to initialize the state.
 */
#define BENCH_START(name) {			\
	name.curr  = 0;				\
	name.worst = 0;				\
	name.total = 0;				\
	name.count = 0;				\
	name.prev  = clock_t4tick();		\
    }

/* (╯°□°）╯︵ ┻━┻ */
#define BENCH_LAP(name)							\
    {									\
        name.curr = clock_t4tick();					\
        uint name ## _diff = name.curr - name.prev;			\
	name.prev = name.curr;						\
	name.total += name ## _diff;					\
	name.count++;							\
	if (name ## _diff > name.worst)					\
	    name.worst = name ## _diff;					\
    }

#define BENCH_PRINT_WORST(name)			\
    log(#name ".worst: %u", name.worst)

#define BENCH_PRINT_AVERAGE(name)				\
    log(#name ".avg: %u / (%u * 983.04)", name.total, name.count)

#else

#define BENCH(name)
#define BENCH_EXTERN(name)
#define BENCH_START(name)
#define BENCH_LAP(name)
#define BENCH_PRINT_WORST(name)
#define BENCH_PRINT_AVERAGE(name)

#endif
#endif
