
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <std.h>
#include <vt100.h>

#ifdef ASSERT
#define assert(expr, msg, ...)						\
    {									\
	if (!(expr))							\
	    debug_assert_fail(__FILE__, __LINE__, msg, ##__VA_ARGS__);	\
    }
#else
#define assert(expr, msg, ...)
#endif

#ifdef DEBUG

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

pmode debug_processor_mode(void);
void debug_cpsr(void);
void debug_spsr(void);

#else

#define debug_log( ... )
#define debug_assert_fail
#define debug_processor_mode()
#define debug_cpsr()
#define debug_spsr()

#endif

#endif
