#
# AL-Link JTAG Scan Script
#
# Usage:
#   openocd -f al_link_scan.tcl -c "al_scan"
#   openocd -f al_link_scan.tcl -c "al_scan 2000"
#   openocd -f al_link_scan.tcl -c {scan_device 0x336C 0x1001 "1-3" 5000}
#

proc scan_device {vid pid {port ""} {speed 10000}} {
    adapter driver al_link
    if {$port ne ""} {
        puts "scan port $port"
        adapter usb location $port
    }
    adapter speed $speed
    al_link_vid_pid $vid $pid
    transport select jtag
    jtag init
    shutdown
}

proc al_scan {{speed 2000} {port ""}} {
    scan_device 0x336C 0x1001 $port $speed
}
