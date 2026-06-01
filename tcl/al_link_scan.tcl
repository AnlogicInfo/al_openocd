#
# AL_Link JTAG Scan Script
#
# Usage:
#   openocd -f al_link_scan.tcl
#   openocd -f al_link_scan.tcl -c "al_scan 2000"     # specify speed
#

proc al_scan {{speed 2000}} {
    adapter driver al_link
    al_link_vid_pid 0x336C 0x1001
    adapter speed $speed
    transport select jtag
    
    # Create a dummy TAP with long IR to auto-detect chain
    # OpenOCD will report the actual IDCODE values found
    jtag newtap auto0 tap -irlen 8 -expected-id 0
    
    puts "=========================================="
    puts "AL-LINK JTAG Scan"
    puts "Speed: $speed kHz"
    puts "=========================================="
    
    init
    
    puts ""
    puts "JTAG chain detection complete."
    puts "Check above for IDCODE values."
    puts "=========================================="
    
    shutdown
}

# Default action: run scan at 2MHz
al_scan
