#include <flash/nor/dwcssi/dwcssi.h>
#define GD_CMD_READ_STATUS_BYTE1      0x35
#define GD_CMD_WRITE_STATUS_BYTE1     0x31

#define GD_CMD_ENABLE_QPI             0x38
#define GD_CMD_DISABLE_QPI            0xFF

// int gd_gd25q_err_chk(struct flash_bank *bank)
// {
//     return ERROR_OK;
// }

int gd_gd25q_quad_en(struct flash_bank *bank)
{
    uint8_t sr_byte1 = 0;
    uint8_t quad_en_seq[2] = {GD_CMD_WRITE_STATUS_BYTE1, 0};

    dwcssi_rd_flash_reg(bank, &sr_byte1, GD_CMD_READ_STATUS_BYTE1, 1);
    quad_en_seq[1] = sr_byte1 | 0x2;
    dwcssi_wr_flash_reg(bank, quad_en_seq, 2, STANDARD_SPI_MODE);

    return ERROR_OK;
}

int gd_gd25q_quad_dis(struct flash_bank *bank)
{
    uint8_t sr_byte1 = 0;
    uint8_t quad_dis_seq[2] = {GD_CMD_WRITE_STATUS_BYTE1, 0};

    dwcssi_rd_flash_reg(bank, &sr_byte1, GD_CMD_READ_STATUS_BYTE1, 1);

    quad_dis_seq[1] = sr_byte1 & 0xFD;
    dwcssi_wr_flash_reg(bank, quad_dis_seq, 2, STANDARD_SPI_MODE);
    return ERROR_OK;
}

int gd_gd25q_qpi_en(struct flash_bank *bank)
{
    uint8_t qpi_en_cmd = GD_CMD_ENABLE_QPI;
    general_spi_quad_en(bank);
    dwcssi_flash_tx_cmd(bank, &qpi_en_cmd, 1, STANDARD_SPI_MODE);
    return ERROR_OK;
}

int gd_gd25q_qpi_dis(struct flash_bank* bank)
{
    uint8_t qpi_dis_cmd = GD_CMD_DISABLE_QPI;
    dwcssi_flash_tx_cmd(bank, &qpi_dis_cmd, 1, QPI_MODE);
    return ERROR_OK;
}

const flash_ops_t gd_gd25q_ops = {
    .clk_div   = 8,
    .wait_cycle = 8,

    .qread_cmd = 0x6B,
    .qprog_cmd = 0x32,

    .quad_rd_config = general_spi_quad_rd_config,
    .quad_en      = general_spi_quad_en,
    .quad_dis     = general_spi_quad_dis,

    .qpi_en       = gd_gd25q_qpi_en,
    .qpi_dis      = gd_gd25q_qpi_dis
};
