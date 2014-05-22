	.file	"kernel.asm"
	.text
	.align	2
	.global	exit_to_redboot
	.type	exit_to_redboot, %function
exit_to_redboot:
	mov	pc, r0
	.size	exit_to_redboot, .-exit_to_redboot
