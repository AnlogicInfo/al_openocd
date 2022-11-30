# Defines basic Tcl procs for OpenOCD flash module

#
# programs utility proc
# usage: programs filename num
# optional args: verify, reset, exit and address
#

lappend _telnet_autocomplete_skip programs_error
proc programs_error {description exit} {
	if {$exit == 1} {
		echo $description
		shutdown error
	}

	error $description
}

proc programs {filename num args} {
	set exit 0
	set needsflash 1
    set address 0

	foreach arg $args {
		if {[string equal $arg "preverify"]} {
			set preverify 1
		} elseif {[string equal $arg "verify"]} {
			set verify 1
		} elseif {[string equal $arg "reset"]} {
			set reset 1
		} elseif {[string equal $arg "exit"]} {
			set exit 1
		} else {
			set address $arg
		}
	}

	# Set variables
	set filename \{$filename\}
	set flash_args "$filename $address"


	# make sure init is called
	if {[catch {init}] != 0} {
		programs_error "** OpenOCD init failed **" 1
	}

	# reset target and call any init scripts
	if {[catch {reset init}] != 0} {
		programs_error "** Unable to reset target **" $exit
	}

    # nand probe
    if {[catch {eval nand probe $num}] != 0} {
		programs_error "** Nand probe Failed **" $exit
    }

	# Check whether programming is needed
	if {[info exists preverify]} {
		echo "**pre-verifying**"
		if {[catch {eval nand verify $num $flash_args}] == 0} {
			echo "**Verified OK - No flashing**"
			set needsflash 0
		}
	}

	# start programming phase
	if {$needsflash == 1} {
		echo "** Programming Started **"

		if {[catch {eval nand write_image $num $flash_args erase}] == 0} {
			echo "** Programming Finished **"
			if {[info exists verify]} {
				# verify phase
				echo "** Verify Started **"
				if {[catch {eval nand verify $num $flash_args}] == 0} {
					echo "** Verified OK **"
				} else {
					programs_error "** Verify Failed **" $exit
				}
			}
		} else {
			programs_error "** Programming Failed **" $exit
		}
	}

	if {[info exists reset]} {
		# reset target if requested
		if {$exit == 1} {
			# also disable target polling, we are shutting down anyway
			poll off
		}
		echo "** Resetting Target **"
		reset run
	}


	if {$exit == 1} {
		shutdown
	}
	return
}

add_help_text programs "write an image to nand, nand number and filename is required for this command. verify, reset, exit are optional"
add_usage_text programs "<filename> \[address\] \[pre-verify\] \[verify\] \[reset\] \[exit\]"