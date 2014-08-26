#ifndef __DEBUG_CPU_H__
#define __DEBUG_CPU_H__

#include <std.h>
#include <syscall.h>

typedef enum {
    USER       = 0x10,
    FIQ        = 0x11,
    IRQ        = 0x12,
    SUPERVISOR = 0x13,
    ABORT      = 0x16,
    UNDEFINED  = 0x1B,
    SYSTEM     = 0x1F
} pmode;

pmode processor_mode(void) TEXT_COLD;
void  debug_cpsr(void) TEXT_COLD;
void  debug_spsr(void) TEXT_COLD;

#endif
