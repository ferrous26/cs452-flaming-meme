#include <vt100.h>
#include <clock.h>
#include <irq.h>
#include <ts7200.h>

#define HWI_HANDLER ((uint* volatile)0x18)

inline static void
_init_vector_irq(const uint interrupt, const uint priority, voidf handle) {
    assert(interrupt < 64, "Invalid Interrupt %d", interrupt);
    assert(priority < 16, "Invalid vector priority on interrupt %d", interrupt);

    const uint base = interrupt > 31 ? VIC2_BASE : VIC1_BASE;
    const uint set  = mod2(interrupt, 32) | VICCNTL_ENABLE_MASK;

    *(volatile voidf* const)(base + VICVECADDR_OFFSET + 4*priority) = handle;
    *(volatile uint*  const)(base + VICVECCNTL_OFFSET + 4*priority) = set;
}

static void default_isr() {}

inline static void _init_all_vector_irq() {
    //setup VEC1
    _init_vector_irq(23, 0, irq_uart1_recv);
    _init_vector_irq(25, 1, irq_uart2_recv);
    _init_vector_irq(24, 2, irq_uart1_send);
    _init_vector_irq(26, 3, irq_uart2_send);

    //setup VEC2
    _init_vector_irq(52, 0, irq_uart1);
    _init_vector_irq(51, 1, irq_clock);
    _init_vector_irq(54, 2, irq_uart2);

    *(voidf*)(VIC1_BASE|VICDEFVECTADDR_OFFSET) = default_isr;
    *(voidf*)(VIC2_BASE|VICDEFVECTADDR_OFFSET) = default_isr;
}

void irq_init() {
    irq_deinit(); // reset!
    *HWI_HANDLER = (0xea000000 | (((uint)hwi_enter >> 2) - 8));

    // irq_enable_user_protection();
    irq_enable_interrupt(23); // UART1 recv
    irq_enable_interrupt(24); // UART1 send
    irq_enable_interrupt(25); // UART2 recv
    irq_enable_interrupt(26); // UART2 send

    irq_enable_interrupt(51); // Timer 3
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
    const uint shift = mod2(interrupt, 32);

    *(volatile uint* const)(base + cmd) = 1 << shift;
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

char* debug_interrupt_table(char* ptr) {
    return sprintf(ptr,
"VIC1 Table                              VIC2 Table\n\r"
"       IRQ Status: %p                          IRQ Status: %p\n\r"
"       FIQ Status: %p                          FIQ Status: %p\n\r"
"       Raw Status: %p                          Raw Status: %p\n\r"
"           Select: %p                              Select: %p\n\r"
"          Enabled: %p                             Enabled: %p\n\r"
"     Soft Enabled: %p                        Soft Enabled: %p\n\r"
"  User Protection: %s                     User Protection: %s\n\r",
irq_status(VIC1_BASE),		        irq_status(VIC2_BASE),
fiq_status(VIC1_BASE),                  fiq_status(VIC2_BASE),
irq_raw_status(VIC1_BASE),		irq_raw_status(VIC2_BASE),
irq_mode_status(VIC1_BASE),		irq_mode_status(VIC2_BASE),
irq_enabled_status(VIC1_BASE),		irq_enabled_status(VIC2_BASE),
irq_software_status(VIC1_BASE),		irq_software_status(VIC2_BASE),
		   irq_is_user_protected(VIC1_BASE) ? "Yes" : "No",
		   irq_is_user_protected(VIC2_BASE) ? "Yes" : "No");
}
