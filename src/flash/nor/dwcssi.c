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


FLASH_BANK_COMMAND_HANDLER(dwcssi_flash_bank_command)
{
    struct dwcssi_flash_bank *dwcssi_info;

    LOG_DEBUG("%s", __func__);

    if (CMD_ARGC < 6)
        return ERROR_COMMAND_SYNTAX_ERROR;

    dwcssi_info = malloc(sizeof(struct dwcssi_flash_bank));
    if(!dwcssi_info) {
        LOG_ERROR("not enough memory");
        return ERROR_FAIL;
    }

    bank->driver_priv = dwcssi_info;
    dwcssi_info->probed = false;
    dwcssi_info->ctrl_base = 0;
    if (CMD_ARGC >= 7) {
        COMMAND_PARSE_ADDRESS(CMD_ARGV[6], dwcssi_info->ctrl_base);
        LOG_DEBUG("ASSUMING DWCSSI device at ctrl_base = " TARGET_ADDR_FMT,
                dwcssi_info->ctrl_base);
    }
    return ERROR_OK;
}

static void qspi_mio_init(struct flash_bank *bank)
{
    struct target *target = bank->target;
    uint8_t mio_num;
    uint32_t value=0;
    for (mio_num = 0; mio_num < 28; mio_num = mio_num + 4)
    {
        target_read_u32(target, MIO_BASE + mio_num, &value);
        if(value != 1)
        {
            LOG_DEBUG("mio reg %x init ", (MIO_BASE + mio_num));
            target_write_u32(target,  MIO_BASE + mio_num, 1);
        }
    }
}


static int dwcssi_read_reg(struct flash_bank *bank, uint32_t *value, target_addr_t address)
{
    struct target *target = bank->target;
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;

    int result = target_read_u32(target, dwcssi_info->ctrl_base + address, value);
    if (result != ERROR_OK) {
        LOG_ERROR("dwcssi_read_reg() error at " TARGET_ADDR_FMT,
                dwcssi_info->ctrl_base + address);
        return result;
    }

    // LOG_INFO("dwcssi_read_reg() get 0x%" PRIx32 " at " TARGET_ADDR_FMT,
    //             *value, dwcssi_info->ctrl_base + address);
    return ERROR_OK;
}

