.align 3
.global uart0_init_simple
uart0_init_simple:
		/* save lr register */	
		mov  x10, x30

		/* pin mux */
		ldr x11, =0xf8803068
		mov w12, #0x3
		str w12, [x11]
		ldr x11, =0xf880306c
		mov w12, #0x3
		str w12, [x11]
		ldr x11, =0xf8803410
		mov w12, #0x1
		str w12, [x11]
		/* uart init */
		ldr x11, =0xf840000c
		mov w12, #0x83
		str w12, [x11]
		dsb sy
		ldr x11, =0xf8400000
		mov w12, #0x16
		str w12, [x11]
		dsb sy
		ldr x11, =0xf840000c
		mov w12, #0x03
		str w12, [x11]
		dsb sy
		ldr x11, =0xf8400008
		mov w12, #0x07
		str w12, [x11]
		dsb sy


		/* return */
		mov x30, x10
		ret
