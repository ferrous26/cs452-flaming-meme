#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <std.h>

void debug_init(void);

#define DEBUG_HOME 5
#define DEBUG_OFFSET 60
#define DEBUG_HISTORY_SIZE 35
#define DEBUG_END  (DEBUG_HOME + DEBUG_HISTORY_SIZE)

void debug_message(const char* const message, ...);
void debug_cpsr(void);


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

#endif
