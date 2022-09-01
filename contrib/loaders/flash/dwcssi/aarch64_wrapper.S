#define REGBYTES 8
		.section .text.entry
		.global _start
_start:
		ldr x28, =stack_end
        mov sp, x28
		b    	flash_dwcssi
        bkpt	#0

		.section .data
		.balign REGBYTES
stack:
		.fill	16, REGBYTES, 0x8675309
stack_end:
