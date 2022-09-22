.section ".text.boot"
.global	_start

# define LREG lw
# define SREG sw
# define REGBYTES 4

_start:
		ldr x28, =stack_end
        mov sp, x28
		b    	flash_dwcssi
        hlt #0xF00


		.section .data
		.balign REGBYTES
stack:
		.fill	16, REGBYTES, 0x8675309
stack_end: