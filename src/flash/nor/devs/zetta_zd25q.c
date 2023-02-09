#include <flash\nor\dwcssi\dwcssi.h>
#define ZETTA_READ_STATUS_BYTE0            0x05
#define ZETTA_READ_STATUS_BYTE1            0x35
#define ZETTA_ENABLE_QPI                   0x38
#define ZETTA_DISABLE_QPI                  0xFF

#define ZETTA_EP_FAIL(x)                   ((x>>2) & 0x1)
int zetta_zd25q_reset(struct flash_bank *bank)
{
    return ERROR_OK;
}

int zetta_zd25q_err_chk(struct flash_bank *bank)
{
    uint32_t sr;

    dwcssi_read_flash_reg(bank, &sr, ZETTA_READ_STATUS_BYTE1, 1);
    if(ZETTA_EP_FAIL(sr))
        return ERROR_FAIL;
    else
        return ERROR_OK;
}

int zetta_zd25q_quad_en(struct flash_bank *bank)
{
    dwcssi_flash_tx_cmd(bank, ZETTA_ENABLE_QPI);
    return ERROR_OK;
}

int zetta_zd25q_quad_dis(struct flash_bank *bank)
{
    dwcssi_flash_tx_cmd(bank, ZETTA_DISABLE_QPI);
    return ERROR_OK;
}


const flash_ops_t zetta_zd25q_ops = {
    .qread_cmd = 0x6B,
    .qprog_cmd = 0X32,
    .reset     = zetta_zd25q_reset,
    .err_chk   = zetta_zd25q_err_chk,
    .quad_en   = zetta_zd25q_quad_en,
    .quad_dis  = zetta_zd25q_quad_dis
};
