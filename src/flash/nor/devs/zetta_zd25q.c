#include <flash\nor\dwcssi\dwcssi.h>


int zetta_zd25q_reset(struct flash_bank *bank)
{
    return ERROR_OK;
}

int zetta_zd25q_err_chk(struct flash_bank *bank)
{
    return ERROR_OK;
}

int zetta_zd25q_quad_dis(struct flash_bank *bank)
{
    return ERROR_OK;
}

int zetta_zd25q_quad_en(struct flash_bank *bank)
{

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
