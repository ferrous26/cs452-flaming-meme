	.file	"context.asm"
	.text

	.align	2
	.global	hwi_enter
	.type	hwi_enter, %function
hwi_enter:
	msr	cpsr, #0xDF
	sub	sp, sp, #4
	stmfd	sp!, {r0,r2,r3,ip}
	mov	r0, sp

	msr	cpsr, #0xD2	/* IRQ */
	str	lr, [r0, #16]	/* Save Real LR */
	msr	cpsr, #0xD3	/* supervisor */
	mov	lr, #0		/* you got more work to do */
	mov	r0, #0		/* 0 will be the handle for IRQ */
	.size	hwi_enter, .-hwi_enter

	.align	2
	.global	kernel_enter
	.type	kernel_enter, %function
kernel_enter:
	mrs	r3, spsr
	mov	r2, lr

	msr	cpsr, #0xDF 		/* system */
	stmfd	sp!, {r0-r11, lr}	/* Store user state on the users stack */
	mov	r2, sp			/* kernel needs the sp to find data again */
	msr	cpsr, #0xD3 		/* supervisor */
	
	mov     sp, #0x300000
	b	syscall_handle
	.size	kernel_enter, .-kernel_enter

	.align	2
	.global	kernel_exit
	.type	kernel_exit, %function
kernel_exit:
	@ r0 holds address of user stack
	msr	cpsr, #0xDF		/* System */
	mov	sp, r0
	ldmfd	sp!, {r0-r11, lr}	
	msr	cpsr, #0xD3		/* Supervisor */
	
	msr	spsr, r3
	cmp	r2, #0		
	movnes	pc, r2

	msr	cpsr, #0xDF		/* System */
	mov	r0, sp
	sub	sp, sp, #20
	msr	cpsr, #0xD2		/* IRQ */
	
	mov	sp, r0
	ldmfd	sp, {r0,r2,r3,ip,pc}^	/* ^ acts like movs when pc is in list */
	.size	kernel_exit, .-kernel_exit



