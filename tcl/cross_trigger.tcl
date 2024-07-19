proc input_trigger {TARGET CORE TRIGGER CHANNEL} {
  set TRIGINEN [expr {1<<$CHANNEL}]
  puts "Command: $TARGET.cti.$CORE write INEN$TRIGGER $TRIGINEN"
  $TARGET.cti.$CORE write INEN$TRIGGER $TRIGINEN
}

proc output_trigger {TARGET CORE TRIGGER CHANNEL} {
  set TRIGOUTEN [expr {1<<$CHANNEL}]
  set TRIGRD [eval $TARGET.cti.$CORE read OUTEN$TRIGGER]
  set TRIGWR [expr {$TRIGOUTEN | $TRIGRD}]
  puts "Command: $TARGET.cti.$CORE write OUTEN$TRIGGER $TRIGWR"
  $TARGET.cti.$CORE write OUTEN$TRIGGER $TRIGWR
}

proc enable_channel {TARGET CORE CHANNEL EN} {
    if {$EN eq "on"} {
      $TARGET.cti.$CORE channel $CHANNEL ungate
      puts "Command: $TARGET.cti.$CORE channel $CHANNEL ungate"
    } elseif {$EN eq "off"} {
      $TARGET.cti.$CORE channel $CHANNEL gate
      puts "Command: $TARGET.cti.$CORE channel $CHANNEL gate"
    }
}

proc config_cti {en filename} {
  
  if {[catch {open $filename r} fp]} {
    puts "Error: Unable to open file $filename"
  } else {
    puts "File opened successfully"
    # Proceed with operations on the file
  }
  set content [read $fp]
  close $fp
  puts "Read CTI config"
  set configList [split $content "\n"]
  foreach config $configList {
      set configSubList [split $config " "]
      set direction [lindex $configSubList 0]
      set target [lindex $configSubList 1]
      set core [lindex $configSubList 2]
      set trigger [lindex $configSubList 3]
      set channel [lindex $configSubList 4]
      if {$direction eq "input"} {
          input_trigger $target $core $trigger $channel
      } elseif {$direction eq "output"} {
          output_trigger $target $core $trigger $channel
      } else {
          break
      }
      enable_channel $target $core $channel $en
  }
}

