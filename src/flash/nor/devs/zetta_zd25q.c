#include <flash\nor\dwcssi\dwcssi.h>
#define ZETTA_CMD_WRITE_REGISTER               0x01
#define ZETTA_CMD_WRITE_REGISTER_BYTE1         0x31

#define ZETTA_CMD_READ_STATUS_BYTE0            0x05
#define ZETTA_CMD_READ_STATUS_BYTE1            0x35

#define ZETTA_CMD_ENABLE_QPI                   0x38
#define ZETTA_CMD_DISABLE_QPI                  0xFF

#define ZETTA_EP_FAIL(x)                   ((x>>2) & 0x1)
int zetta_zd25q_reset(struct flash_bank *bank)
{
    return ERROR_OK;
}

int zetta_zd25q_err_chk(struct flash_bank *bank)
{
    uint32_t sr;

    dwcssi_rd_flash_reg(bank, &sr, ZETTA_CMD_READ_STATUS_BYTE1, 1);
    if(ZETTA_EP_FAIL(sr))
        return ERROR_FAIL;
    else
        return ERROR_OK;
}

int zetta_zd25q_quad_en(struct flash_bank *bank)
{
    uint32_t sr_byte1 = 0;
    uint8_t quad_en_seq[2] = {ZETTA_CMD_READ_STATUS_BYTE1, 0};

    dwcssi_rd_flash_reg(bank, &sr_byte1, ZETTA_CMD_READ_STATUS_BYTE1, 1);
    
    quad_en_seq[1] = sr_byte1 | 0x2;
    dwcssi_wr_flash_reg(bank, quad_en_seq, 2, STANDARD_SPI_MODE);
    return ERROR_OK;
}

int zetta_zd25q_quad_dis(struct flash_bank *bank)
{
    uint32_t sr_byte1 = 0;
    uint8_t quad_dis_seq[2] = {ZETTA_CMD_READ_STATUS_BYTE1, 0};

    dwcssi_rd_flash_reg(bank, &sr_byte1, ZETTA_CMD_READ_STATUS_BYTE1, 1);

    quad_dis_seq[1] = sr_byte1 & 0xFD;
    dwcssi_wr_flash_reg(bank, quad_dis_seq, 2, STANDARD_SPI_MODE);
    return ERROR_OK;
}


const flash_ops_t zetta_zd25q_ops = {
    .qread_cmd = 0x6B,
    .qprog_cmd = 0x32,
    .reset     = zetta_zd25q_reset,
    .err_chk   = zetta_zd25q_err_chk,
    .quad_en   = zetta_zd25q_quad_en,
    .quad_dis  = zetta_zd25q_quad_dis
};
