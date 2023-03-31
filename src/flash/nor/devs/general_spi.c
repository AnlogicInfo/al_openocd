#include <flash/nor/dwcssi/dwcssi.h>

#define      SPI_RD_SR_BYTE0       0x05
#define      SPI_RD_SR_BYTE1       0x35
#define      SPI_RD_SR_BYTE2       0x15

#define      SPI_WR_SR_BYTE0       0x01
#define      SPI_WR_SR_BYTE1       0x31
#define      SPI_WR_SR_BYTE2       0x11

void general_spi_quad_rd_config(struct flash_bank *bank, uint8_t addr_len)
{
    struct dwcssi_trans_config trans_config;
    LOG_INFO("general config trans");
    trans_config.tmod = RX_ONLY;
    trans_config.spi_frf = SPI_FRF_X4_MODE;
    trans_config.ndf = 0;

    trans_config.rx_ip_lv = 0x3F;
    trans_config.wait_cycle = 8;

    trans_config.trans_type = TRANS_TYPE_TT0;
    trans_config.stretch_en = ENABLE;
    trans_config.addr_len = addr_len;
    dwcssi_config_trans(bank, &trans_config);
}


int general_reset_f0(struct flash_bank *bank, uint8_t cmd_mode)
{
    uint8_t flash_reset[] = {0xF0};
    LOG_INFO("flash reset F0");
    dwcssi_flash_tx_cmd(bank, flash_reset, 1, cmd_mode);
    return ERROR_OK;
}

int general_reset_66_99(struct flash_bank *bank, uint8_t cmd_mode)
{
    uint8_t flash_reset[] = {0x66, 0x99};

    LOG_INFO("flash reset 66 99");
    dwcssi_flash_tx_cmd(bank, &flash_reset[0], 1, cmd_mode);
    dwcssi_flash_tx_cmd(bank, &flash_reset[1], 1, cmd_mode);

    return ERROR_OK;
}

int general_spi_reset(struct flash_bank *bank, uint8_t cmd_mode)
{
    return ERROR_OK;
}

int general_spi_err_chk(struct flash_bank *bank)
{
    return ERROR_OK;
}


int general_spi_quad_en(struct flash_bank *bank)
{
    uint8_t flash_cr = 0;
    uint8_t sr_wr_seq[2] = {SPI_WR_SR_BYTE1, 0};
    dwcssi_rd_flash_reg(bank, &flash_cr, SPI_RD_SR_BYTE1, 1);
    sr_wr_seq[1] = flash_cr | 0x2;
    dwcssi_wr_flash_reg(bank, sr_wr_seq, 2, STANDARD_SPI_MODE);

    return ERROR_OK;
}

int general_spi_quad_dis(struct flash_bank* bank)
{
    uint8_t flash_cr = 0;
    uint8_t sr_wr_seq[2] = {SPI_WR_SR_BYTE1, 0};
    dwcssi_rd_flash_reg(bank, &flash_cr, SPI_RD_SR_BYTE1, 1);
    sr_wr_seq[1] =  flash_cr & (0x3C);
    dwcssi_wr_flash_reg(bank, sr_wr_seq, 2, STANDARD_SPI_MODE);
    dwcssi_rd_flash_reg(bank, &flash_cr, SPI_RD_SR_BYTE1, 1);
    LOG_INFO("general quad dis sr %x", flash_cr);
    return ERROR_OK;
}

int general_spi_qpi_en(struct flash_bank* bank)
{
    return ERROR_OK;
}

int general_spi_qpi_dis(struct flash_bank* bank)
{
    return ERROR_OK;
}
const flash_ops_t general_spi_ops = {
    .clk_div    = 8,

    .qread_cmd = 0x6B,
    .qprog_cmd = 0x32,
};

