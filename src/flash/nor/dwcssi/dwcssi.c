/*
 * Author: Tianyi Wang (tywang@anlogic.com)
 * Date:  2022-04-20
 * Modified By: Tianyi Wang (tywang@anlogic.com>)
 * Last Modified: 2022-04-27
 */
/*
The AL9000 QSPI controller is based on DWC_ssi 1.02a
*/
#include "dwcssi.h"
#include "dwcssi_flash.h"
static const struct dwcssi_target target_devices[] = {
     // name, tap_idcode, ctrl_base 
    { "AL9000 RPU", 0x1c900a6d, 0xF804E000 },
    { "AL9000 APU", 0x5ba00477, 0xF804E000 },
    { NULL, 0, 0 }
};

static int (*reset_methods[3])(struct flash_bank*, uint8_t cmd_mode) = {
    NULL,
    general_reset_f0,
    general_reset_66_99
};

FLASH_BANK_COMMAND_HANDLER(dwcssi_flash_bank_command)
{
    struct dwcssi_flash_bank *driver_priv;
    target_addr_t base;
    LOG_DEBUG("%s", __func__);

    if (CMD_ARGC < 6)
        return ERROR_COMMAND_SYNTAX_ERROR;

    driver_priv = malloc(sizeof(struct dwcssi_flash_bank));
    if(!driver_priv) {
        LOG_ERROR("not enough memory");
        return ERROR_FAIL;
    }

    if (CMD_ARGC >= 7) {
        COMMAND_PARSE_ADDRESS(CMD_ARGV[6], base);
        LOG_DEBUG("ASSUMING DWCSSI device at ctrl_base = " TARGET_ADDR_FMT,
                base);
    }

    bank->driver_priv = driver_priv;

    driver_priv->probed = false;
    driver_priv->loader.dev_info = (struct dwcssi_flash_bank *) driver_priv;
    driver_priv->ctrl_base = base;
    driver_priv->loader.set_params_priv = NULL;
    driver_priv->loader.exec_target = bank->target;
    driver_priv->loader.copy_area  = NULL;
    driver_priv->loader.ctrl_base = base;
    return ERROR_OK;
}

static int qspi_mio_init(struct flash_bank *bank)
{
    struct target *target = bank->target;
    uint8_t mio_num;
    uint32_t value=0;
    for (mio_num = 0; mio_num < 28; mio_num = mio_num + 4)
    {
        if(target_read_u32(target, MIO_BASE + mio_num, &value)!=ERROR_OK)
            return ERROR_FAIL;
        if(value != 1)
        {
            LOG_DEBUG("mio reg %x init ", (MIO_BASE + mio_num));
            if(target_write_u32(target,  MIO_BASE + mio_num, 1)!= ERROR_OK)
                return ERROR_FAIL;
        }
    }
    return ERROR_OK;
}


static int dwcssi_read_reg(struct flash_bank *bank, uint32_t *value, target_addr_t address)
{
    struct target *target = bank->target;
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;

    int result = target_read_u32(target, driver_priv->ctrl_base + address, value);
    if (result != ERROR_OK) {
        LOG_ERROR("dwcssi_read_reg() error at " TARGET_ADDR_FMT,
                driver_priv->ctrl_base + address);
        return result;
    }

    // LOG_INFO("dwcssi_read_reg() get 0x%" PRIx32 " at " TARGET_ADDR_FMT,
    //             *value, driver_priv->ctrl_base + address);
    return ERROR_OK;
}

static int dwcssi_write_reg(struct flash_bank *bank, target_addr_t address, uint32_t value)
{
    struct target *target = bank->target;
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    int result;

    // LOG_INFO("dwcssi_write_reg() writing 0x%" PRIx32 " to " TARGET_ADDR_FMT,
    //         value, driver_priv->ctrl_base + address);

    result = target_write_u32(target, driver_priv->ctrl_base + address, value);
    if (result != ERROR_OK) {
        LOG_ERROR("dwcssi_write_reg() error writing 0x%" PRIx32 " to " TARGET_ADDR_FMT,
                value, driver_priv->ctrl_base + address);
        return result;
    }
    return ERROR_OK;
}

static int dwcssi_get_bits(struct flash_bank *bank, target_addr_t address, uint32_t bitmask, uint32_t index)
{
    uint32_t rd_value, ret_value;

    if(dwcssi_read_reg(bank, &rd_value, address) != ERROR_OK)
        return ERROR_FAIL;

    ret_value = (rd_value & bitmask) >> index;
    return ret_value;
}

static int dwcssi_set_bits(struct flash_bank *bank, target_addr_t address, uint32_t value, uint32_t bitmask)
{
    uint32_t rd_value, wr_value;
    if(dwcssi_read_reg(bank, &rd_value, address) != ERROR_OK)
        return ERROR_FAIL;

    wr_value = (rd_value & ~bitmask) | (value & bitmask);

    dwcssi_write_reg(bank, address, wr_value);
    return ERROR_OK;

}

/*config dwcssi controller*/

static int dwcssi_disable(struct flash_bank *bank)
{
    uint32_t ssic_en;

    // LOG_INFO("dwcssi disable");
    if(dwcssi_read_reg(bank, &ssic_en, DWCSSI_REG_SSIENR) != ERROR_OK)
        return ERROR_FAIL;
    return dwcssi_write_reg(bank, DWCSSI_REG_SSIENR, ssic_en & ~DWCSSI_SSIC_EN(1));

}