static int dwcssi_write_reg(struct flash_bank *bank, target_addr_t address, uint32_t value)
{
    struct target *target = bank->target;
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;
    int result;

    // LOG_INFO("dwcssi_write_reg() writing 0x%" PRIx32 " to " TARGET_ADDR_FMT,
    //         value, dwcssi_info->ctrl_base + address);

    result = target_write_u32(target, dwcssi_info->ctrl_base + address, value);
    if (result != ERROR_OK) {
        LOG_ERROR("dwcssi_write_reg() error writing 0x%" PRIx32 " to " TARGET_ADDR_FMT,
                value, dwcssi_info->ctrl_base + address);
        return result;
    }
    return ERROR_OK;
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

static int dwcssi_get_bits(struct flash_bank *bank, target_addr_t address, uint32_t bitmask, uint32_t index)
{
    uint32_t rd_value, ret_value;

    if(dwcssi_read_reg(bank, &rd_value, address) != ERROR_OK)
        return ERROR_FAIL;

    ret_value = (rd_value & bitmask) >> index;
    return ret_value;
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

static int dwcssi_config_SPICTRLR0(struct flash_bank *bank, uint32_t val, uint32_t mask)
{
    dwcssi_set_bits(bank, DWCSSI_REG_SPI_CTRLR0, val, mask);
    return ERROR_OK;
}

static void dwcssi_x4_mode(struct flash_bank *bank, uint32_t tmod)
{
    dwcssi_spi_ctrlr0_t spi_ctrlr0;

    spi_ctrlr0.reg_val = 0;

    spi_ctrlr0.reg_fields.TRANS_TYPE            = TRANS_TYPE_TT0;
    spi_ctrlr0.reg_fields.ADDR_L                = ADDR_L32;
    spi_ctrlr0.reg_fields.INST_L                = INST_L8;
    spi_ctrlr0.reg_fields.CLK_STRETCH_EN        = ENABLE; 
    if(tmod == TX_ONLY)
    {
        spi_ctrlr0.reg_fields.WAIT_CYCLES       = 0;
    }
    else
    {
        spi_ctrlr0.reg_fields.WAIT_CYCLES       = 8;
    }

    dwcssi_config_SPICTRLR0(bank, spi_ctrlr0.reg_val, 0xFFFFFFFF);
    // LOG_INFO("dwcssi config SPICTRLR0 value %x", spi_ctrlr0.reg_val);
}


static void dwcssi_config_init(struct flash_bank *bank)
{

    dwcssi_disable(bank);
    dwcssi_config_BAUDR(bank, 2);
    dwcssi_config_SER(bank, 1);
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

    if(frf == SPI_FRF_X4_MODE)
    {
        dwcssi_config_CTRLR1(bank, tx_total_len - 1 );
        dwcssi_x4_mode(bank, TX_ONLY);
    }
    dwcssi_enable(bank);
}

static void dwcssi_config_rx(struct flash_bank *bank, uint8_t frf, uint8_t rx_ip_lv)
{
    dwcssi_disable(bank);
    dwcssi_config_SER(bank,1);
    dwcssi_config_CTRLR0(bank, DFS_BYTE, frf, RX_ONLY);
    dwcssi_config_RXFTLR(bank, rx_ip_lv);
    if(frf == SPI_FRF_X4_MODE)
    {
        dwcssi_x4_mode(bank, RX_ONLY);
    }
    dwcssi_enable(bank);
}

/*dwc base functions*/
int dwcssi_txwm_wait(struct flash_bank* bank)
{
    int64_t start = timeval_ms();
    uint32_t ir_status;
    // TX fifo empty
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

    while (1) {
        uint32_t status;
        if (dwcssi_read_reg(bank, &status, DWCSSI_REG_SR) != ERROR_OK)
        // if (dwcssi_read_reg(bank, &status, DWCSSI_REG_ISR) != ERROR_OK)
            return ERROR_FAIL;
        if (!(status & DWCSSI_SR_BUSY(1)))
            break;
        int64_t now = timeval_ms();
        if (now - start > 1000) {
            dwcssi_read_reg(bank, &ir_status, DWCSSI_REG_ISR);
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
    for(i = 0; i < in_cnt; i++)
    {
        // LOG_INFO("tx buf %x data %x", i, *(in_buf+i));
        dwcssi_tx(bank, *(in_buf+i));
    }

    return (dwcssi_txwm_wait(bank));
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
    uint32_t i;

    for(i = 0; i < out_cnt; i++)
    {
        dwcssi_rx(bank, out_buf+i);
        // LOG_INFO("read buf %x data %x", i, *(out_buf+i));
    }
    return ERROR_OK;
}

//dwcssi-flash intf

int dwcssi_wait_flash_idle(struct flash_bank *bank, int timeout, uint8_t* sr)
{
    int64_t endtime;
    uint8_t rx = ERROR_FAIL;
    // uint32_t rx_len = 0x1000;
    uint32_t rx_len = 1;

    // LOG_INFO("check flash status");

    dwcssi_config_eeprom(bank, rx_len);

    endtime = timeval_ms() + timeout;

    
    while(timeval_ms() < endtime)
    {
        dwcssi_tx(bank, SPIFLASH_READ_STATUS);
        if(dwcssi_txwm_wait(bank)!= ERROR_OK)
            return ERROR_FAIL;        

        if(dwcssi_rx(bank, &rx) == ERROR_OK)
        {
            *sr = rx;
            if((rx & SPIFLASH_BSY_BIT) == 0) {
                // LOG_INFO("flash status %x idle", rx);
                return ERROR_OK;
            }
            else if(flash_check_status(rx))
                return ERROR_FAIL;
        }    
    }

    LOG_ERROR("flash timeout");
    return ERROR_FAIL;
}

static int dwcssi_flash_wr_en(struct flash_bank *bank, uint8_t frf)
{
    uint8_t tx_start_lv = 0;

    dwcssi_config_tx(bank, frf, 0, tx_start_lv);
    dwcssi_tx(bank, SPIFLASH_WRITE_ENABLE);
    dwcssi_txwm_wait(bank);
    return ERROR_OK;
}

int dwcssi_read_flash_reg(struct flash_bank *bank, uint32_t* rd_val, uint8_t cmd, uint32_t len)
{
    // LOG_INFO("dwcssi read flash reg");
    int64_t endtime;
    uint8_t rx = 0, i = 0;
    dwcssi_txwm_wait(bank);
    dwcssi_config_eeprom(bank, len);
    dwcssi_tx(bank, cmd);

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


int dwcssi_wr_flash_reg(struct flash_bank *bank, uint8_t cmd, uint8_t sr1, uint8_t cr1)
{
    uint8_t flash_sr;
    // uint32_t flash_cr=0;
    // LOG_INFO("dwcssi wr flash sr %x cr %x", sr1, cr1);

    dwcssi_txwm_wait(bank);
    dwcssi_flash_wr_en(bank, SPI_FRF_X1_MODE);
    dwcssi_wait_flash_idle(bank, 1000, &flash_sr);

    dwcssi_config_tx(bank, SPI_FRF_X1_MODE, 2, 2);
    dwcssi_tx(bank, cmd);
    dwcssi_tx(bank, sr1);
    dwcssi_tx(bank, cr1);

    dwcssi_txwm_wait(bank);

    dwcssi_disable(bank);
    dwcssi_config_SER(bank, 0);
    dwcssi_enable(bank);

    dwcssi_disable(bank);
    dwcssi_config_SER(bank, 1);
    dwcssi_enable(bank);

    dwcssi_wait_flash_idle(bank, 1000, &flash_sr);
    // LOG_INFO("cr val %x", cr_val);
    return ERROR_OK;
}

/*flash basic functions */

// static void dwcssi_flash_quad_en(struct flash_bank *bank)
// {
//     uint32_t flash_cr = 0, quad_en;
//     dwcssi_read_flash_reg(bank, &flash_cr, FLASH_RD_CONFIG_REG_CMD, 1);
//     quad_en = (flash_cr >> 0x1) & 0x1;
//     // LOG_INFO("flash cr %x bit %x", flash_cr, quad_en);
//     if(quad_en == 0)
//     {
//         // LOG_INFO("dwcssi flash en quad");
//         dwcssi_wr_flash_reg(bank, FLASH_WR_CONFIG_REG_CMD, 0x00, flash_cr | 0x2);
//     }

// }

// static void dwcssi_flash_quad_disable(struct flash_bank *bank)
// {
//     uint32_t flash_cr;
//     dwcssi_read_flash_reg(bank, &flash_cr, FLASH_RD_CONFIG_REG_CMD, 1);
//     dwcssi_wr_flash_reg(bank, FLASH_WR_CONFIG_REG_CMD, 0x00, flash_cr & (0x3C));
// }

static int dwcssi_erase_sector(struct flash_bank *bank, unsigned int sector)
{
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;
    uint8_t flash_sr;
    uint32_t offset;
    int retval;

    offset = bank->sectors[sector].offset;

    // dwcssi_disable(bank);
    // dwcssi_config_TXFTLR(bank, 0, 1);
    // dwcssi_enable(bank);
    dwcssi_flash_wr_en(bank, SPI_FRF_X1_MODE);
    dwcssi_config_tx(bank, SPI_FRF_X1_MODE, 0xFF, 4);
    // LOG_INFO("dwcssi erase cmd %x sector %x", dwcssi_info->dev->erase_cmd, offset);

    dwcssi_tx(bank, dwcssi_info->dev->erase_cmd);
    dwcssi_tx(bank, offset >> 24);
    dwcssi_tx(bank, offset >> 16);
    dwcssi_tx(bank, offset >> 8);
    dwcssi_tx(bank, offset);

    dwcssi_txwm_wait(bank);
    retval = dwcssi_wait_flash_idle(bank, DWCSSI_MAX_TIMEOUT, &flash_sr);

    return retval;
}

/*dwc driver*/
static int dwcssi_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{

    struct target *target = bank->target;
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;
    // struct flash_device_op *flash_op = dwcssi_info->dev->flash_op;
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

    if (!(dwcssi_info->probed)) {
        LOG_ERROR("Flash bank not probed");
        return ERROR_FLASH_BANK_NOT_PROBED;
    }

    for (sector = first; sector <= last; sector++) {
        if (bank->sectors[sector].is_protected) {
            LOG_ERROR("Flash sector %u protected", sector);
            return ERROR_FAIL;
        }
    }

    if (dwcssi_info->dev->erase_cmd == 0x00)
        return ERROR_FLASH_OPER_UNSUPPORTED;

    // dwcssi_flash_quad_disable(bank);
    dwcssi_info->dev->quad_dis(bank);
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
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;

    LOG_INFO("dwcssi read x4 cmd %x", dwcssi_info->dev->qread_cmd);
    dwcssi_disable(bank);
    dwcssi_config_CTRLR1(bank, len);
    dwcssi_enable(bank);
    dwcssi_tx(bank, dwcssi_info->dev->qread_cmd);
    dwcssi_tx(bank, offset);
    dwcssi_rx_buf(bank, buffer, len);
    return ERROR_OK;
}

static int dwcssi_read(struct flash_bank *bank, uint8_t *buffer, uint32_t offset, uint32_t count)
{
    uint32_t cur_count, page_size;
    uint32_t page_offset;
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;

    page_size = dwcssi_info->dev->pagesize ? 
                dwcssi_info->dev->pagesize : SPIFLASH_DEF_PAGESIZE;

    page_offset = offset % page_size;

    // dwcssi_flash_quad_en(bank);
    dwcssi_info->dev->quad_en(bank);
    dwcssi_config_rx(bank, SPI_FRF_X4_MODE, 0x3F);
    while(count > 0)
    {
        if(page_offset + count > page_size)
            cur_count = page_size - page_offset;
        else
            cur_count = count;
        dwcssi_read_page(bank, buffer, offset, cur_count);
        LOG_INFO("read addr %x count %x", offset, count);

        page_offset = 0;
        buffer        += cur_count;
        offset        += cur_count;
        count         -= cur_count;
    }

    return ERROR_OK;
}

static int slow_dwcssi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t len)
{
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;
    uint8_t flash_sr;

    // LOG_INFO("dwcssi slow write offset %x len %x", offset, len);
    dwcssi_flash_wr_en(bank, SPI_FRF_X1_MODE);
    dwcssi_config_tx(bank, SPI_FRF_X4_MODE, len, 0x4);
    dwcssi_tx(bank, dwcssi_info->dev->qprog_cmd);
    dwcssi_tx(bank, offset);
    dwcssi_tx_buf(bank, buffer, len);
    return dwcssi_wait_flash_idle(bank, 9000, &flash_sr);
}

static const uint8_t riscv32_bin[] = {
#include "../../../contrib/loaders/flash/dwcssi/build/dwcssi_riscv_32.inc"
};

static const uint8_t riscv64_bin[] = {
#include "../../../contrib/loaders/flash/dwcssi/build/dwcssi_riscv_64.inc"
};

static const uint8_t aarch64_bin[] = {
#include "../../../contrib/loaders/flash/dwcssi/build/dwcssi_aarch_64.inc"
};

static int dwcssi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{
	struct target *target = bank->target;
    uint32_t cur_count, page_size;
    uint32_t page_offset;
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;
    // struct flash_device_op *flash_op = dwcssi_info->dev->flash_op;
	int retval = ERROR_OK;

    LOG_INFO("dwcssi write");

    count = flash_write_boundary_check(bank, offset, count);
    if(flash_sector_check(bank, offset, count)!=ERROR_OK)
        return ERROR_FAIL;

    if(target->state != TARGET_HALTED)
    {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;        
    }

    uint32_t xlen = 0;
    struct working_area *algorithm_wa = NULL;
    struct working_area *data_wa      = NULL;
    const uint8_t *bin;
    size_t bin_size;
    uint64_t start, now, used_time;
    int   loader_target;

    if(strncmp(target_name(target), "riscv", 4) == 0)
    {
        // LOG_INFO("loader on riscv");
        loader_target = RISCV; 
        xlen = riscv_xlen(target);
        if(xlen == 32)
        {
            // LOG_INFO("rv 32");
            bin = riscv32_bin;
            bin_size = sizeof(riscv32_bin);
        } else {
            // LOG_INFO("rv 64");
            bin = riscv64_bin;
            bin_size = sizeof(riscv64_bin);
        }
    }
    else{
        loader_target = ARM;
        xlen = 64;
        bin = aarch64_bin;
        bin_size = sizeof(aarch64_bin);
    }


    uint32_t data_wa_size = 0;

    start = timeval_ms();
	if (target_alloc_working_area(target, bin_size, &algorithm_wa) == ERROR_OK) {
		retval = target_write_buffer(target, algorithm_wa->address,
				bin_size, bin);
		if (retval != ERROR_OK) {
			LOG_ERROR("Failed to write code to " TARGET_ADDR_FMT ": %d",
					algorithm_wa->address, retval);
			target_free_working_area(target, algorithm_wa);
			algorithm_wa = NULL;

		} else {
			data_wa_size = MIN(target_get_working_area_avail(target), count);
			if (data_wa_size < 128) {
				LOG_WARNING("Couldn't allocate data working area.");
				target_free_working_area(target, algorithm_wa);
				algorithm_wa = NULL;
			} else if (target_alloc_working_area(target, data_wa_size, &data_wa) != ERROR_OK) {
				target_free_working_area(target, algorithm_wa);
				algorithm_wa = NULL;
			}
		}
	} else {
		LOG_WARNING("Couldn't allocate %zd-byte working area.", bin_size);
		algorithm_wa = NULL;
	}

    page_size = dwcssi_info->dev->pagesize ? 
                dwcssi_info->dev->pagesize : SPIFLASH_DEF_PAGESIZE;

    page_offset = offset % page_size;
    now = timeval_ms();
    used_time = now - start;
    LOG_DEBUG("loader init time %llx", used_time);
    // dwcssi_flash_quad_en(bank);
    // flash_op->quad_en(bank);
    dwcssi_info->dev->quad_en(bank);

    if (algorithm_wa)
    {
        // LOG_INFO("wa allocate success");
        struct reg_param reg_params[6];
	
        if(loader_target == RISCV)
        {
    	    init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
		    init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[4], "a4", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[5], "a5", xlen, PARAM_OUT);
        }
        else if(loader_target == ARM)
        {
    	    init_reg_param(&reg_params[0], "x0", xlen, PARAM_IN_OUT);
		    init_reg_param(&reg_params[1], "x1", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[2], "x2", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[3], "x3", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[4], "x4", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[5], "x5", xlen, PARAM_OUT);
        }

        while(count > 0)
        {
            start = timeval_ms();
            cur_count = MIN(count, data_wa_size);
			buf_set_u64(reg_params[0].value, 0, xlen, dwcssi_info->ctrl_base);
			buf_set_u64(reg_params[1].value, 0, xlen, page_size);
			buf_set_u64(reg_params[2].value, 0, xlen, data_wa->address);
			buf_set_u64(reg_params[3].value, 0, xlen, offset);
			buf_set_u64(reg_params[4].value, 0, xlen, cur_count);
			buf_set_u64(reg_params[5].value, 0, xlen, dwcssi_info->dev->qprog_cmd);

            retval = target_write_buffer(target, data_wa->address, cur_count, buffer);
			if (retval != ERROR_OK) {
				LOG_DEBUG("Failed to write %d bytes to " TARGET_ADDR_FMT ": %d",
						cur_count, data_wa->address, retval);
				goto err;
			}
			now = timeval_ms();
            used_time = now - start;

            LOG_DEBUG("count %x loader init time %llx", count, used_time);
            LOG_DEBUG("write(ctrl_base=0x%" TARGET_PRIxADDR ", page_size=0x%x, "
					"data_address=0x%" TARGET_PRIxADDR ", offset=0x%" PRIx32
					", count=0x%" PRIx32 "), buffer=%02x %02x %02x %02x %02x %02x ..." PRIx32,
					dwcssi_info->ctrl_base, page_size, data_wa->address, offset, cur_count,
					buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);

            start = timeval_ms();
			retval = target_run_algorithm(target, 0, NULL,
					ARRAY_SIZE(reg_params), reg_params,
					algorithm_wa->address, 0, 10000, NULL);
			if (retval != ERROR_OK) {
				LOG_ERROR("Failed to execute algorithm at " TARGET_ADDR_FMT ": %d",
						algorithm_wa->address, retval);
				goto err;
			}

			uint64_t algorithm_result = buf_get_u64(reg_params[0].value, 0, xlen);
            
			if (algorithm_result != 0) {
			    LOG_DEBUG("Algorithm returned error %llx", algorithm_result);
				retval = ERROR_FAIL;
				goto err;
			}
            now = timeval_ms();
            used_time = now - start;

            LOG_DEBUG("count %x loader execute time %llx", count, used_time);

			buffer += cur_count;
			offset += cur_count;
			count -= cur_count;
        }

    	target_free_working_area(target, data_wa);
		target_free_working_area(target, algorithm_wa);
    }
    else
    {
        while(count > 0) 
        {
            if(page_offset + count > page_size)
                cur_count = page_size - page_offset;
            else
                cur_count = count;

            slow_dwcssi_write(bank, buffer, offset, cur_count);

            page_offset = 0;
            buffer      += cur_count;
            offset      += cur_count;
            count       -= cur_count;
        }
    }

err:
	target_free_working_area(target, data_wa);
	target_free_working_area(target, algorithm_wa);

    return ERROR_OK;
}

static int dwcssi_reset_flash(struct flash_bank *bank)
{
    int flash_err;
    uint8_t flash_sr;
    // uint32_t flash_cr;

    flash_err = dwcssi_wait_flash_idle(bank, 100, &flash_sr);
    // dwcssi_read_flash_reg(bank, &flash_cr, FLASH_RD_CONFIG_REG_CMD, 1);
    if(flash_err != 0)
    {
        // LOG_INFO("Flash Status Error %x", flash_sr);
        dwcssi_config_tx(bank, SPI_FRF_X1_MODE, 1, 0);

        dwcssi_tx(bank, 0xF0);
        if(dwcssi_txwm_wait(bank) != 0)
            return ERROR_TARGET_TIMEOUT;
        dwcssi_wait_flash_idle(bank, 100, &flash_sr);
    }

    // dwcssi_flash_quad_disable(bank);
    return ERROR_OK;
}


int dwcssi_info_init(struct flash_bank* bank, struct dwcssi_flash_bank *dwcssi_info)
{
    const struct dwcssi_target *target_device;
    struct target *target = bank->target;
    // reset probe state
    if(dwcssi_info->probed)
        free(bank->sectors);
    dwcssi_info->probed = false;

    // init ctrl_base
    if(dwcssi_info->ctrl_base == 0)
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

        dwcssi_info->ctrl_base = target_device->ctrl_base;
        LOG_DEBUG("Valid DWCSSI on device %s at address" TARGET_ADDR_FMT, target_device->name, bank->base);
    }
    else
    {
        LOG_DEBUG("Assuming DWCSSI as specified at address " TARGET_ADDR_FMT "with ctrl at" TARGET_ADDR_FMT,
         dwcssi_info->ctrl_base, bank->base);
    }
    return ERROR_OK;
}

static int dwcssi_probe(struct flash_bank *bank)
{
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;
    uint32_t id = 0;


    LOG_INFO("probe bank %d name %s", bank->bank_number, bank->name);
    dwcssi_info_init(bank, dwcssi_info);
    
    qspi_mio_init(bank);
    dwcssi_config_init(bank);

    if(dwcssi_reset_flash(bank) == ERROR_OK)
    {
        // read flash ID
        dwcssi_read_flash_reg(bank, &id, SPIFLASH_READ_ID, 3);
        LOG_INFO("read id: 0x%08x", id);
        if(flash_bank_init(bank, dwcssi_info, id) != ERROR_OK)
            return ERROR_FAIL;
    }
    else
        return ERROR_FAIL;

    dwcssi_info->dev->quad_dis(bank);

    return ERROR_OK;
}


static int dwcssi_auto_probe(struct flash_bank *bank)
{
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;
    if(dwcssi_info->probed)
        return ERROR_OK;
    return dwcssi_probe(bank);
}

static int dwcssi_protect_check(struct flash_bank *bank)
{
    /* Nothing to do. Protection is only handled in SW. */
    
    return ERROR_OK;
}

static int get_dwcssi_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;

    if (!(dwcssi_info->probed)) {
        command_print_sameline(cmd, "\ndwcssi flash bank not probed yet\n");
        return ERROR_OK;
    }

    command_print_sameline(cmd, "\ndwcssi flash information:\n"
            "  Device \'%s\' (ID 0x%08" PRIx32 ")\n",
            dwcssi_info->dev->name, dwcssi_info->dev->device_id);

    return ERROR_OK;
}


const struct flash_driver dwcssi_flash = {
    .name = "dwcssi",
    .flash_bank_command = dwcssi_flash_bank_command,
    .erase = dwcssi_erase,
    .protect = dwcssi_protect,
    .write = dwcssi_write,
    .read = dwcssi_read,
    .probe = dwcssi_probe,
    .auto_probe = dwcssi_auto_probe,
    .erase_check = default_flash_blank_check,
    .protect_check = dwcssi_protect_check,
    .info = get_dwcssi_info,
    .free_driver_priv = default_flash_free_driver_priv
};
