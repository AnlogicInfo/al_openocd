init
source [find mem_helper.tcl]

proc unlock {} {
    set boot_mode [mrw 0xf8800200]
    set rst_reg   [mrw 0xf8806330]
    set rst_val [expr {$rst_reg & 0xfffe}]

    if {$boot_mode != 0x0} {
        mww 0xf8800200 0x0
        mww 0xf8806330 $rst_val

        mdw 0xf8800200
        mdw 0xf8806330
    }
}

unlock
shutdown
