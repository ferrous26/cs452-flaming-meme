	.file	"memory.asm"
	.text
	.align	2
	.global	memcpy
	.type	memcpy, %function
memcpy: /* r0 = dst, r1 = src, r2 = len, r3 = scratch
	/* void memcpy(void* dst, const void* src, size len) {
	       while (len--)
                   *dst++ = *src++;
	   }
	*/
.loop:
	/* load src and increment pointer */
	ldrb r3, [r1]
	add  r1, r1, #1

	/* try to optimize pipeline stalling */
	subs  r2, r2, #1

	/* store and update pointer */
	strb r3, [r0]
	add  r0, r0, #1

	/* start again if we need to */
	bpl .loop

	mov  pc, lr
	.size	memcpy, .-memcpy
