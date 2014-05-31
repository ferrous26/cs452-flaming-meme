#include <irq.h>

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
		  "      Soft Enable: %p\n"
		  "  User Protection: %s\n",
		  table,
		  irq_status(base),
		  fiq_status(base),
		  irq_raw_status(base),
		  irq_mode_status(base),
		  irq_enabled_status(base),
		  irq_software_enabled_status(base),
		  irq_is_user_protected(base) ? "Yes" : "No");

	base += 0x10000;
    }
}
#endif
