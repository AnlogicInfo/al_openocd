
# define REGBYTES 16

		.section .text.entry
		.global _start
_start:
		ldr x28, =stack_end
        mov sp, x28
		bl    	emmc_dwcmshc_async

        hlt #0x0B
		.section .data
		.balign REGBYTES
stack:
		.fill	16, REGBYTES, 0x08675309
stack_end:
