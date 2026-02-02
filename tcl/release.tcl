# Release core script for dr1m90
# Usage: release_core <core_num>
# core_num: 0 or 1

# Helper to read register using mdw and capture
proc release_read_reg {addr} {
    # Use mdw (Memory Display Word) and capture output
    # Output format: 0x12345678: 00000000
    if {[catch {capture "mdw $addr"} output]} {
        echo "Error: Failed to execute mdw: $output"
        return 0
    }
    
    # Extract the value using regex
    # Match pattern: address: value
    # Handle both 0xPREFIX and plain hex
    if {[regexp {:\s+([0-9a-fA-F]+)} $output match value]} {
        return "0x$value"
    }
    
    # Fallback/Error
    echo "Error: Failed to parse mdw output: $output"
    return 0
}

proc release_core {core_num} {
    global _CHIPNAME
    
    # Set default chipname if not defined
    if {![info exists _CHIPNAME]} {
        set _CHIPNAME "dr1m90"
    }
    
    set target_name "$_CHIPNAME.pstap"
    
    # Verify target exists
    if {[lsearch [target names] $target_name] == -1} {
        echo "Error: Target $target_name not found. Please ensure configuration is loaded."
        return
    }
    
    # Switch to pstap target
    echo "Switching to target: $target_name"
    targets $target_name
    
    set reg_addr 0xf8806334
    set bit_mask 0
    
    if {$core_num == 0} {
        set bit_mask 0x10
    } elseif {$core_num == 1} {
        set bit_mask 0x20
    } else {
        echo "Error: Invalid core_num $core_num. Supported values: 0, 1."
        return
    }
    
    # Read-Modify-Write to preserve other core's state
    # We use our custom reader because mem2array/read_memory might be broken/missing
    set current_val [release_read_reg $reg_addr]
    
    set new_val [expr {$current_val | $bit_mask}]
    
    echo "Releasing core $core_num..."
    echo "Register [format 0x%08x $reg_addr]: [format 0x%08x $current_val] -> [format 0x%08x $new_val]"
    
    mww $reg_addr $new_val
}
