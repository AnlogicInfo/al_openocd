#include <flash/nor/dwcssi/dwcssi.h>
// SR
//|BIT 7 | BIT 6| BIT 5| BIT 4| BIT 3 | BIT 2| BIT 1| BIT 0|
// SRWD    QE     BP3    BP2    BP1     BP1   WEL    WIP 


void issi_quad_rd_config(struct flash_bank *bank, uint8_t addr_len)
{
    struct dwcssi_trans_config trans_config;

    trans_config.tmod = RX_ONLY;
    trans_config.spi_frf = SPI_FRF_X4_MODE;
    trans_config.ndf = 0;

    trans_config.rx_ip_lv = 0x3F;
    trans_config.wait_cycle = 6;

    trans_config.trans_type = TRANS_TYPE_TT1;
    trans_config.stretch_en = ENABLE;
    trans_config.addr_len = addr_len; //add mode bits into addr
    // LOG_INFO("issi rd config addr len %d", addr_len);
    dwcssi_config_trans(bank, &trans_config);
}

int issi_quad_en(struct flash_bank *bank)
{
    uint8_t sr[2], wr_seq[2] = {SPIFLASH_WRITE_STATUS, 0};
    uint8_t quad_en;

    dwcssi_rd_flash_reg(bank, sr, SPIFLASH_READ_STATUS, 1);
    wr_seq[1] = sr[0] | (1<<6);
    dwcssi_wr_flash_reg(bank, wr_seq, 2, STANDARD_SPI_MODE);
    dwcssi_rd_flash_reg(bank, sr, SPIFLASH_READ_STATUS, 1);

    // LOG_ERROR("issi quad en sr %x", sr[0]);
    quad_en = (sr[0]>>6) & 0x1;
    if(quad_en != 1)
    {
        return ERROR_FAIL;
    }

    return ERROR_OK;
}


int issi_quad_dis(struct flash_bank *bank)
{
    uint8_t sr[2], wr_seq[2] = {SPIFLASH_WRITE_STATUS, 0};
    uint8_t quad_en;

    dwcssi_rd_flash_reg(bank, sr, SPIFLASH_READ_STATUS, 1);
    wr_seq[1] = sr[0] & (~(1<<6));
    dwcssi_wr_flash_reg(bank, wr_seq, 2, STANDARD_SPI_MODE);
    dwcssi_rd_flash_reg(bank, sr, SPIFLASH_READ_STATUS, 1);

    // LOG_INFO("issi quad dis sr %x", sr[0]);
    quad_en = (sr[0]>>6) & 0x1;
    if(quad_en != 0)
    {
        return ERROR_FAIL;
    }

    return ERROR_OK;
}

const flash_ops_t issi_ops = {
    .clk_div = 4,
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
    .quad_rd_config = issi_quad_rd_config,
    .quad_en = issi_quad_en,
    .quad_dis = issi_quad_dis
};