static int dwcssi_enable(struct flash_bank *bank)
{
    uint32_t ssic_en;
    // LOG_INFO("dwcssi enable");
    if(dwcssi_read_reg(bank, &ssic_en, DWCSSI_REG_SSIENR) != ERROR_OK)
        return ERROR_FAIL;
    return dwcssi_write_reg(bank, DWCSSI_REG_SSIENR, ssic_en | DWCSSI_SSIC_EN(1));
}

static int dwcssi_config_BAUDR(struct flash_bank *bank, uint32_t sckdv)
{
    uint32_t val, mask;
    // LOG_INFO("config init");

    val = DWCSSI_BAUDR_SCKDV(sckdv);
    mask = DWCSSI_BAUDR_SCKDV_MASK;

    dwcssi_set_bits(bank, DWCSSI_REG_BAUDR, val, mask);
    return ERROR_OK;
}

static int dwcssi_config_SER(struct flash_bank *bank, uint32_t enable)
{
    uint32_t val, mask;
    val = DWCSSI_SER(enable << bank->bank_number);
    mask = DWCSSI_SER_MASK;
    LOG_DEBUG("config SER %x mask %x", val, mask);
    dwcssi_set_bits(bank, DWCSSI_REG_SER, val, mask);
    return ERROR_OK;
}

static int dwcssi_config_CTRLR0(struct flash_bank *bank, uint32_t dfs, uint32_t spi_ref, uint32_t tmod)
{
    uint32_t val, mask;
    // LOG_INFO("config CTRLR0");
    val = DWCSSI_CTRLR0_DFS(dfs) | DWCSSI_CTRLR0_SPI_FRF(spi_ref) | DWCSSI_CTRLR0_TMOD(tmod);
    mask = DWCSSI_CTRLR0_DFS_MASK | DWCSSI_CTRLR0_SPI_FRF_MASK | DWCSSI_CTRLR0_TMOD_MASK;

    dwcssi_set_bits(bank, DWCSSI_REG_CTRLR0, val, mask);
    return ERROR_OK;
}

static int dwcssi_config_CTRLR1(struct flash_bank *bank, uint32_t ndf)
{
    // LOG_INFO("config CTRLR1 ndf %x", ndf);
    dwcssi_set_bits(bank, DWCSSI_REG_CTRLR1, DWCSSI_CTRLR1_NDF(ndf), DWCSSI_CTRLR1_NDF_MASK);   
    return ERROR_OK;
}

static int dwcssi_config_TXFTLR(struct flash_bank *bank, uint32_t tft, uint32_t txfthr)
{
    uint32_t val, mask;
    
    val  = DWCSSI_TXFTLR_TFT(tft) | DWCSSI_TXFTLR_TXFTHR(txfthr);
    mask = DWCSSI_TXFTLR_TFT_MASK | DWCSSI_TXFTLR_TXFTHR_MASK;
    dwcssi_set_bits(bank, DWCSSI_REG_TXFTLR, val, mask);
    return ERROR_OK;
}

static int dwcssi_config_RXFTLR(struct flash_bank *bank, uint32_t rft)
{
    uint32_t val, mask;
    
    val  = DWCSSI_RXFTLR_RFT(rft);
    mask = DWCSSI_RXFTLR_RFT_MASK;
    dwcssi_set_bits(bank, DWCSSI_REG_RXFTLR, val, mask);
    return ERROR_OK;
}

static uint32_t dwcssi_set_SPICTRLR0(uint8_t trans_type, uint8_t addr_len, uint8_t stretch_en, uint8_t wait_cycle)
{
    dwcssi_spi_ctrlr0_t spi_ctrlr0;

    spi_ctrlr0.reg_val = 0;

    spi_ctrlr0.reg_fields.TRANS_TYPE            = trans_type;
    spi_ctrlr0.reg_fields.ADDR_L                = addr_len;
    spi_ctrlr0.reg_fields.INST_L                = INST_L8;
    spi_ctrlr0.reg_fields.CLK_STRETCH_EN        = stretch_en; 
    spi_ctrlr0.reg_fields.WAIT_CYCLES           = wait_cycle;
    return spi_ctrlr0.reg_val;
}

// ctrl for enhanced spi mode
static int dwcssi_config_SPICTRLR0(struct flash_bank *bank, uint8_t trans_type, uint8_t addr_len, uint8_t stretch_en, uint8_t wait_cycle)
{
    uint32_t spictrl;
    // dwcssi_spi_ctrlr0_t spi_ctrlr0;

    // spi_ctrlr0.reg_val = 0;

    // spi_ctrlr0.reg_fields.TRANS_TYPE            = trans_type;
    // spi_ctrlr0.reg_fields.ADDR_L                = addr_len;
    // spi_ctrlr0.reg_fields.INST_L                = INST_L8;
    // spi_ctrlr0.reg_fields.CLK_STRETCH_EN        = stretch_en; 
    // spi_ctrlr0.reg_fields.WAIT_CYCLES           = wait_cycle;
    spictrl = dwcssi_set_SPICTRLR0(trans_type, addr_len, stretch_en, wait_cycle);
    dwcssi_set_bits(bank, DWCSSI_REG_SPI_CTRLR0, spictrl, 0xFFFFFFFF);
    return ERROR_OK;
}



