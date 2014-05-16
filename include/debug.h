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
void debug_init(void);
    name.count = 0;

#define DEBUG_HOME 5
#define DEBUG_OFFSET 60
#define DEBUG_TIME_PRINT_WORST(name)			\
    debug_message(#name ".worst: %u", name.worst);
#define DEBUG_HISTORY_SIZE 35
#define DEBUG_END  (DEBUG_HOME + DEBUG_HISTORY_SIZE)

void debug_message(const char* const message, ...);
void debug_cpsr(void);
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

typedef enum {
    USER       = 0x10,
    FIQ        = 0x11,
    IRQ        = 0x12,
    SUPERVISOR = 0x13,
    ABORT      = 0x16,
    UNDEFINED  = 0x1b,
    SYSTEM     = 0x1f
} pmode;

#else
#define DEBUG_TIME(name)
#define DEBUG_TIME_INIT(name)
#define DEBUG_TIME_PRINT_WORST(name)
inline pmode debug_processor_mode(void);
#define DEBUG_TIME_PRINT_AVERAGE(name)
#define DEBUG_TIME_LAP(name, threshold)
#endif

#endif
