
#ifndef __IRQ_H__
#define __IRQ_H__

#include <std.h>
#include <debug.h>

void irq_handle(const uint codehi, const uint codelo, uint* const sp);

#define VIC1_BASE 0x800B0000
#define VIC2_BASE 0x800C0000

#define VIC_IRQ_STATUS_OFFSET    0x00
#define VIC_FIQ_STATUS_OFFSET    0x04
#define VIC_RAW_STATUS_OFFSET    0x08
#define VIC_IRQ_MODE_OFFSET      0x0C
#define VIC_IRQ_ENABLED_OFFSET   0x10
#define VIC_IRQ_DISABLE_OFFSET   0x14
#define VIC_SOFT_ENABLED_OFFSET  0x18
#define VIC_SOFT_DISABLE_OFFSET  0x1C
#define VIC_PROTECTION_OFFSET    0x20
#define VIC_VECTOR_ADDRESS       0x30

#define READ_VIC(vic, offset) *((uint32*)(vic|offset))

static inline uint32 irq_status(const uint vic) {
    return READ_VIC(vic, VIC_IRQ_STATUS_OFFSET);
}

static inline uint32 fiq_status(const uint vic) {
    return READ_VIC(vic, VIC_FIQ_STATUS_OFFSET);
}

static inline uint32 irq_raw_status(const uint vic) {
    return READ_VIC(vic, VIC_RAW_STATUS_OFFSET);
}

static inline uint32 irq_mode_status(const uint vic) {
    return READ_VIC(vic, VIC_IRQ_MODE_OFFSET);
}

static inline uint32 irq_enabled_status(const uint vic) {
    return READ_VIC(vic, VIC_IRQ_ENABLED_OFFSET);
}

static inline uint32 irq_software_enabled_status(const uint vic) {
    return READ_VIC(vic, VIC_SOFT_ENABLED_OFFSET);
}

static inline bool irq_is_user_protected(const uint vic) {
    return READ_VIC(vic, VIC_PROTECTION_OFFSET);
}

#if DEBUG
void debug_interrupt_table();
#endif

#endif
