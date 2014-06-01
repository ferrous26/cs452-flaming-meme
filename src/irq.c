#include <irq.h>

void irq_init() {

    // TODO: turn on bits for appropriate hardware, for now
    //       just make sure everything is being turned off
    VIC(VIC1_BASE, VIC_IRQ_ENABLE_OFFSET) = 0;
    VIC(VIC2_BASE, VIC_IRQ_ENABLE_OFFSET) = 0;

    // make sure everything goes through IRQ and not FIQ
    VIC(VIC1_BASE, VIC_IRQ_MODE_OFFSET)   = 0;
    VIC(VIC2_BASE, VIC_IRQ_MODE_OFFSET)   = 0;

    // *HWI_HANDLER = (0xea000000 | (((int)hwi_enter >> 2) - 4));
    *(void**)0x38 = hwi_enter;

    // TODO: find out why user protection breaks things...
    // task launcher is a user task so it cant start up interrupts 
    // without a syscall
    // irq_enable_user_protection();
  

    irq_enable_interrupt(VIC1_BASE, 0xF000);
}

void irq_enable_user_protection() {
    VIC(VIC1_BASE, VIC_PROTECTION_OFFSET) = 0x1;
    VIC(VIC2_BASE, VIC_PROTECTION_OFFSET) = 0x1;
}

void irq_disable_user_protection() {
    VIC(VIC1_BASE, VIC_PROTECTION_OFFSET) = 0x0;
    VIC(VIC2_BASE, VIC_PROTECTION_OFFSET) = 0x0;
}

void irq_enable_interrupt(const uint v, const uint i) {
    VIC(v, VIC_IRQ_ENABLE_OFFSET) = i;
}

void irq_clear_interrupt(const uint v, const uint i) {
    VIC(v, VIC_IRQ_DISABLE_OFFSET) = i;
}

void irq_simulate_interrupt(const uint v, const uint i) {
    VIC(v, VIC_SOFT_ENABLE_OFFSET) = i;
}

void irq_clear_simulated_interrupt(const uint v, const uint i) {
    VIC(v, VIC_SOFT_DISABLE_OFFSET) = i;
}

#if DEBUG
void debug_interrupt_table() {
    uint base = VIC1_BASE;

    for (size table = 1; table < 3; table++) {
	debug_log("VIC%d Table\n"
		  "       IRQ Status: %p\n"
		  "       FIQ Status: %p\n"
		  "       Raw Status: %p\n"
		  "           Select: %p\n"
		  "          Enabled: %p\n"
		  "     Soft Enabled: %p\n"
		  "  User Protection: %s\n",
		  table,
		  irq_status(base),
		  fiq_status(base),
		  irq_raw_status(base),
		  irq_mode_status(base),
		  irq_enabled_status(base),
		  irq_software_status(base),
		  irq_is_user_protected(base) ? "Yes" : "No");
	base += 0x10000;
    }
}
#endif
