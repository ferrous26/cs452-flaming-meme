
#ifndef __IRQ_H__
#define __IRQ_H__

#include <std.h>
#include <debug.h>

void irq_init();

#define VIC1_BASE 0x800B0000
#define VIC2_BASE 0x800C0000

#define VIC_IRQ_STATUS_OFFSET    0x00
#define VIC_FIQ_STATUS_OFFSET    0x04
#define VIC_RAW_STATUS_OFFSET    0x08
#define VIC_IRQ_MODE_OFFSET      0x0C
#define VIC_IRQ_ENABLE_OFFSET    0x10
#define VIC_IRQ_DISABLE_OFFSET   0x14
#define VIC_SOFT_ENABLE_OFFSET   0x18
#define VIC_SOFT_DISABLE_OFFSET  0x1C
#define VIC_PROTECTION_OFFSET    0x20
#define VIC_VECTOR_ADDRESS       0x30

#define VIC(vic, offset) (*((uint*)(vic|offset)))

static inline uint irq_status(const uint v)          { return VIC(v, VIC_IRQ_STATUS_OFFSET);   }
static inline uint fiq_status(const uint v)          { return VIC(v, VIC_FIQ_STATUS_OFFSET);   }
static inline uint irq_raw_status(const uint v)      { return VIC(v, VIC_RAW_STATUS_OFFSET);   }
static inline uint irq_mode_status(const uint v)     { return VIC(v, VIC_IRQ_MODE_OFFSET);     }
static inline uint irq_enabled_status(const uint v)  { return VIC(v, VIC_IRQ_ENABLE_OFFSET);   }
static inline uint irq_software_status(const uint v) { return VIC(v, VIC_SOFT_ENABLE_OFFSET);  }
static inline bool irq_is_user_protected(const uint v) { return VIC(v, VIC_PROTECTION_OFFSET); }

void irq_enable_user_protection();
void irq_disable_user_protection();

void irq_simulate_interrupt(const uint v, const uint i);
void irq_clear_simulated_interrupt(const uint v, const uint i);


#if DEBUG
void debug_interrupt_table();
#endif

#endif
