#include <flash/nor/dwcssi/dwcssi.h>
// SR1
//|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
// SRP0    SEC    TB     BP2    BP1     BP0    WEL    BUSY
// SR2
//|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
// SUS     CMP    LB3    LB2    LB1     R      QE     SRP1



const flash_ops_t win_ops = {
    .clk_div = 8,
    .wait_cycle = 8,
    .trans_type = TRANS_TYPE_TT0,

    .qe_index = 9,
    .rdsr1_cmd = 0x05,
    .rdsr2_cmd = 0x35,
    .rdsr1n2_cmd = 0,

    .wrsr1_cmd = 0x01,
    .wrsr2_cmd = 0,
    .wrsr1n2_cmd = 0x01,

    .qread_cmd = 0x6B,
    .qprog_cmd = 0x32,
    .quad_rd_config = general_spi_quad_rd_config
};