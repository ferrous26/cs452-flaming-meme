	.file	"context.asm"
	.text
	.align	2
	.global	syscall_enter
	.type	syscall_enter, %function
syscall_enter:
	ldr	r0, [lr, #-4]		/* load bwi instruction into r0 */
	and	r0, r0, #0x00FFFFFF	/* extract the code from bwi instruction ~ [24:0] */

	movs	pc, lr
	.size syscall_enter, .-processor_mode


