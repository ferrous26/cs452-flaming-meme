
#include <debug.h>
#include <io.h>
#include <vt100.h>
#include <kernel.h>

#if DEBUG

void debug_assert_fail(const char* const file,
		       const uint line,
		       const char* const msg, ...) {
    va_list args;
    va_start(args, msg);

    vt_log_start();
    kprintf("assertion failure at %s:%u\n\r", file, line);
    kprintf_va(msg, args);
    vt_log_end();
    vt_flush();

    va_end(args);
    shutdown();
}

void debug_log(const char* const msg, ...) {
    va_list args;
    va_start(args, msg);

    vt_log_start();
    kprintf_va(msg, args);
    vt_log_end();
    vt_flush();

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

static inline const char* processor_mode_string(uint status) {
    switch (status & CPU_MODE_MASK) {
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

    debug_log("CPSR Information");
    debug_log("       Current Mode: %s", processor_mode_string(status));
    debug_log("        Thumb State: %s", thumb_state(status) ? 'Yes' : 'No');
    debug_log("        FIQ Enabled: %s", fiq_state(status)   ? 'Yes' : 'No');
    debug_log("        IRQ Enabled: %s", irq_state(status)   ? 'No'  : 'Yes');
    debug_log("      Abort Enabled: %s", abort_state(status) ? 'No'  : 'Yes');
    debug_log("    Condition Codes: %c %c %c %c %c",
	      cc_n(status) ? 'N' : '_',
	      cc_z(status) ? 'Z' : '_',
	      cc_c(status) ? 'C' : '_',
	      cc_v(status) ? 'V' : '_',
	      cc_s(status) ? 'S' : '_');
}

void debug_spsr(void) {
    const uint status;
    asm("mrs %0, spsr" : "=r" (status));

    debug_log("SPSR Information");
    debug_log("       Current Mode: %s", processor_mode_string(status));
    debug_log("        Thumb State: %s", thumb_state(status) ? 'Yes' : 'No');
    debug_log("        FIQ Enabled: %s", fiq_state(status)   ? 'Yes' : 'No');
    debug_log("        IRQ Enabled: %s", irq_state(status)   ? 'No'  : 'Yes');
    debug_log("      Abort Enabled: %s", abort_state(status) ? 'No'  : 'Yes');
    debug_log("    Condition Codes: %c %c %c %c %c",
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

#endif