// static void dwcssi_x4_mode(struct flash_bank *bank, uint32_t tmod)
// {
//     dwcssi_spi_ctrlr0_t spi_ctrlr0;

//     spi_ctrlr0.reg_val = 0;

//     spi_ctrlr0.reg_fields.TRANS_TYPE            = TRANS_TYPE_TT0;
//     spi_ctrlr0.reg_fields.ADDR_L                = ADDR_L32;
//     spi_ctrlr0.reg_fields.INST_L                = INST_L8;
//     spi_ctrlr0.reg_fields.CLK_STRETCH_EN        = ENABLE; 
//     if(tmod == TX_ONLY)
//     {
//         spi_ctrlr0.reg_fields.WAIT_CYCLES       = 0;
//     }
//     else
//     {
//         spi_ctrlr0.reg_fields.WAIT_CYCLES       = 8;
//     }

//     dwcssi_config_SPICTRLR0_2(bank, spi_ctrlr0.reg_val, 0xFFFFFFFF);
//     // LOG_INFO("dwcssi config SPICTRLR0 value %x", spi_ctrlr0.reg_val);
// }

static void dwcssi_config_init(struct flash_bank *bank)
{

    dwcssi_disable(bank);
    dwcssi_config_BAUDR(bank, 2);
    dwcssi_config_SER(bank, ENABLE);
    dwcssi_config_CTRLR0(bank, DFS_BYTE, SPI_FRF_X1_MODE, EEPROM_READ);
    dwcssi_enable(bank);
}

static void dwcssi_config_eeprom(struct flash_bank *bank, uint32_t len)
{
    dwcssi_disable(bank);
    dwcssi_config_SER(bank,1);
    dwcssi_config_CTRLR0(bank, DFS_BYTE, SPI_FRF_X1_MODE, EEPROM_READ);
    dwcssi_config_CTRLR1(bank, len-1);
    dwcssi_config_TXFTLR(bank, 0, 0);
    dwcssi_enable(bank);
}

void dwcssi_config_tx(struct flash_bank *bank, uint8_t frf, uint32_t tx_total_len, uint32_t tx_start_lv)
{
    dwcssi_disable(bank);
    dwcssi_config_SER(bank,1);
    dwcssi_config_CTRLR0(bank, DFS_BYTE, frf, TX_ONLY);
    dwcssi_config_TXFTLR(bank, 0, tx_start_lv);
    dwcssi_enable(bank);
}

static void dwcssi_config_rx(struct flash_bank *bank, uint8_t frf, uint8_t rx_ip_lv)
{
    dwcssi_disable(bank);
    dwcssi_config_SER(bank,1);
    dwcssi_config_CTRLR0(bank, DFS_BYTE, frf, RX_ONLY);
    dwcssi_config_RXFTLR(bank, rx_ip_lv);
    // if(frf == SPI_FRF_X4_MODE)
    // {
    //     dwcssi_x4_mode(bank, RX_ONLY);
    // }
    dwcssi_enable(bank);
}

void dwcssi_config_trans(struct flash_bank *bank, struct dwcssi_trans_config *trans_config)
{

    switch(trans_config->tmod)
    {
        case(EEPROM_READ): 
            dwcssi_config_eeprom(bank, trans_config->ndf); 
            break;
        case(TX_ONLY): 
            dwcssi_config_tx(bank, trans_config->spi_frf, 0, trans_config->tx_start_lv);
            // LOG_INFO("spi frf %x tx_start_lv %x", trans_config->spi_frf, trans_config->tx_start_lv);
            if(trans_config->spi_frf == SPI_FRF_X4_MODE)
            {
                // LOG_INFO("spi trans_type %x addr len %x stretch_en %x",trans_config->trans_type, trans_config->addr_len, trans_config->stretch_en);

                dwcssi_disable(bank);
                dwcssi_config_SPICTRLR0(bank, trans_config->trans_type, trans_config->addr_len, trans_config->stretch_en, 0);
                if(trans_config->stretch_en)
                    dwcssi_config_CTRLR1(bank, trans_config->ndf);
                dwcssi_enable(bank);
            }
            break;
        case(RX_ONLY):
            dwcssi_config_rx(bank, trans_config->spi_frf, trans_config->rx_ip_lv);
            if(trans_config->spi_frf == SPI_FRF_X4_MODE)
            {
                dwcssi_disable(bank);
                dwcssi_config_SPICTRLR0(bank, trans_config->trans_type, trans_config->addr_len, trans_config->stretch_en, trans_config->wait_cycle);
                dwcssi_enable(bank);
            }
            break;
        default:
            LOG_ERROR("invald trans mode");
            break;
    }
}

/*dwc base functions*/
static int dwcssi_wait_tx_empty(struct flash_bank* bank)
{
    int64_t start = timeval_ms();
    while(1)
    {
        if(dwcssi_get_bits(bank, DWCSSI_REG_SR, DWCSSI_SR_TFE_MASK, 2))
            break;
        int64_t now = timeval_ms();
        if (now - start > 1000) {
            LOG_ERROR("txfifo timeout");
            return ERROR_TARGET_TIMEOUT;
        }
    }
    return ERROR_OK;
}


