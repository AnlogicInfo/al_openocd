#include <flash\nor\dwcssi\dwcssi.h>
// s25f1256s flash defines
#define   FLASH_RD_CONFIG_REG_CMD              0x35
#define   FLASH_WR_CONFIG_REG_CMD              0x01

typedef union sp_s25fl_sr1_t
{
    uint8_t reg_val;
    struct
    {
        uint8_t WIP   : 1;
        uint8_t WEL   : 1;
        uint8_t BP0   : 1;
        uint8_t BP1   : 1;
        uint8_t BP2   : 1;
        uint8_t E_ERR : 1;
        uint8_t P_ERR : 1;
        uint8_t SRWD  : 1;
    } reg_fields;
} sp_s25fl_sr1_t;

typedef union sp_s25fl_cr1_t
{
    uint8_t reg_val;
    struct {
        uint8_t FREEZE : 1;
        uint8_t QUAD   : 1;
        uint8_t TBPARM : 1;
        uint8_t BPNV   : 1;
        uint8_t RFU    : 1;
        uint8_t TBPROT : 1;
        uint8_t LC0    : 1;
        uint8_t LC1    : 1;        
    } reg_fields;

} sp_s25fl_cr1_t;


#define   FLASH_STATUS_ERR(x)                  ((x >> 5) & 0x3)
#define   FLASH_STATUS_WP(x)                   ((x >> 2) & 0x7)
#define   FLASH_CONFIG_QUAD(x)                 ((x >> 1) & 0x1)


int sp_s25fl_reset(struct flash_bank *bank)
{
    int flash_err;
    uint8_t flash_sr;
    // uint32_t flash_cr;

    LOG_INFO("reset sp flash");
    flash_err = dwcssi_wait_flash_idle(bank, 100, &flash_sr);
    // dwcssi_rd_flash_reg(bank, &flash_cr, FLASH_RD_CONFIG_REG_CMD, 1);
    if(flash_err != 0)
    {
        // LOG_INFO("Flash Status Error %x", flash_sr);
        dwcssi_config_tx(bank, SPI_FRF_X1_MODE, 1, 0);

        dwcssi_tx(bank, 0xF0);
        if(dwcssi_txwm_wait(bank) != 0)
            return ERROR_TARGET_TIMEOUT;
        dwcssi_wait_flash_idle(bank, 100, &flash_sr);
    }

    return ERROR_OK;
}

int sp_s25fl_err_chk(struct flash_bank* bank)
{
    struct dwcssi_flash_bank* driver_priv = bank->driver_priv;
    uint8_t sr = driver_priv->flash_sr1;

    if(FLASH_STATUS_ERR(sr) != 0)
        return ERROR_FAIL;
    else
        return ERROR_OK;
}

int sp_s25fl_quad_en(struct flash_bank* bank)
{
    uint32_t flash_cr = 0, quad_en;
    uint8_t config_reg[2] = {0};
    dwcssi_rd_flash_reg(bank, &flash_cr, FLASH_RD_CONFIG_REG_CMD, 1);
    quad_en = (flash_cr >> 0x1) & 0x1;

    config_reg[0] = FLASH_WR_CONFIG_REG_CMD;
    config_reg[1] = flash_cr | 0x2;
    
    // LOG_INFO("flash cr %x bit %x", flash_cr, quad_en);
    if(quad_en == 0)
    {
        LOG_DEBUG("dwcssi flash en quad");
        dwcssi_wr_flash_reg(bank, config_reg, 2, STANDARD_SPI_MODE);
    }

    return ERROR_OK;
}

int sp_s25fl_quad_dis(struct flash_bank* bank)
{
    uint32_t flash_cr;
    uint8_t config_reg[2] = {0};

    dwcssi_rd_flash_reg(bank, &flash_cr, FLASH_RD_CONFIG_REG_CMD, 1);

    config_reg[0] = FLASH_WR_CONFIG_REG_CMD;
    config_reg[1] = flash_cr & (0x3C);
    dwcssi_wr_flash_reg(bank, config_reg, 2, STANDARD_SPI_MODE);
    return ERROR_OK;
}

const flash_ops_t sp_s25fl_ops = {
    .qread_cmd = 0x6C,
    .qprog_cmd = 0x34,
    .reset     = sp_s25fl_reset,
    .err_chk   = sp_s25fl_err_chk,
    .quad_en   = sp_s25fl_quad_en,
    .quad_dis  = sp_s25fl_quad_dis
};


