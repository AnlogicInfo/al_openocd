#include <flash/nor/dwcssi/dwcssi.h>
// SR
//|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
// BUSY    RES    SEC    WPLD   WSP     WSE    WEL    BUSY
// CR
//|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
// WPEN    RES    RES    RES    BPNV    RES    IOC    RES
const flash_ops_t mc_ops = {
    .clk_div = 8,
    .wait_cycle  = 8,

    .qe_index = 9,
    .rdsr1_cmd = 0x05,
    .rdsr2_cmd = 0x35,
    .rdsr1n2_cmd = 0,

    .wrsr1_cmd = 0,
    .wrsr2_cmd = 0,
    .wrsr1n2_cmd = 0x01,
    .qread_cmd = 0x6B,
    .rd_trans_type = TRANS_TYPE_TT0,
    .qprog_cmd = 0x32,
    .wr_trans_type = TRANS_TYPE_TT1,
};