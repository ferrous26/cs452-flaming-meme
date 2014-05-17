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
	ldrb r3, [r1]
	add  r1, r1, #1
	strb r3, [r0]
	add  r0, r0, #1
	subs  r2, r2, #1
	bpl .loop
	mov  pc, lr
	.size	memcpy, .-memcpy
