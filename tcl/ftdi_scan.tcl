proc scan_device {pid vid {port ""}} {
    adapter driver ftdi
    if {$port ne ""} {
      puts "scan port $port"
      adapter usb location $port
    }
    adapter speed    10000
    ftdi vid_pid $pid $vid
    ftdi layout_init 0x0008 0x001b
    transport select jtag
    jtag init
    shutdown
}

proc usb_list {pid vid} {
  adapter driver ftdi
  set retstr [ftdi list $pid $vid]
  puts $retstr
  shutdown
}
