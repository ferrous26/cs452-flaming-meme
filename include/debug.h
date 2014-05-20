
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <std.h>

#ifdef ASSERT
#define assert(expr, msg, ...)						\
    {									\
	if (!(expr))							\
	    debug_assert_fail(__FILE__, __LINE__, msg, ##__VA_ARGS__);	\
    }
#else
#define assert(expr, msg, ...)
#endif

#define DEBUG_HOME 2
#define DEBUG_OFFSET 50
#define DEBUG_HISTORY_SIZE 36
#define DEBUG_END (DEBUG_HOME + DEBUG_HISTORY_SIZE)

void debug_init();
void debug_log(const char* const message, ...);
void debug_assert_fail(const char* const file,
		       const uint line,
		       const char* const msg, ...);

typedef enum {
    USER       = 0x10,
    FIQ        = 0x11,
    IRQ        = 0x12,
    SUPERVISOR = 0x13,
    ABORT      = 0x16,
    UNDEFINED  = 0x1b,
    SYSTEM     = 0x1f
} pmode;

inline pmode debug_processor_mode(void);
void debug_cpsr(void);
void debug_spsr(void);

#endif
