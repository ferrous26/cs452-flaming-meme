#include <vt100.h>
#include <clock.h>

#include <irq.h>

#define DEFAULT_LDR_PC 0xE59FF018


#define SOFT_IRQ_HANDLE(name, irq)              \
static void __attribute__ ((used)) name() {     \
    vt_log("IRQ " #irq);                        \
    vt_flush();                                 \
    irq_clear_simulated_interrupt(irq);         \
}                                               \

SOFT_IRQ_HANDLE(irq0, 0)
SOFT_IRQ_HANDLE(irq1, 1)
SOFT_IRQ_HANDLE(irq63, 63)

inline static void
_init_vector_irq(const uint interrupt, const int priority, voidf handle) {
    assert(interrupt < 64, "Invalid Interrupt %d", interrupt);
    assert(priority < 16, "Invalid vector priority on interrupt %d", interrupt);

    const uint base = interrupt > 31 ? VIC2_BASE : VIC1_BASE;
    const uint set  = (interrupt & VICCNTL_INT_MASK) | VICCNTL_ENABLE_MASK;

    *(voidf*)(base + VICVECADDR_OFFSET + 4*priority) = handle;
    *(uint*) (base + VICVECCNTL_OFFSET + 4*priority) = set;
}

SOFT_IRQ_HANDLE(uart1, 52)

inline static void _init_all_vector_irq() {
    //setup VEC1
    _init_vector_irq(25, 0, irq_uart2_send);
    _init_vector_irq(26, 1, irq_uart2_recv);
    /*
    _init_vector_irq(23, 2, irq_uart1_send);
    _init_vector_irq(24, 3, irq_uart2_recv);
    */

    //setup VEC2
    _init_vector_irq(54, 0, irq_uart2);
    _init_vector_irq(52, 1, irq_uart1);
    _init_vector_irq(51, 2, irq_clock);
    _init_vector_irq(63, 3, irq63);
}

void irq_init() {
    irq_deinit(); // reset!
    *HWI_HANDLER = (0xea000000 | (((uint)hwi_enter >> 2) - 8));

    // irq_enable_user_protection();
    irq_enable_interrupt(0);  // Soft Interrupt
    irq_enable_interrupt(1);  // Soft Interrupt
    irq_enable_interrupt(63); // Soft Interrupt
    irq_enable_interrupt(51); // Timer 3

    /*
    irq_enable_interrupt(23); // UART1 recv
    irq_enable_interrupt(24); // UART1 send
    */
    irq_enable_interrupt(25); // UART2 recv
    irq_enable_interrupt(26); // UART2 send
  
    irq_enable_interrupt(52); // UART1 general
    irq_enable_interrupt(54); // UART2 general

    _init_all_vector_irq();
}

void irq_deinit() {
    //EVERYTHING OFF
    VIC(VIC1_BASE, VIC_IRQ_DISABLE_OFFSET) = 0xFFFFFFFF;
    VIC(VIC2_BASE, VIC_IRQ_DISABLE_OFFSET) = 0xFFFFFFFF;

    // make sure everything goes through IRQ and not FIQ
    VIC(VIC1_BASE, VIC_IRQ_MODE_OFFSET) = 0;
    VIC(VIC2_BASE, VIC_IRQ_MODE_OFFSET) = 0;

    irq_disable_user_protection();
    *HWI_HANDLER = DEFAULT_LDR_PC;
}

inline static void _irq_interrupt(const uint cmd, const uint interrupt) {
    assert(interrupt < 64, "Invalid Interrupt %d", interrupt);
    
    const uint base  = interrupt > 31 ? VIC2_BASE : VIC1_BASE;
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

#ifdef DEBUG
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

