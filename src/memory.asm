	.file	"memory.asm"
	.text
	.align	2
	.global	memcpy
	.type	memcpy, %function
memcpy:
 	ands   r3, r0, #3  /* if r0 is aligned */
 	andeqs r3, r1, #3  /* if r0 && r1 are aligned */
	bne .slowcpy

.fastcpy:
	cmp r2, #16
	bmi .slowcpy
	ldmia r1!, {r3-r6}
	stmia r0!, {r3-r6}
	subs r2, r2, #16
	bne .fastcpy
	mov pc, lr

.slowcpy:
	/* load src and increment pointer */
	ldrb r3, [r1]
	add  r1, r1, #1

	/* try to optimize pipeline stalling */
	subs  r2, r2, #1

	/* store and update pointer */
	strb r3, [r0]
	add  r0, r0, #1

	/* start again if we need to */
	bpl .slowcpy

	mov  pc, lr
	.size	memcpy, .-memcpy