int dwcssi_txwm_wait(struct flash_bank* bank)
{
    int64_t start = timeval_ms();
    // uint32_t ir_status;
    // TX fifo empty
    dwcssi_wait_tx_empty(bank);
    while (1) {
        uint32_t status;
        if (dwcssi_read_reg(bank, &status, DWCSSI_REG_SR) != ERROR_OK)
        // if (dwcssi_read_reg(bank, &status, DWCSSI_REG_ISR) != ERROR_OK)
            return ERROR_FAIL;
        if (!(status & DWCSSI_SR_BUSY(1)))
            break;
        int64_t now = timeval_ms();
        if (now - start > 1000) {
            // dwcssi_read_reg(bank, &ir_status, DWCSSI_REG_ISR);
            // LOG_INFO("ctl staus %x interrupt status %x", status, ir_status);
            LOG_ERROR("ssi timeout");
            return ERROR_TARGET_TIMEOUT;
        }
    }

    return ERROR_OK;
}


int dwcssi_tx(struct flash_bank *bank, uint32_t in)
{
    int64_t start = timeval_ms();
    int     fifo_not_full=0;

    while(1) 
    {
        fifo_not_full = dwcssi_get_bits(bank, DWCSSI_REG_SR, DWCSSI_SR_TFTNF_MASK, 1);
        // LOG_INFO("tx fifo status %d\n", fifo_not_full);
        if(fifo_not_full)
            break;
        int64_t now = timeval_ms();
        if (now - start > 1000) {
            LOG_ERROR("txfifo full");
            return ERROR_TARGET_TIMEOUT;
        }
    }

    dwcssi_write_reg(bank, DWCSSI_REG_DRx_START, in);
    // LOG_INFO("tx %x", in);
    return ERROR_OK;
}

static int dwcssi_tx_buf(struct flash_bank * bank, const uint8_t* in_buf, uint32_t in_cnt)
{
    uint32_t i;
    uint8_t sr;
    for(i = 0; i < in_cnt; i++)
    {
        dwcssi_tx(bank, *(in_buf+i));
    }

    // return (dwcssi_txwm_wait(bank));
    return dwcssi_wait_flash_idle(bank, 100, &sr);
}

static int dwcssi_rx(struct flash_bank *bank, uint8_t *out)
{
    uint32_t value;

    if(dwcssi_get_bits(bank, DWCSSI_REG_SR, DWCSSI_SR_RFNE_MASK, 3))
    {
        dwcssi_read_reg(bank, &value, DWCSSI_REG_DRx_START);
        if (out)
            *out = value & 0xff;
        return ERROR_OK;
    }

    return ERROR_WAIT;
}

static int dwcssi_rx_buf(struct flash_bank *bank, uint8_t* out_buf, uint32_t out_cnt)
{
    uint32_t i=0;
    while(i < out_cnt)
    {
        if(dwcssi_rx(bank, out_buf+i) == ERROR_OK)
            i++;
        // LOG_INFO("read buf %x data %x", i, *(out_buf+i));
    }
    return ERROR_OK;
}

//dwcssi-flash intf

int dwcssi_wait_flash_idle(struct flash_bank *bank, int timeout, uint8_t* sr)
{
    // struct dwcssi_flash_bank* driver_priv = bank->driver_priv;
    // const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;

    int64_t endtime;
    int retval = ERROR_FAIL;
    uint8_t rx = ERROR_FAIL;
    uint32_t rx_len = 1;

    // LOG_INFO("check flash status");

    dwcssi_config_eeprom(bank, rx_len);

    endtime = timeval_ms() + timeout;

    while(timeval_ms() < endtime)
    {
        dwcssi_tx(bank, SPIFLASH_READ_STATUS);
        if(dwcssi_txwm_wait(bank)!= ERROR_OK)
        {
            retval = ERROR_FAIL;
            break;
        }        

        if(dwcssi_rx(bank, &rx) == ERROR_OK)
        {
            *sr = rx;
            if((rx & SPIFLASH_BSY_BIT) == 0) {

                retval = ERROR_OK;
                LOG_DEBUG("flash status %x idle", rx);
                break;
            }
        }
        else
            break;
    }


    // if((retval == ERROR_OK) && (flash_ops!= NULL))
    // {
    //     driver_priv->flash_sr1 = rx;
    //     retval = flash_ops->err_chk(bank);
    // }

    return retval;
}

