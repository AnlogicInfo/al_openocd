proc scan_device {port pid vid} {
  puts "scan port $port"
  adapter driver ftdi
  adapter usb location $port
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
