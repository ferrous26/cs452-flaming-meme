	.file	"context.asm"
	.text
	.align	2
	.global	kernel_enter
	.type	kernel_enter, %function
kernel_enter:
	mov	r2, lr
	mrs	r3, spsr

	msr	cpsr, #0x9F 		/* system */
	stmfd	sp!, {r0, r2-r12, lr}
	mov	r2, sp
	msr	cpsr, #0x93 		/* supervisor */
	ldmfd   sp!, {r4-r12, lr}
/*	mov     sp, #0x100000 */
	b	syscall_handle (PLT)
	.size	kernel_enter, .-kernel_enter

	.align	2
	.global	kernel_exit
	.type	kernel_exit, %function
kernel_exit:
	@ r0 holds address of user stack
	stmfd	sp!, {r4-r12, lr}

	msr	cpsr, #0x9F		/* System */
	mov	sp, r0
	ldmfd	sp!, {r0, r2-r12, lr}
	msr	cpsr, #0x93		/* Supervisor */

	msr	spsr, r3
	movs	pc, r2
	.size	kernel_exit, .-kernel_exit
