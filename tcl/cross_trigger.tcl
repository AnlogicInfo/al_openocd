proc input_trigger {TARGET CORE TRIGGER CHANNEL} {
  set TRIGINEN [expr {1<<$CHANNEL}]
  puts "Command: $TARGET.cti.$CORE write INEN$TRIGGER $TRIGINEN"
  $TARGET.cti.$CORE write INEN$TRIGGER $TRIGINEN
}

proc output_trigger {TARGET CORE TRIGGER CHANNEL} {
  set TRIGOUTEN [expr {1<<$CHANNEL}]
  puts "Command: $TARGET.cti.$CORE write OUTEN$TRIGGER $TRIGOUTEN"
  $TARGET.cti.$CORE write OUTEN$TRIGGER $TRIGOUTEN
}

proc enable_channel {TARGET CORE CHANNEL} {
    $TARGET.cti.$CORE channel $CHANNEL ungate
}

proc config_cti {filename} {
    set fp [open $filename r]
    set content [read $fp]
    close $fp

    set configList [split $content "\n"]
    foreach config $configList {
        if {$config ne "\n"} {
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

            enable_channel $target $core $channel            
        }
    }
}

config_cti $::env(IDE_ROOT_PATH)/tools/data/cti_config.txt
