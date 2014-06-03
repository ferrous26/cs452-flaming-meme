#include <vt100.h>
#include <clock.h>

#include <irq.h>

void __attribute__ ((used)) irq1() {
    vt_log("IRQ1");
    irq_clear_simulated_interrupt(1);
}

void __attribute__ ((used)) irq0() {
    vt_log("IRQ0");
    irq_clear_simulated_interrupt(0);
}

void __attribute__ ((used)) irq63() {
    vt_log("IRQ63");
    irq_clear_simulated_interrupt(63);
}

void irq_init() {
    //EVERYTHING OFF
    VIC(VIC1_BASE, VIC_IRQ_DISABLE_OFFSET) = 0xFFFFFFFFul;
    VIC(VIC2_BASE, VIC_IRQ_DISABLE_OFFSET) = 0xFFFFFFFFul;

    // make sure everything goes through IRQ and not FIQ
    VIC(VIC1_BASE, VIC_IRQ_MODE_OFFSET)    = 0;
    VIC(VIC2_BASE, VIC_IRQ_MODE_OFFSET)    = 0;

    // *HWI_HANDLER = (0xea000000 | (((int)hwi_enter >> 2) - 4));
    *(void*volatile*)0x38 = hwi_enter;

    // irq_enable_user_protection();
    irq_enable_interrupt(0);  // Soft Interrupt
    irq_enable_interrupt(1);  // Soft Interrupt
    irq_enable_interrupt(63); // Soft Interrupt

    irq_enable_interrupt(51); // Timer 3

    *(void*volatile*)0x800B0100 = irq0;
    *(volatile uint*)0x800B0200 = 0x20;

    *(void*volatile*)0x800B0104 = irq1;
    *(volatile uint*)0x800B0204 = 0x21;

    *(void*volatile*)0x800C0100 = irq_clock;
    *(volatile uint*)0x800C0200 = 0x33;

    *(void*volatile*)0x800C0104 = irq63;
    *(volatile uint*)0x800C0204 = 0x3F;
    
    /*
    irq_enable_interrupt(23); // UART1 recv
    irq_enable_interrupt(24); // UART1 send
    irq_enable_interrupt(25); // UART2 recv
    irq_enable_interrupt(26); // UART2 send
    */
    /*
    irq_enable_interrupt(52); // UART1 general
    irq_enable_interrupt(54); // UART2 general 
    */
}

inline static void _irq_interrupt(const uint cmd, const uint interrupt) {
    const uint base  = interrupt >> 5 ? VIC2_BASE : VIC1_BASE;
    const uint shift = interrupt & 0x1F;
    VIC(base, cmd)   = 1 << shift;
}

void irq_enable_user_protection() {
    VIC(VIC1_BASE, VIC_PROTECTION_OFFSET) = 1;
    VIC(VIC2_BASE, VIC_PROTECTION_OFFSET) = 1;
}

void irq_disable_user_protection() {
    VIC(VIC1_BASE, VIC_PROTECTION_OFFSET) = 0;
    VIC(VIC2_BASE, VIC_PROTECTION_OFFSET) = 0;
}

void irq_enable_interrupt(const uint i) {
    _irq_interrupt(VIC_IRQ_ENABLE_OFFSET, i);
}

void irq_disable_interrupt(const uint i) {
    _irq_interrupt(VIC_IRQ_DISABLE_OFFSET, i);
}

void irq_simulate_interrupt(const uint i) {
    _irq_interrupt(VIC_SOFT_ENABLE_OFFSET, i);
}

void irq_clear_simulated_interrupt(const uint i) {
    _irq_interrupt(VIC_SOFT_DISABLE_OFFSET, i);
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

