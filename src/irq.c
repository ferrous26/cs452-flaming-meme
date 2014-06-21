#include <vt100.h>
#include <clock.h>

#include <irq.h>

#define DEFAULT_LDR_PC 0xE59FF018

inline static void
_init_vector_irq(const uint interrupt, const uint priority, voidf handle) {

    assert(interrupt < 64,
	   "Invalid Interrupt %d", interrupt);

    assert(priority < 16,
	   "Invalid vector priority on interrupt %d", interrupt);

    const uint base = interrupt > 31 ? VIC2_BASE : VIC1_BASE;
    const uint set  = mod2(interrupt, 32) | VICCNTL_ENABLE_MASK;

    *(voidf*)(base + VICVECADDR_OFFSET + 4*priority) = handle;
    *(uint*) (base + VICVECCNTL_OFFSET + 4*priority) = set;
}

static void _irq_default1() {
    char buffer[1024];
    char* ptr = buffer;
    ptr  = debug_interrupt_table(buffer);
    *ptr = '\0';
    Abort(__FILE__, __LINE__, "Default IRQ handler1 was triggered:\n%s", buffer);
}

static void _irq_default2() {
    char buffer[1024];
    char* ptr = buffer;
    ptr  = debug_interrupt_table(buffer);
    *ptr = '\0';
    Abort(__FILE__, __LINE__, "Default IRQ handler2 was triggered:\n%s", buffer);
}

inline static void _init_all_vector_irq() {
    //setup VEC1
    _init_vector_irq(25, 0, irq_uart2_recv);
    _init_vector_irq(23, 1, irq_uart1_recv);
    _init_vector_irq(26, 2, irq_uart2_send);
    _init_vector_irq(24, 3, irq_uart1_send);

    //setup VEC2
    _init_vector_irq(54, 0, irq_uart2);
    _init_vector_irq(52, 1, irq_uart1);
    _init_vector_irq(51, 2, irq_clock);
    
    *(voidf*)(VIC1_BASE|VICDEFVECTADDR_OFFSET) = _irq_default1;
    *(voidf*)(VIC2_BASE|VICDEFVECTADDR_OFFSET) = _irq_default2;
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
    
    *(volatile uint* const)(interrupt + cmd) = 1 << shift;
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
"VIC1 Table                              VIC2 Table\n"
"       IRQ Status: %p                          IRQ Status: %p\n"
"       FIQ Status: %p                          FIQ Status: %p\n"
"       Raw Status: %p                          Raw Status: %p\n"
"           Select: %p                              Select: %p\n"
"          Enabled: %p                             Enabled: %p\n"
"     Soft Enabled: %p                        Soft Enabled: %p\n"
"  User Protection: %s                     User Protection: %s\n",
irq_status(VIC1_BASE),		        irq_status(VIC2_BASE),
fiq_status(VIC1_BASE),                  fiq_status(VIC2_BASE),
irq_raw_status(VIC1_BASE),		irq_raw_status(VIC2_BASE),
irq_mode_status(VIC1_BASE),		irq_mode_status(VIC2_BASE),
irq_enabled_status(VIC1_BASE),		irq_enabled_status(VIC2_BASE),
irq_software_status(VIC1_BASE),		irq_software_status(VIC2_BASE),
		   irq_is_user_protected(VIC1_BASE) ? "Yes" : "No",
		   irq_is_user_protected(VIC2_BASE) ? "Yes" : "No");
}
