#include <flash/nor/dwcssi/dwcssi.h>


const flash_ops_t gd_gd25b512_ops = 
{
    .clk_div = 8,
    .wait_cycle = 8,

    .qread_cmd = 0x6C,
    .rd_trans_type = TRANS_TYPE_TT0,
    .qprog_cmd = 0x34,
    .wr_trans_type = TRANS_TYPE_TT0,
    .quad_rd_config = general_spi_quad_rd_config,
    .quad_en = general_spi_quad_en,
    .quad_dis = general_spi_quad_dis,

};