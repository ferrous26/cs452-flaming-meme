	.file	"context.asm"
	.text


	.align	2
	.section .text.kern
	.global	 hwi_enter
	.type	 hwi_enter, %function
hwi_enter:
	msr	cpsr, #0xDF	/* System */
	sub	sp, sp, #4
	stmfd	sp!, {r0,r2,r3,r12}
	mov	r0, sp
	msr	cpsr, #0xD2	/* IRQ */

	sub	lr, lr, #4
	str	lr, [r0, #16]	/* Save Real LR */

	mov	lr, #0
	mov	r0, #0

	.global	kernel_enter
	.type	kernel_enter, %function
kernel_enter:
	mov	r2, lr
	mrs	r3, spsr

	msr	cpsr, #0xDF 		/* system */
	stmfd	sp!, {r0-r11, lr}	/* Store user state on the users stack */
	mov	r2, sp			/* kernel needs the sp to find data again */
	msr	cpsr, #0xD3 		/* supervisor */

	mov     sp, #0x300000
	b	syscall_handle
	.size	kernel_enter, .-kernel_enter
	.size	hwi_enter, .-hwi_enter
