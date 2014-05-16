	.file	"context.asm"
	.text
	.align	2
	.global	syscall_enter
	.type	syscall_enter, %function
syscall_enter:
	# msr	cpsr, #0x9F /* system */
	stmfd	sp!, {r0-lr}
	# msr	cpsr, #0x93 /* supervisor */

	bl syscall_handle(PLT)

	# msr	cpsr, #0x9F /* system */
	ldmfd	sp!, {r0-lr}
	# msr	cpsr, #0x93 /* supervisor */

	mov	r0, #100
	movs	pc, lr
	.size	syscall_enter, .-syscall_enter


