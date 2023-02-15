#include <flash\nor\dwcssi\dwcssi.h>
#define GD_CMD_READ_STATUS_BYTE1      0x35
#define GD_CMD_WRITE_STATUS_BYTE1     0x31

#define GD_CMD_ENABLE_QPI             0x38
#define GD_CMD_DISABLE_QPI            0xFF

int gigadev_gd25q_reset(struct flash_bank *bank)
{
    return ERROR_OK;
}

int gigadev_gd25q_err_chk(struct flash_bank *bank)
{
    return ERROR_OK;
}

int gigadev_gd25q_quad_en(struct flash_bank *bank)
{
    uint32_t sr_byte1 = 0;
    dwcssi_rd_flash_reg(bank, &sr_byte1, GD_CMD_READ_STATUS_BYTE1, 1);
    dwcssi_wr_flash_config_reg(bank, GD_CMD_WRITE_STATUS_BYTE1, sr_byte1 | 0x2);

    dwcssi_flash_tx_cmd(bank, 0, GD_CMD_ENABLE_QPI);
    return ERROR_OK;
}

int gigadev_gd25q_quad_dis(struct flash_bank *bank)
{

    dwcssi_flash_tx_cmd(bank, 0, GD_CMD_DISABLE_QPI);
    return ERROR_OK;
}

const flash_ops_t gigadev_gd25q_ops = {
    .qread_cmd = 0xeb,
    .qprog_cmd = 0x32,
    .reset     = gigadev_gd25q_reset,
    .err_chk   = gigadev_gd25q_err_chk,
    .quad_en   = gigadev_gd25q_quad_en,
    .quad_dis  = gigadev_gd25q_quad_dis
};
