
# define REGBYTES 16

		.section .text.entry
		.global _start
_start:
		ldr x28, =stack_end
        mov sp, x28
		bl    	flash_dwcssi

        hlt #0x0B
		.section .data
		.balign REGBYTES
stack:
		.fill	16, REGBYTES, 0x8675309
stack_end:
