	.file	"memory.asm"
	.section ".text.kern"

	.align	2
	.global memcpy
	.type	memcpy,   %function
memcpy:
	/** r0 = dest, r1 = src, r2 = len, r3,r12 = free scratch **/
	/** r4-9 = expensive scratch (we shrink wrap them) **/

	/* If already word aligned, skip to accelerated case */
	ands   r3, r0, #3
	andeqs r3, r1, #3
	beq    .fastcpy

	/* Otherwise, we want to try and align the addresses    */
	/* but first we need to figure out how much to align.   */
	/* It is possible we cannot align the addresses, so we  */
	/* do not try very hard. We try and get the src address */
	/* aligned because that value is already in r3.         */
	mov   ip, #4
	sub   r3, ip, r3
	cmp   r2, r3      /* we want to try copying MIN(r2, r3) bytes */
	movmi r3, r2

	/* if r2 was smaller then we need to zero it */
	movmi r2, #0
	/* else calculate how much more copying after alignment */
	subpl r2, r2, r3

.slowcpy:
	ldrb ip, [r1], #1
	subs r3, r3, #1   /* set condition flags here */
	strb ip, [r0], #1
	bne .slowcpy      /* use condition flags here */

	/* if we are done, then we can leave */
	cmp   r2, #0
	moveq pc, lr

	/* otherwise, check alignment again */
	ands    r3, r0, #3
	andeqs  r3, r1, #3
	/* if still not aligned, then give up */
	bne     .buttcpy

.fastcpy:
	/* if we are moving less than 2 words, then skip to the small case */
	cmp r2, #9
	bmi .small

	/* otherwise, begin shrink wrapped section */
	stmfd sp!, {r4-r9}

.big:
	/* multiple of 32? go go go */
	subs r2, r2, #32
	ldmplia r1!, {r3-r9, ip}
	stmplia r0!, {r3-r9, ip}

	/* if we are done, then restore the registers and return */
	ldmeqfd sp!, {r4-r9}
	moveq pc, lr

	/* otherwise, we need if there is something left, go back to start */
	bpl   .big
	addmi r2, r2, #32

	/* at this point, we have less than 32 bytes left to copy       */
	/* so we can make some assumptions to avoid branch instructions */

	subs r2, r2, #16
	ldmplia r1!, {r3-r6}
	stmplia r0!, {r3-r6}

	addmi r2, r2, #16
	ldmfd sp!, {r4-r9}
	moveq pc, lr

.small:
	subs r2, r2, #8
	ldmplia r1!, {r3, ip}
	stmplia r0!, {r3, ip}

	/* if done, then return */
	moveq pc, lr

	/* else, prepare for the one word case */
	addmi r2, r2, #8
	subs  r2, r2, #4

	ldrpl r3, [r1], #4
	addmi r2,  r2,  #4  /* use the ldr delay slot */
	strpl r3, [r0], #4

	/* if we are done, then return */
	moveq pc, lr

	/* otherwise, copy the butt of the buffer */
.buttcpy:
	ldrb r3, [r1], #1
	subs r2,  r2,  #1  /* set condition flags here */
	strb r3, [r0], #1
	bne .buttcpy       /* use condition flags here */
	mov   pc, lr
	.size	memcpy, .-memcpy


	.section ".text"
	.global memset
	.type	memset,   %function
memset:
	/** r0 = dest, r1 = new_val, r2 = len **/
	/* If already word aligned, skip to accelerated case */
	ands   r3, r0, #3
	beq    .fastset

	mov   ip, #4
	sub   r3, ip, r3
	cmp   r2, r3
	movmi r3, r2
	movmi r2, #0
	subpl r2, r2, r3

.alignset:
	strb r1, [r0], #1
	subs r3, r3, #1   /* set condition flags here */
	bne .alignset

	/* check alignment again */
	ands r3, r0, #3
	bne  .slowset /* if still not aligned, then give up */

.fastset:
	/* make a nice register for setting */
	orr r1, r1, r1, LSL #8
	orr r1, r1, r1, LSL #16
	mov r12, r1

.bigset: /* multiple of 8? */
	subs    r2, r2, #8
	stmplia r0!, {r1, r12}
	moveq   pc, lr
	bpl .bigset /* if there is something left, go back to start */

	/* at this point we have less than 8 bytes left */
	addmi r2, r2, #8
	subs  r2, r2, #4

	str   r1, [r0], #4
	addmi r2, r2, #4
	moveq pc, lr
	/* fall through to the slow case for the butt of the buffer */

.slowset:
	subs   r2,  r2,  #1   /* set condition flags here */
	strplb r1, [r0], #1
	bne    .slowset
	mov    pc, lr
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
