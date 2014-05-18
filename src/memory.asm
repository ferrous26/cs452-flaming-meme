	.file	"memory.asm"
	.text
	.align	2
	.global	memcpy, memcpy4, memcpy8, memcpy16, memcpy32, memcmp, memcmp4
	.type	memcpy,   %function
	.type	memcpy4,  %function
	.type	memcpy8,  %function
	.type	memcpy16, %function
	.type	memcpy32, %function
	.type	memcmp,   %function
	.type	memcmp4,  %function
memcpy:
/* 	ands   r3, r0, #3  /* if r0 is aligned */
/* 	andeqs r3, r1, #3  /* if r0 && r1 are aligned */
/*	bne .memcpy1 /* have to go slow if we are not aligned */

memcpy32:
	subs r2, r2, #32
	ldmplia r1!, {r3-r10}
	stmplia r0!, {r3-r10}
	bne memcpy32
	mov pc, lr
	.size   memcmp32, .-memcmp32

memcpy16:
	subs r2, r2, #16
	ldmplia r1!, {r3-r6}
	stmplia r0!, {r3-r6}
	bne memcpy16
	mov pc, lr
	.size   memcmp16, .-memcmp16

memcpy8:
	subs r2, r2, #8
	ldmplia r1!, {r3-r4}
	stmplia r0!, {r3-r4}
	bne memcpy8
	mov pc, lr
	.size   memcmp8, .-memcmp8

memcpy4:
	subs r2, r2, #4
	ldmplia r1!, {r3}
	stmplia r0!, {r3}
	bne memcpy4
	mov pc, lr
	.size   memcmp4, .-memcmp4

memcpy1: /* the really slow way */
	ldrb r3, [r1]
	add  r1, r1, #1
	subs  r2, r2, #1 /* code motion: optimize pipeline stalling */
	strb r3, [r0]
	add  r0, r0, #1
	bpl memcpy1
	mov  pc, lr
	.size	memcpy1, .-memcpy1

memcmp:
	ldrb r3, [r0]
	ldrb r4, [r1]
	add r0, r0, #1
	add r1, r1, #1

	cmp r3, r4
	movmi r0, #-1
	movmi pc, lr
	movne r0, #1
	movne pc, lr

	subs r2, r2, #1
	bne memcmp
	mov r0, #0
	mov pc, lr
	.size   memcmp, .-memcmp

memcmp4:
	ldmia r0!, {r3}
	ldmia r1!, {r4}

	cmp r3, r4
	movmi r0, #-1
	movmi pc, lr
	movne r0, #1
	movne pc, lr

	subs r2, r2, #4
	bne memcmp4
	mov r0, #0
	mov pc, lr
	.size   memcmp4, .-memcmp4
