
#ifndef __IRQ_H__
#define __IRQ_H__

#include <std.h>
#include <debug.h>

#define HWI_HANDLER ((uint* volatile)0x18)

void irq_init(void);
void irq_deinit(void);

void hwi_enter(void);   /* found in context.asm */

#define VIC1_BASE 0x800B0000
#define VIC2_BASE 0x800C0000

#define VIC_IRQ_STATUS_OFFSET    0x00 // asserted when enabled and RAW are asserted
#define VIC_FIQ_STATUS_OFFSET    0x04
#define VIC_RAW_STATUS_OFFSET    0x08 // RAW means hardware is actually asserting
#define VIC_IRQ_MODE_OFFSET      0x0C
#define VIC_IRQ_ENABLE_OFFSET    0x10
#define VIC_IRQ_DISABLE_OFFSET   0x14
#define VIC_SOFT_ENABLE_OFFSET   0x18
#define VIC_SOFT_DISABLE_OFFSET  0x1C
#define VIC_PROTECTION_OFFSET    0x20
#define VIC_VECTOR_ADDRESS       0x30
#define VICVECCNTL_OFFSET        0x200
    #define VICCNTL_ENABLE_MASK  0x20
    #define VICCNTL_INT_MASK     0x1F
#define VICVECADDR_OFFSET        0x100
#define VICDEFVECTADDR_OFFSET    0x34

#define VIC(vic, offset) (*(volatile uint*)(vic|offset))
#define VIC_PTR(vic, offset) (*(void*volatile*)(vic|offset))

static inline uint irq_status(const uint v)          { return VIC(v, VIC_IRQ_STATUS_OFFSET);   }
static inline uint fiq_status(const uint v)          { return VIC(v, VIC_FIQ_STATUS_OFFSET);   }
static inline uint irq_raw_status(const uint v)      { return VIC(v, VIC_RAW_STATUS_OFFSET);   }
static inline uint irq_mode_status(const uint v)     { return VIC(v, VIC_IRQ_MODE_OFFSET);     }
static inline uint irq_enabled_status(const uint v)  { return VIC(v, VIC_IRQ_ENABLE_OFFSET);   }
static inline uint irq_software_status(const uint v) { return VIC(v, VIC_SOFT_ENABLE_OFFSET);  }
static inline bool irq_is_user_protected(const uint v) { return VIC(v, VIC_PROTECTION_OFFSET); }

void irq_enable_user_protection(void);
void irq_disable_user_protection(void);

// For simulating interrupts
void irq_simulate_interrupt(const uint i);
void irq_clear_simulated_interrupt(const uint i);

// For dealing with real interrupts
void irq_enable_interrupt(const uint i);
void irq_disable_interrupt(const uint i);

#ifdef DEBUG
    void debug_interrupt_table(void);
#else
    #define debug_interrupt_table()
#endif

#endif