int dwcssi_wait_flash_idle_quad(struct flash_bank *bank)
{
    // struct dwcssi_flash_bank* driver_priv = bank->driver_priv;
    // const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;

    int64_t endtime;
    int retval = ERROR_FAIL;
    uint8_t rx;
    struct dwcssi_trans_config trans_config;

    trans_config.addr_len = ADDR_L0;
    trans_config.ndf      = 1;
    trans_config.rx_ip_lv = 0x3F;
    trans_config.spi_frf  = SPI_FRF_X4_MODE;
    trans_config.stretch_en = ENABLE;
    trans_config.tmod       = RX_ONLY;
    trans_config.trans_type = TRANS_TYPE_TT0;
    trans_config.wait_cycle = 0;

    // LOG_INFO("check flash status");

    dwcssi_config_trans(bank, &trans_config);
    dwcssi_disable(bank);
    dwcssi_config_CTRLR1(bank, 0);
    dwcssi_enable(bank);

    endtime = timeval_ms() + 100;

    while(timeval_ms() < endtime)
    {
        dwcssi_tx(bank, SPIFLASH_READ_STATUS);
        if(dwcssi_txwm_wait(bank)!= ERROR_OK)
        {
            retval = ERROR_FAIL;
            break;
        }        

        if(dwcssi_rx(bank, &rx) == ERROR_OK)
        {
            LOG_INFO("flash status %x idle", rx);
            if((rx & SPIFLASH_BSY_BIT) == 0) {

                retval = ERROR_OK;
                break;
            }
        }
        else
            break;
    }


    // if((retval == ERROR_OK) && (flash_ops!= NULL))
    // {
    //     driver_priv->flash_sr1 = rx;
    //     retval = flash_ops->err_chk(bank);
    // }

    return retval;
}


int dwcssi_flash_tx_cmd(struct flash_bank *bank, uint8_t *cmd, uint8_t len, uint8_t cmd_mode)
{
    struct dwcssi_trans_config trans_config;
    int i;
    trans_config.tmod = TX_ONLY;
    trans_config.tx_start_lv = len - 1;
    trans_config.rx_ip_lv = 0;
    trans_config.trans_type = TRANS_TYPE_TT0;
    trans_config.stretch_en = DISABLE;
    trans_config.addr_len = 0;
    trans_config.wait_cycle = 0;
    if(cmd_mode == QPI_MODE)
    {
        trans_config.spi_frf = SPI_FRF_X4_MODE;
        trans_config.trans_type = TRANS_TYPE_TT2;
    }
    else
    {
        trans_config.spi_frf = SPI_FRF_X1_MODE;
    }
    dwcssi_config_trans(bank, &trans_config);
    for(i=0; i<len; i++)
    {
        dwcssi_tx(bank, *(cmd+i));
    }
    dwcssi_txwm_wait(bank);
    
    return ERROR_OK;
}

static int dwcssi_flash_wr_en(struct flash_bank *bank, uint8_t frf)
{
    uint8_t sr;
    dwcssi_config_tx(bank, frf, 0, 0);
    dwcssi_tx(bank, SPIFLASH_WRITE_ENABLE);
    return dwcssi_wait_flash_idle(bank, 100, &sr);
}

int dwcssi_rd_flash_reg(struct flash_bank *bank, uint32_t* rd_val, uint8_t cmd, uint32_t len)
{
    // LOG_INFO("dwcssi read flash reg");
    int64_t endtime;
    uint8_t rx = 0, i = 0;
    dwcssi_txwm_wait(bank);
    dwcssi_config_eeprom(bank, len);
    dwcssi_tx(bank, cmd);

    dwcssi_txwm_wait(bank);
    endtime = timeval_ms() + DWCSSI_PROBE_TIMEOUT;

    while(timeval_ms() < endtime || i < len)
    {
        if(dwcssi_rx(bank, &rx) == ERROR_OK)
        {
            *rd_val |= rx << (8 * i);
            i = i + 1;
        }
    }
    return ERROR_OK;
}

int dwcssi_wr_flash_reg(struct flash_bank *bank, uint8_t *cmd, uint8_t len, uint8_t cmd_mode)
{
    uint8_t wr_en = SPIFLASH_WRITE_ENABLE;
    uint8_t sr;
    dwcssi_flash_tx_cmd(bank, &wr_en, 1, cmd_mode);
    
    dwcssi_flash_tx_cmd(bank, cmd, len, cmd_mode);

    // dwcssi_disable(bank);
    // dwcssi_config_SER(bank, 0);
    // dwcssi_enable(bank);

    // dwcssi_disable(bank);
    // dwcssi_config_SER(bank, ENABLE);
    // dwcssi_enable(bank);

    return dwcssi_wait_flash_idle(bank, 1000, &sr);
}

static int dwcssi_erase_sector(struct flash_bank *bank, unsigned int sector)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;
    uint8_t flash_sr, offset_shift;
    int addr_bytes = flash_ops->addr_len >> 1, i;
    uint32_t offset;
    int retval;

    offset = bank->sectors[sector].offset;

    dwcssi_flash_wr_en(bank, SPI_FRF_X1_MODE);
    dwcssi_config_tx(bank, SPI_FRF_X1_MODE, 0xFF, addr_bytes);
    // LOG_INFO("dwcssi erase cmd %x sector %x", driver_priv->dev->erase_cmd, offset);

    dwcssi_tx(bank, driver_priv->dev->erase_cmd);
    for(i = (addr_bytes-1); i >= 0; i--)
    {
        offset_shift = i<<3;
        dwcssi_tx(bank, offset >> offset_shift);
    }
    dwcssi_wait_tx_empty(bank);
    retval = dwcssi_wait_flash_idle(bank, DWCSSI_MAX_TIMEOUT, &flash_sr);

    return retval;
}

