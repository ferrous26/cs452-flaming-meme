#include <kernel.h>

static inline void kernel_exit()
{
    //everythings done, just restore user space and jump back
    asm volatile ("mov    r0, %0                                \n"
                  "msr	  cpsr, #0xDF             /* System */  \n"
                  "mov	  sp, r0                                \n"
                  "ldmfd  sp!, {r0-r11, lr}                     \n"
                  "msr    cpsr, #0xD3		/* Supervisor */\n"

                  "msr    spsr, r3                              \n"
                  "cmp    r2, #0                                \n"
                  "movnes pc, r2                                \n"

                  "msr    cpsr, #0xDF            /* System */   \n"
                  "mov    r0, sp                                \n"
                  "add    sp, sp, #20                           \n"
                  "msr    cpsr, #0xD3           /* Supervisor */\n"

                  "/* ^ acts like movs when pc is in list */    \n"
                  "ldmfd  r0, {r0,r2,r3,r12,pc}^                \n"
                  :
                  : "r" (task_active->sp)
                  : "r0");
}
