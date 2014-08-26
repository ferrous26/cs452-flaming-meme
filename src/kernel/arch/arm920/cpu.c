#include <kernel/arch/arm920/cpu.h>
#include <vt100.h>
#include <kernel.h>
#include <syscall.h>

#include <tasks/term_server.h>

#define CPU_MODE_MASK             0x0000001f

#define THUMB_STATUS_MASK         0x00000020
#define FIQ_STATUS_MASK           0x00000040
#define IRQ_STATUS_MASK           0x00000080
#define ABORT_STATUS_MASK         0x00000100

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

static inline bool cc_n(const uint status) { return (status & FLAG_NEGATIVE_MASK);        }
static inline bool cc_z(const uint status) { return (status & FLAG_ZERO_MASK);            }
static inline bool cc_c(const uint status) { return (status & FLAG_CARRY_MASK);           }
static inline bool cc_v(const uint status) { return (status & FLAG_OVERFLOW_MASK);        }
static inline bool cc_s(const uint status) { return (status & FLAG_STICKY_OVERFLOW_MASK); }

static void _debug_psr(const char* const name, const uint status) {

    void (*logger)(const char* const,...) = NULL;
    switch (processor_mode()) {
    case USER:
	logger = log;
	break;
    case FIQ:
    case IRQ:
    case SUPERVISOR:
    case ABORT:
    case UNDEFINED:
    case SYSTEM:
	logger = klog;
    };

    logger("%s Information\r\n"
	   "       Current Mode: %s\n"
	   "        Thumb State: %s\n"
	   "        FIQ Enabled: %s\n"
	   "        IRQ Enabled: %s\n"
	   "      Abort Enabled: %s\n"
	   "    Condition Codes: %c %c %c %c %c",
	   name,
	   processor_mode_string(status),
	   thumb_state(status) ? "Yes" : "No",
	   fiq_state(status)   ? "No"  : "Yes",
	   irq_state(status)   ? "No"  : "Yes",
	   abort_state(status) ? "No"  : "Yes",
	   cc_n(status) ? 'N' : '_',
	   cc_z(status) ? 'Z' : '_',
	   cc_c(status) ? 'C' : '_',
	   cc_v(status) ? 'V' : '_',
	   cc_s(status) ? 'S' : '_');
}

/**
 * http://www.heyrick.co.uk/armwiki/The_Status_register
 */
void debug_cpsr(void) {
    uint status;
    asm("mrs %0, cpsr" : "=r" (status));
    _debug_psr("CPSR", status);
}

void debug_spsr(void) {
    uint status;
    asm("mrs %0, spsr" : "=r" (status));
    _debug_psr("SPSR", status);
}

pmode processor_mode() {
    uint status;
    asm("mrs %0, cpsr" : "=r" (status));

    return (status & CPU_MODE_MASK);
}