/*dwc driver*/
static int dwcssi_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{

    struct target *target = bank->target;
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;

    unsigned int sector;
    int retval = ERROR_OK;

    LOG_DEBUG("%s: from sector %u to sector %u", __func__, first, last);
    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    if ((last < first) || (last >= bank->num_sectors)) {
        LOG_ERROR("Flash sector invalid");
        return ERROR_FLASH_SECTOR_INVALID;
    }

    if (!(driver_priv->probed)) {
        LOG_ERROR("Flash bank not probed");
        return ERROR_FLASH_BANK_NOT_PROBED;
    }

    for (sector = first; sector <= last; sector++) {
        if (bank->sectors[sector].is_protected) {
            LOG_ERROR("Flash sector %u protected", sector);
            return ERROR_FAIL;
        }
    }

    if (driver_priv->dev->erase_cmd == 0x00)
        return ERROR_FLASH_OPER_UNSUPPORTED;

    flash_ops->quad_dis(bank);
    for (sector = first; sector <= last; sector++)
    {
        retval = dwcssi_erase_sector(bank, sector);
        if (retval != ERROR_OK)
            break;

        keep_alive();
    }

    return retval;
}

static int dwcssi_protect(struct flash_bank *bank, int set, unsigned int first, unsigned last)
{
    for(unsigned int sector = first; sector <= last; sector ++)
        bank->sectors[sector].is_protected = set;

    return ERROR_OK;
}

static int dwcssi_read_page(struct flash_bank *bank, uint8_t *buffer, uint32_t offset, uint32_t len)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;

    LOG_DEBUG("dwcssi read page offset %x len %x", offset, len);
    dwcssi_disable(bank);
    dwcssi_config_CTRLR1(bank, len);
    dwcssi_enable(bank);
    dwcssi_tx(bank, flash_ops->qread_cmd);
    dwcssi_tx(bank, offset);
    dwcssi_wait_tx_empty(bank);
    dwcssi_rx_buf(bank, buffer, len);
    return ERROR_OK;
}

static int dwcssi_read(struct flash_bank *bank, uint8_t *buffer, uint32_t offset, uint32_t count)
{
    uint32_t cur_count, page_size;
    uint32_t page_offset;
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;

    page_size = driver_priv->dev->pagesize ? 
                driver_priv->dev->pagesize : SPIFLASH_DEF_PAGESIZE;

    page_offset = offset % page_size;

    LOG_DEBUG("read addr %x count %x", offset, count);
    flash_ops->quad_en(bank);
    flash_ops->trans_config(bank, RX_ONLY);
    // dwcssi_config_rx(bank, SPI_FRF_X4_MODE, 0x3F);

    while(count > 0)
    {
        if(page_offset + count > page_size)
            cur_count = page_size - page_offset;
        else
            cur_count = count;
        dwcssi_read_page(bank, buffer, offset, cur_count);

        page_offset = 0;
        buffer        += cur_count;
        offset        += cur_count;
        count         -= cur_count;
    }

    return ERROR_OK;
}

static const uint8_t riscv32_crc_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_crc_riscv_32.inc"
};

static const uint8_t riscv64_crc_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_crc_riscv_64.inc"
};

static const uint8_t aarch64_crc_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_crc_aarch_64.inc"
};

static struct code_src crc_srcs[3] = 
{
    [RV64_SRC] = {riscv64_crc_bin, sizeof(riscv64_crc_bin)},
    [RV32_SRC] = {riscv32_crc_bin, sizeof(riscv32_crc_bin)},
    [AARCH64_SRC] = {aarch64_crc_bin, sizeof(aarch64_crc_bin)},
};

static void dwcssi_checksum_params_priv(struct flash_loader *loader)
{
    struct dwcssi_flash_bank  *driver_priv = loader->dev_info;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;
    
    buf_set_u64(loader->reg_params[4].value, 0, loader->xlen, flash_ops->qread_cmd);
    // LOG_INFO("target set %s qread_cmd %x", loader->reg_params[4].reg_name, flash_ops->qread_cmd);
}

static int dwcssi_checksum(struct flash_bank *bank, target_addr_t address, uint32_t count, uint32_t *crc)
{
    int retval = ERROR_OK;
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;
    struct flash_loader *loader = &driver_priv->loader;

    loader->work_mode = CRC_CHECK;
    loader->block_size = driver_priv->dev->pagesize;
    loader->image_size = count;
    loader->param_cnt = 5;
    loader->set_params_priv = dwcssi_checksum_params_priv;

    flash_ops->quad_en(bank);
    flash_ops->trans_config(bank, RX_ONLY);   
    retval = loader_flash_crc(loader, crc_srcs, address, crc);
    return retval;
}

static int dwcssi_verify(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
    int retval = ERROR_OK;
    uint32_t target_crc, image_crc;

    // count = 0x110;

    retval = image_calculate_checksum(buffer, count, &image_crc);
    if(retval != ERROR_OK)
        return retval;
    
    retval = dwcssi_checksum(bank, offset, count, &target_crc);

    if(retval != ERROR_OK)
        return retval;

    if(~image_crc != ~target_crc)
    {
        LOG_ERROR("checksum image %x target %x", image_crc, target_crc);
        retval = ERROR_FAIL;
    }

    return retval;
}

