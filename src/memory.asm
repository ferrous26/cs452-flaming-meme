	.file	"memory.asm"
	.text
	.align	2
	.global memcpy, memcmp
	.type	memcpy,   %function
	.type	memcmp,   %function

memcpy:
	/** r0 = dest, r1 = src, r2 = len **/

	/* Back up r0, since we must return it */
	stmfd sp!, {r0, r4-r10}

	/* If already word aligned, skip to accelerated case */
	ands   r3, r0, #3
	andeqs r3, r1, #3
	beq    .fastcpy

	/* If we got here, we want to try and align the addresses */
  sub  r2, r2, r3
.align:
	ldrb r4, [r1]
	add  r1, r1, #1
	subs r3, r3, #1   /* set condition flags here */
	strb r4, [r0]
	add  r0, r0, #1
	bne .align

	/* check alignment again */
	ands    r3, r0, #3
	andeqs  r3, r1, #3
	bne     .slowcpy

/* fall through to the fastcpy case! */

.fastcpy:

/* more than 32? */
.big:
  subs r2, r2, #32
  ldmplia r1!, {r3-r10}
  stmplia r0!, {r3-r10}
  beq .done
  bpl .big /* if there is something left, go back to start */

  add r2, r2, #32

.medium:
  subs r2, r2, #8
  ldmplia r1!, {r3-r4}
  stmplia r0!, {r3-r4}
  beq .done
  bpl .medium /* if there is something left, go back to start */

  add r2, r2, #8

.small:
  subs r2, r2, #4
  ldmplia r1!, {r3}
  stmplia r0!, {r3}
  beq .done
  bpl .small /* if there is something left, go back to start */

  add r2, r2, #4


.slowcpy:
	ldrb r3, [r1]
	add r1, r1, #1
	subs r2, r2, #1
	strb r3, [r0]
	add r0, r0, #1
	bne .slowcpy

.done:
	ldmfd sp!, {r0, r4-r10}
	mov  pc, lr

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
