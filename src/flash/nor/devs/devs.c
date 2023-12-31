#include <flash/nor/dwcssi/dwcssi.h>

const flash_ops_t gd_gd25b512_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 0x9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0x31,
	.wrsr1n2_cmd = 0,

	.qread_cmd = 0x6C,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x34,
	.wr_trans_type = TRANS_TYPE_TT0,
};


const flash_ops_t gd_gd25q_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 0x9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0x31,
	.wrsr1n2_cmd = 0,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0,
};

/*
SR
|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
SRWD    QE     BP3    BP2    BP1     BP1   WEL    WIP
*/

const flash_ops_t issi_24_ops = {
	.clk_div    = 20,
	.wait_cycle  = 6,

	.qe_index = 6,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0,
	.rdsr1n2_cmd = 0,

	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0,
	.qread_cmd = 0xEB,
	.rd_trans_type = TRANS_TYPE_TT1,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0,
};

const flash_ops_t issi_32_ops = {
	.clk_div    = 20,
	.wait_cycle  = 6,

	.qe_index = 6,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0,
	.rdsr1n2_cmd = 0,

	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0,
	.qread_cmd = 0xEC,
	.rd_trans_type = TRANS_TYPE_TT1,
	.qprog_cmd = 0x34,
	.wr_trans_type = TRANS_TYPE_TT0,
};
/*
 SR
|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
 BUSY    RES    SEC    WPLD   WSP     WSE    WEL    BUSY
 CR
|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
 WPEN    RES    RES    RES    BPNV    RES    IOC    RES
*/

const flash_ops_t mc_ops = {
	.clk_div    = 20,
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
	.unset_protect_cmd = 0x98
};

const flash_ops_t sp_s25fl_24_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0x01,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0,
};

const flash_ops_t sp_s25fl_32_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0x01,

	.qread_cmd = 0x6C,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x34,
	.wr_trans_type = TRANS_TYPE_TT0,
};

/*
 SR1
|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
 SRP0    SEC    TB     BP2    BP1     BP0    WEL    BUSY
 SR2
|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
 SUS     CMP    LB3    LB2    LB1     R      QE     SRP1
*/

const flash_ops_t win_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,

	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0x01,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0
};

/*
 SR1
|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
 SRP0    SEC    TB     BP2    BP1     BP0    WEL    BUSY
 SR2
|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
 SUS     CMP    LB3    LB2    LB1     R      QE     SRP1
*/

const flash_ops_t win_32_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,

	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0x01,

	.qread_cmd = 0x6C,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x34,
	.wr_trans_type = TRANS_TYPE_TT0
};

const flash_ops_t zetta_zd25q_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,

	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0x31,
	.wrsr1n2_cmd = 0x01,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0
};

const flash_ops_t zb_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0x31,
	.wrsr1n2_cmd = 0,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0
};

const flash_ops_t puya_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0x31,
	.wrsr1n2_cmd = 0,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0
};

const flash_ops_t mksv_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0x31,
	.wrsr1n2_cmd = 0,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0
};

/* Nonvolatile Configuration Register */
const flash_ops_t micron_24_ops = {
	.clk_div    = 20,
	.wait_cycle  = 8,

	.qe_index = 0,
	.rdsr1_cmd = 0,
	.rdsr2_cmd = 0,
	.rdsr1n2_cmd = 0,

	.wrsr1_cmd = 0,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0,
};

const flash_ops_t micron_32_ops = {
	.clk_div    = 20,
	.wait_cycle  = 8,

	.qe_index = 0,
	.rdsr1_cmd = 0,
	.rdsr2_cmd = 0,
	.rdsr1n2_cmd = 0,

	.wrsr1_cmd = 0,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0,

	.qread_cmd = 0x6C,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x34,
	.wr_trans_type = TRANS_TYPE_TT0,
};

const flash_ops_t mac_25l_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 6,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x38,
	.wr_trans_type = TRANS_TYPE_TT1
};

const flash_ops_t xtx_25_ops = {
	.clk_div    = 20,
	.wait_cycle = 8,

	.qe_index = 0x9,
	.rdsr1_cmd = 0x05,
	.rdsr2_cmd = 0x35,
	.rdsr1n2_cmd = 0,
	.wrsr1_cmd = 0x01,
	.wrsr2_cmd = 0,
	.wrsr1n2_cmd = 0x01,

	.qread_cmd = 0x6B,
	.rd_trans_type = TRANS_TYPE_TT0,
	.qprog_cmd = 0x32,
	.wr_trans_type = TRANS_TYPE_TT0,
};