static int slow_dwcssi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t len)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;
    struct dwcssi_trans_config trans_config;
    int retval;
    trans_config.tmod = TX_ONLY;
    trans_config.spi_frf = SPI_FRF_X4_MODE;
    trans_config.ndf = len - 1;

    trans_config.trans_type = TRANS_TYPE_TT0;
    trans_config.stretch_en = ENABLE;
    trans_config.addr_len = flash_ops->addr_len;
    trans_config.wait_cycle = 0;
    trans_config.tx_start_lv = 0;
    LOG_INFO("dwcssi slow write offset %x len %x", offset, len);
    flash_ops->quad_en(bank);
    dwcssi_flash_wr_en(bank, SPI_FRF_X1_MODE);
    dwcssi_config_trans(bank, &trans_config);
    dwcssi_tx(bank, flash_ops->qprog_cmd); // 1byte cmd
    dwcssi_tx(bank, offset); //addr
    retval = dwcssi_tx_buf(bank, buffer, len);

    return retval;

}

static const uint8_t riscv32_sync_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_sync_riscv_32.inc"
};

static const uint8_t riscv64_sync_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_sync_riscv_64.inc"
};

static const uint8_t aarch64_sync_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_sync_aarch_64.inc"
};

static const uint8_t riscv32_async_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_async_riscv_32.inc"
};

static const uint8_t riscv64_async_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_async_riscv_64.inc"
};

static const uint8_t aarch64_async_bin[] = {
#include "../../../../contrib/loaders/flash/qspi/dwcssi/build/flash_async_aarch_64.inc"
};

static struct code_src sync_srcs[3] = 
{
    [RV64_SRC] = {riscv64_sync_bin, sizeof(riscv64_sync_bin)},
    [RV32_SRC] = {riscv32_sync_bin, sizeof(riscv32_sync_bin)},
    [AARCH64_SRC] = {aarch64_sync_bin, sizeof(aarch64_sync_bin)},

};

static struct code_src async_srcs[3] = 
{
    [RV64_SRC] = {riscv64_async_bin, sizeof(riscv64_async_bin)},
    [RV32_SRC] = {riscv32_async_bin, sizeof(riscv32_async_bin)},
    [AARCH64_SRC] = {aarch64_async_bin, sizeof(aarch64_async_bin)},
};

static void dwcssi_write_sync_params_priv(struct flash_loader *loader)
{
    struct dwcssi_flash_bank  *driver_priv = loader->dev_info;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;
    uint32_t spictrl;

    spictrl = dwcssi_set_SPICTRLR0(TRANS_TYPE_TT0, flash_ops->addr_len, ENABLE, 0);
    buf_set_u64(loader->reg_params[5].value, 0, loader->xlen, flash_ops->qprog_cmd);
    buf_set_u64(loader->reg_params[6].value, 0, loader->xlen, spictrl);
}

static void dwcssi_write_async_params_priv(struct flash_loader *loader)
{
    struct dwcssi_flash_bank  *driver_priv = loader->dev_info;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;
    uint32_t spictrl;

    spictrl = dwcssi_set_SPICTRLR0(TRANS_TYPE_TT0, flash_ops->addr_len, ENABLE, 0);

    buf_set_u64(loader->reg_params[6].value, 0, loader->xlen, flash_ops->qprog_cmd);
    buf_set_u64(loader->reg_params[7].value, 0, loader->xlen, spictrl);
}



static int dwcssi_write_async(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;
    struct flash_loader *loader = &driver_priv->loader;

    int retval;

    loader->work_mode = ASYNC_TRANS;
    loader->block_size = driver_priv->dev->pagesize; 
    loader->image_size = count;
    loader->param_cnt = 8;
    loader->set_params_priv = dwcssi_write_async_params_priv;
    LOG_DEBUG("count %x block size %x image size %x", count, loader->block_size, loader->image_size);

    flash_ops->quad_en(bank);

    retval = loader_flash_write_async(loader, async_srcs, 
        buffer, offset, count);
    return retval;
}

static int dwcssi_write_sync(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = driver_priv->dev->flash_ops;
    struct flash_loader *loader = &driver_priv->loader;
    int retval;

    loader->work_mode = SYNC_TRANS;
    loader->block_size = driver_priv->dev->pagesize; 
    loader->image_size = count;
    loader->param_cnt = 7;
    loader->set_params_priv = dwcssi_write_sync_params_priv;

    flash_ops->quad_en(bank);
 
    retval = loader_flash_write_sync(loader, sync_srcs, buffer, offset, count);

    if(retval != ERROR_OK)
        LOG_ERROR("dwcssi write sync error");

    return retval;
}
static int dwcssi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
    int retval = ERROR_FAIL;
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    uint32_t page_offset, page_size, cur_count;

    page_size = driver_priv->dev->pagesize;
    count = flash_write_boundary_check(bank, offset, count);
	while (count > 0) {
		/* clip block at page boundary */
		if (page_offset + count > page_size)
			cur_count = page_size - page_offset;
		else
			cur_count = count;
		retval = slow_dwcssi_write(bank, buffer, offset, cur_count);
		if (retval != ERROR_OK)
			return ERROR_FAIL;
		page_offset = 0;
		buffer += cur_count;
		offset += cur_count;
		count -= cur_count;
	}
    retval = ERROR_OK;

    if(0)
    {
    retval = dwcssi_write_async(bank, buffer, offset, count);

    if(retval != ERROR_OK)
    {
        retval = dwcssi_write_sync(bank, buffer, offset, count);
    }

    }

    return retval;
}

