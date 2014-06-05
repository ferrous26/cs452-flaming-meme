	.file	"memory.asm"
	.text
	.align	2
	.global memcpy
	.type	memcpy,   %function
memcpy:
	/** r0 = dest, r1 = src, r2 = len **/
	/* Back up r0, since we must return it */
	stmfd sp!, {r0, r4-r9}

	/* If already word aligned, skip to accelerated case */
	ands   r3, r0, #3
	andeqs r3, r1, #3
	beq    .fastcpy

	/* If we got here, we want to try and align the addresses */
	sub  r2, r2, r3
.align:
	ldrb r12, [r1], #1
	subs r3, r3, #1   /* set condition flags here */
	strb r12, [r0], #1
	bne .align

	/* check alignment again */
	ands    r3, r0, #3
	andeqs  r3, r1, #3
	bne     .slowcpy /* if still not aligned, then give up */

.fastcpy: /* fall through to the fastcpy case! */
	cmp r2, #4 /* jump right to the word case */
	beq .small

.big: /* multiple of 32? */
	subs r2, r2, #32
	ldmplia r1!, {r3-r9, r12}
	stmplia r0!, {r3-r9, r12}
	beq .done
	bpl .big /* if there is something left, go back to start */
	add r2, r2, #32

/* at this point, we have less than 32 bytes left to copy */
/* so we can make some assumptions to avoid branching */

	subs r2, r2, #16
	ldmplia r1!, {r3-r6}
	stmplia r0!, {r3-r6}
	beq .done
	addmi r2, r2, #16

	subs r2, r2, #8
	ldmplia r1!, {r3, r12}
	stmplia r0!, {r3, r12}
	beq .done
	addmi r2, r2, #8

.small:
	subs r2, r2, #4
	ldmplia r1!, {r3}
	stmplia r0!, {r3}
	beq .done
	addmi r2, r2, #4

.slowcpy:
	subs r2, r2, #1  /* set condition flags here */
	ldrplb r3, [r1], #1
	strplb r3, [r0], #1
	bne .slowcpy

.done:
	ldmfd sp!, {r0, r4-r9}
	mov  pc, lr
	.size	memcpy, .-memcpy


	.global memset
	.type	memset,   %function
memset:
	/** r0 = dest, r1 = new_val, r2 = len **/
	/* Back up r0, since we must return it */
	stmfd sp!, {r0}

	/* If already word aligned, skip to accelerated case */
	ands   r3, r0, #3
	beq    .fastset

	/* If we got here, we want to try and align the addresses */
	sub  r2, r2, r3
.alignset:
	strb r1, [r0], #1
	subs r3, r3, #1   /* set condition flags here */
	bne .alignset

	/* check alignment again */
	ands r3, r0, #3
	bne .slowset /* if still not aligned, then give up */

.fastset:
	/* make a nice register for setting */
	orr r1, r1, r1, LSL #8
	orr r1, r1, r1, LSL #16
	mov r12, r1

.bigset: /* multiple of 8? */
	subs r2, r2, #8
	stmplia r0!, {r1, r12}
	beq .doneset
	bpl .bigset /* if there is something left, go back to start */
	add r2, r2, #8

/* at this point we have less than 8 bytes left */
	subs r2, r2, #4
	stmplia r0!, {r1}
	beq .doneset
	addmi r2, r2, #4

.slowset:
	subs r2, r2, #1   /* set condition flags here */
	strplb r1, [r0], #1
	bne .slowset

.doneset:
	ldmfd sp!, {r0}
	mov pc, lr
	.size   memset, .-memset

/*
	.global memcmp
	.type	memcmp,   %function
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
*/
