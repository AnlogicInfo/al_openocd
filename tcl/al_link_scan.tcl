#
# AL-Link JTAG Scan Script
#
# Usage:
#   openocd -f al_link_scan.tcl -c "al_scan"
#       Use the default 1200 kHz scan speed.
#   openocd -f al_link_scan.tcl -c "al_scan 500"
#       Use the specified scan speed in kHz.
#   openocd -f al_link_scan.tcl -c {scan_device 0x336C 0x1001 "1-3" 500}
#       Scan a specific USB location with an explicit speed.
#

proc scan_device {vid pid {port ""} {speed ""}} {
    adapter driver al_link
    if {$port ne ""} {
        puts "scan port $port"
        adapter usb location $port
    }
    if {$speed ne ""} {
        adapter speed $speed
    } else {
        adapter speed 1200
    }
    al_link_vid_pid $vid $pid
    transport select jtag
    jtag init
    shutdown
}

proc al_scan {{speed ""} {port ""}} {
    scan_device 0x336C 0x1001 $port $speed
}