int driver_priv_init(struct flash_bank* bank, struct dwcssi_flash_bank *driver_priv)
{
    const struct dwcssi_target *target_device;
    struct target *target = bank->target;
// reset probe state
    if(driver_priv->probed)
        free(bank->sectors);
    driver_priv->probed = false;

    // init ctrl_base
    if(driver_priv->ctrl_base == 0)
    {
        for(target_device = target_devices; target_device->name; ++target_device)
        {
            if(target_device->tap_idcode == target->tap->idcode)
                break;
        }

        if(!target_device->name) {
            LOG_ERROR("Device ID 0x%" PRIx32 " is not known as DWCSSPI capable",target->tap->idcode);
            return ERROR_FAIL;
        }

        driver_priv->ctrl_base = target_device->ctrl_base;
        LOG_DEBUG("Valid DWCSSI on device %s at address" TARGET_ADDR_FMT, target_device->name, bank->base);
    }
    else
    {
        LOG_DEBUG("Assuming DWCSSI as specified at address " TARGET_ADDR_FMT "with ctrl at" TARGET_ADDR_FMT,
         driver_priv->ctrl_base, bank->base);
    }
    return ERROR_OK;
}

static int dwcssi_read_id_reset(struct flash_bank * bank, int (*reset)(struct flash_bank*, uint8_t cmd_mode), uint8_t cmd_mode)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    uint32_t id = 0;
    
    if(reset != NULL)
        reset(bank, cmd_mode);
    else
    {
        if(cmd_mode == QPI_MODE)
            return ERROR_FAIL;
    }

    dwcssi_rd_flash_reg(bank, &id, SPIFLASH_READ_ID, 3);
    return flash_id_parse(driver_priv, id);
}

static int dwcssi_read_id(struct flash_bank *bank)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    const flash_ops_t *flash_ops = NULL;

    if(0) // err status clean
    {
        uint8_t config_reg[3] = {0x01, 0, 0};
        dwcssi_wr_flash_reg(bank, config_reg, 3, STANDARD_SPI_MODE);

    }

    if(0) // qpi mode exit
    {
    uint32_t sr_byte1 = 0;
    uint8_t qpi_seq[2] = {0x31, 0}, qpi_enable = 0x38;    
    dwcssi_rd_flash_reg(bank, &sr_byte1, 0x35, 1);
    LOG_INFO("sr_byte1 %x", sr_byte1);
    qpi_seq[1]= sr_byte1 | 0x2;
    dwcssi_wr_flash_reg(bank, qpi_seq, 2, STANDARD_SPI_MODE);

    LOG_INFO("send qpi cmd");
    dwcssi_flash_tx_cmd(bank, &qpi_enable, 1, STANDARD_SPI_MODE);
    }

    // LOG_INFO("sr_byte1 %x", sr_byte1);
    for(int i=0; i<3; i++)
    {
        if(dwcssi_read_id_reset(bank, reset_methods[i], STANDARD_SPI_MODE) == ERROR_OK)
            break;
        else
        {
            if(dwcssi_read_id_reset(bank, reset_methods[i], QPI_MODE) == ERROR_OK)
                break;
        }
    }

    if(driver_priv->probed == true)
    {
        flash_sector_init(bank, driver_priv);
        flash_ops = driver_priv->dev->flash_ops;
        flash_ops->quad_dis(bank);
        return ERROR_OK;
    }
    else
        return ERROR_FAIL;

    return ERROR_OK;

}


static int dwcssi_probe(struct flash_bank *bank)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;

    LOG_INFO("probe bank %d name %s", bank->bank_number, bank->name);
    driver_priv_init(bank, driver_priv);
    if(qspi_mio_init(bank) != ERROR_OK)
    {
        LOG_ERROR("mio init fail");
        return ERROR_FAIL;
    }

    dwcssi_config_init(bank);

    return dwcssi_read_id(bank);
}


static int dwcssi_auto_probe(struct flash_bank *bank)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    if(driver_priv->probed)
        return ERROR_OK;
    return dwcssi_probe(bank);
}

static int dwcssi_protect_check(struct flash_bank *bank)
{
    /* Nothing to do. Protection is only handled in SW. */
    
    return ERROR_OK;
}

static int get_driver_priv(struct flash_bank *bank, struct command_invocation *cmd)
{
    struct dwcssi_flash_bank *driver_priv = bank->driver_priv;

    if (!(driver_priv->probed)) {
        command_print_sameline(cmd, "\ndwcssi flash bank not probed yet\n");
        return ERROR_OK;
    }

    command_print_sameline(cmd, "\ndwcssi flash information:\n"
            "  Device \'%s\' (ID 0x%08" PRIx32 ")\n",
            driver_priv->dev->name, driver_priv->dev->device_id);

    return ERROR_OK;
}


const struct flash_driver dwcssi_flash = {
    .name = "dwcssi",
    .flash_bank_command = dwcssi_flash_bank_command,
    .erase = dwcssi_erase,
    .protect = dwcssi_protect,
    .write = dwcssi_write,
    .read = dwcssi_read,
    .verify = dwcssi_verify,
    .probe = dwcssi_probe,
    .auto_probe = dwcssi_auto_probe,
    .erase_check = default_flash_blank_check,
    .protect_check = dwcssi_protect_check,
    .info = get_driver_priv,
    .free_driver_priv = default_flash_free_driver_priv
};
