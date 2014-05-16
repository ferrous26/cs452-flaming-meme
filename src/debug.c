#include <debug.h>
#include <io.h>
#include <vt100.h>

// keep track of where to put debug messages
static uint32 error_line  = 0;
static uint32 error_count = 0;

void debug_init(void) {
    error_line  = DEBUG_HOME - 1;
    error_count = 0;
}

void debug_message(const char* const msg, ...) {

    error_line++;
    error_count++;
    if (error_line == DEBUG_END) error_line = DEBUG_HOME; // reset

    vt_goto(error_line, DEBUG_OFFSET);
    vt_kill_line();
    kprintf_uint(error_count);
    kprintf_string(": ");

    va_list args;
    va_start(args, msg);
    kprintf_va(msg, args);
    va_end(args);
}



#define CPU_MODE_MASK             0x0000001f

#define THUMB_STATUS_MASK         0x00000020
#define FIQ_STATUS_MASK           0x00000040
#define IRQ_STATUS_MASK           0x00000080
#define ABORT_STATUS_MASK         0x00000100
#define ENDIANESS_STATUS_MASK     0x00000200

#define FLAG_NEGATIVE_MASK        0x80000000
#define FLAG_ZERO_MASK            0x40000000
#define FLAG_CARRY_MASK           0x20000000
#define FLAG_OVERFLOW_MASK        0x10000000
#define FLAG_STICKY_OVERFLOW_MASK 0x08000000

static inline const char* processor_mode_string(void) {
    switch (debug_processor_mode()) {
    case USER:       return "user";
    case FIQ:        return "fiq";
    case IRQ:        return "irq";
    case SUPERVISOR: return "supervisor";
    case ABORT:      return "abort";
    case UNDEFINED:  return "undefined";
    case SYSTEM:     return "system";
    default:         return "ERROR";
    }
}

static inline bool thumb_state(const uint status) { return (status & THUMB_STATUS_MASK);     }
static inline bool   fiq_state(const uint status) { return (status & FIQ_STATUS_MASK);       }
static inline bool   irq_state(const uint status) { return (status & IRQ_STATUS_MASK);       }
static inline bool abort_state(const uint status) { return (status & ABORT_STATUS_MASK);     }
static inline bool   endianess(const uint status) { return (status & ENDIANESS_STATUS_MASK); }

static inline bool cc_n(const uint status) { return (status & FLAG_NEGATIVE_MASK);        }
static inline bool cc_z(const uint status) { return (status & FLAG_ZERO_MASK);            }
static inline bool cc_c(const uint status) { return (status & FLAG_CARRY_MASK);           }
static inline bool cc_v(const uint status) { return (status & FLAG_OVERFLOW_MASK);        }
static inline bool cc_s(const uint status) { return (status & FLAG_STICKY_OVERFLOW_MASK); }

/**
 * http://www.heyrick.co.uk/armwiki/The_Status_register
 */
void debug_cpsr(void) {

    const uint status;
    asm("mrs %0, cpsr" : "=r" (status));

    debug_message("CSPR Information");
    debug_message("       Current Mode: %s", processor_mode_string());
    debug_message("        Thumb State: %s", thumb_state(status) ? 'Yes' : 'No');
    debug_message("        FIQ Enabled: %s", fiq_state(status)   ? 'Yes' : 'No');
    debug_message("        IRQ Enabled: %s", irq_state(status)   ? 'No'  : 'Yes');
    debug_message("      Abort Enabled: %s", abort_state(status) ? 'No'  : 'Yes');
    debug_message("    Condition Codes: %c %c %c %c %c",
		  cc_n(status) ? 'N' : '_',
		  cc_z(status) ? 'Z' : '_',
		  cc_c(status) ? 'C' : '_',
		  cc_v(status) ? 'V' : '_',
		  cc_s(status) ? 'S' : '_');
}

inline pmode debug_processor_mode() {
    const uint status;
    asm("mrs %0, cpsr" : "=r" (status));

    return (status & CPU_MODE_MASK);
}
