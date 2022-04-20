/*
* @Author: Tianyi Wang
* @Date:   2022-04-12 10:48:48
* @Last Modified by:   Tianyi Wang
* @Last Modified time: 2022-04-20 20:16:56
*/
/*
The AL9000 QSPI controller is based on DWC_ssi 1.02a
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include "spi.h"
#include <jtag/jtag.h>
#include <helper/time_support.h>
#include <target/algorithm.h>
#include "target/riscv/riscv.h"


/*Register offsets*/
#define     DWCSSI_REG_CTRLR0                         0x0
#define     DWCSSI_REG_CTRLR1                         0x4
#define     DWCSSI_REG_SSIENR                         0x8
#define     DWCSSI_REG_MWCR                           0xC
#define     DWCSSI_REG_SER                            0x10
#define     DWCSSI_REG_BAUDR                          0x14
#define     DWCSSI_REG_TXFTLR                         0x18
#define     DWCSSI_REG_RXFTLR                         0x1c
#define     DWCSSI_REG_TXFLR                          0x20
#define     DWCSSI_REG_RXFLR                          0x24
#define     DWCSSI_REG_SR                             0x28
#define     DWCSSI_REG_IMR                            0x2C
#define     DWCSSI_REG_ISR                            0x30
#define     DWCSSI_REG_RISR                           0x34
#define     DWCSSI_REG_TXOICR                         0x38
#define     DWCSSI_REG_RXOICR                         0x3c
#define     DWCSSI_REG_RXUICR                         0x40
#define     DWCSSI_REG_MSTICR                         0x44
#define     DWCSSI_REG_ICR                            0x48
#define     DWCSSI_REG_DMACR                          0x4c
#define     DWCSSI_REG_DMATDLR                        0x50
#define     DWCSSI_REG_DMARDLR                        0x54
#define     DWCSSI_REG_IDR                            0x58
#define     DWCSSI_REG_SSIC_VERSION_ID                0x5c
#define     DWCSSI_REG_DRx_START                      0x60
// #define     DWCSSI_REG_DRx[36]                        0x60+i*0x4, i=[0..35]
#define     DWCSSI_REG_RX_SAMPLE_DELAY                0xf0
#define     DWCSSI_REG_SPI_CTRLR0                     0xf4
#define     DWCSSI_REG_DDR_DRIVE_EDGE                 0xf8
#define     DWCSSI_REG_XIP_MODE_BITS                  0xfc

/*Fields*/
#define     DWCSSI_CTRL0_DFS(x)                       (((x) & 0xF) << 0)
#define     DWCSSI_CTRL0_TMOD(x)                      (((x) & 0x3) << 10)
#define     DWCSSI_CTRL0_SPI_FRF(x)                   (((x) & 0x3) << 22)

#define     DWCSSI_CTRL1_NDF(x)                       (((x) & 0xFFFF) << 22)

#define     DWCSSI_SSIC_EN(x)                         (((x) & 0x1) << 0)

#define     DWCSSI_SER(x)                             (((x) & 0x3) << 0)
#define     DWCSSI_BAUDR_SCKDV(x)                     (((x) & 0x7FFF) << 1)

#define     DWCSSI_SR_BUSY(x)                         (((x) & 0x1) << 0)
#define     DWCSSI_ISR_TXEIS(x)                       (((x) & 0x1) << 0)


/*DFS define*/
#define     DFS_BYTE                                  (7)    // 7+1=8 bits=byte
/*TMOD define*/
#define     TX_AND_RX                                 0
#define     TX_ONLY                                   1
#define     RX_ONLY                                   2
#define     EEPROM_READ                               3
/*SPI_FRF define*/
#define     SPI_FRF_X1_MODE                           0
#define     SPI_FRF_X2_MODE                           1
#define     SPI_FRF_X4_MODE                           2
#define     SPI_FRF_X8_MODE                           3

/* Timeout in ms */
#define     DWCSSI_CMD_TIMEOUT                        (100)
#define     DWCSSI_PROBE_TIMEOUT                      (100)
#define     DWCSSI_MAX_TIMEOUT                        (3000)

struct dwcssi_flash_bank {
    bool probed;
    target_addr_t ctrl_base;
    const struct flash_device *dev;
}


FLASH_BANK_COMMAND_HANDLER(dwcssi_flash_bank_command)
{
    struct dwcssi_flash_bank_command *dwcssi_info;

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
    return ERROR_OK;
}

static int dwcssi_write_reg(struct flash_bank *bank, target_addr_t address, uint32_t value)
{
    struct target *target = bank->target;
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;

    int result = target_write_u32(target, dwcssi_info->ctrl_base + address, value);
    if (result != ERROR_OK) {
        LOG_ERROR("dwcssi_write_reg() error writing 0x%" PRIx32 " to " TARGET_ADDR_FMT,
                value, dwcssi_info->ctrl_base + address);
        return result;
    }
    return ERROR_OK;

}

static int dwcssi_set(struct flash_bank *bank, target_addr_t address, uint32_t value, uint32_t bitmask)
{
    uint32_t rd_value, wr_value;
    if(dwcssi_read_reg(bank, &rd, address) != ERROR_OK)
        return ERROR_FAIL;

    wr_value = (rd_value & ~bitmask) | (value & bitmask);

    dwcssi_write_reg(bank, address, wr_value);

}

static int dwcssi_disable(struct flash_bank *bank)
{
    uint32_t ssic_en;
    if(dwcssi_read_reg(bank, &ssic_en, DWCSSI_REG_SSIENR) != ERROR_OK)
        return ERROR_FAIL;
    return dwcssi_write_reg(bank, DWCSSI_REG_SSIENR, ssic_en & ~DWCSSI_SSIC_EN);
}

static int dwcssi_enable(struct flash_bank *bank)
{
    uint32_t ssic_en;
    if(dwcssi_read_reg(bank, &ssic_en, DWCSSI_REG_SSIENR) != ERROR_OK)
        return ERROR_FAIL;
    return dwcssi_write_reg(bank, DWCSSI_REG_SSIENR, ssic_en | DWCSSI_SSIC_EN);

}



static int dwcssi_config(struct flash_bank * bank)
{
    dwcssi_disable(bank);

    dwcssi_set(bank, DWCSSI_REG_CTRLR0, DWCSSI_CTRL0_DFS(DFS_BYTE), DWCSSI_CTRL0_DFS(0xFFFFFFFF));
    dwcssi_set(bank, DWCSSI_REG_CTRLR0, DWCSSI_CTRL0_SPI_FRF(SPI_FRF_X1_MODE), DWCSSI_CTRL0_SPI_FRF(0xFFFFFFFF));
    // dwcssi_set(bank, DWCSSI_REG_CTRLR0, DWCSSI_CTRL0_TMOD(EEPROM_READ), DWCSSI_CTRL0_TMOD(0xFFFFFFFF));

    dwcssi_set(bank, DWCSSI_REG_SER, DWCSSI_SER(1), DWCSSI_SER(0xFFFFFFFF));
    dwcssi_set(bank, DWCSSI_REG_SER, DWCSSI_BAUDR_SCKDV(2), DWCSSI_BAUDR_SCKDV(0xFFFFFFFF));

    dwcssi_enable(bank);

}

static int dwcssi_txwm_wait(struct flash_bank* bank)
{
    int64_t start = timeval_ms();
    while (1) {
        uint32_t ip;
        // if (dwcssi_read_reg(bank, &ip, DWCSSI_REG_SR) != ERROR_OK)
        if (dwcssi_read_reg(bank, &ip, DWCSSI_REG_ISR) != ERROR_OK)
            return ERROR_FAIL;
        if (ip & DWCSSI_ISR_TXEIS)
            break;
        int64_t now = timeval_ms();
        if (now - start > 1000) {
            LOG_ERROR("ip.txeis didn't get set.");
            return ERROR_TARGET_TIMEOUT;
        }
    }

    return ERROR_OK;
}

static int dwcssi_tx(struct flash_bank *bank, uint8_t in)
{
    int64_t start = timeval_ms();
    uint32_t isr;

    while (1) {
        if (dwcssi_read_reg(bank, &isr, DWCSSI_REG_ISR) != ERROR_OK)
            return ERROR_FAIL;
        if (!(isr >> 1))
            break;
        int64_t now = timeval_ms();
        if (now - start > 1000) {
            LOG_ERROR("tx fifo overflow.");
            return ERROR_TARGET_TIMEOUT;
        }
    }

    return dwcssi_write_reg(bank, DWCSSI_REG_DRx_START, in);
}

static int dwcssi_rx(struct flash_bank *bank, uint8_t *out)
{
    int64_t start = timeval_ms();
    uint32_t isr, value;

    while (1) {
        if (dwcssi_read_reg(bank, &isr, DWCSSI_REG_ISR) != ERROR_OK)
            return ERROR_FAIL;
        if (!(isr >> 3))
            break;
        int64_t now = timeval_ms();
        if (now - start > 1000) {
            LOG_ERROR("rxfifo overflow");
            return ERROR_TARGET_TIMEOUT;
        }
    }

    dwcssi_read_reg(bank, &value, DWCSSI_REG_DRx_START);
    if (out)
        *out = value & 0xff;

    return ERROR_OK;
}

static int dwcssi_wait_flash_status(struct flash_bank *bank, int timeout)
{
    int64_t endtime;
    uint8_t rx;
	
	dwcssi_set(bank, DWCSSI_REG_CTRLR0, DWCSSI_CTRL0_TMOD(RX_ONLY), DWCSSI_CTRL0_TMOD(0xFFFFFFFF));
    dwcssi_set(bank, DWCSSI_REG_SER, DWCSSI_SER(1), DWCSSI_SER(0xFFFFFFFF));
    endtime = timeval_ms() + timeout;
    dwcssi_tx(bank, SPIFLASH_READ_STATUS);
    if(dwcssi_rx(bank, NULL) != ERROR_OK)
        return ERROR_FAIL;
    do {
        alive_sleep(1);
        dwcssi_tx(bank, 0);
        if(dwcssi_rx(bank, &rx) != ERROR_OK)
            return ERROR_FAIL;
        if((rx & SPIFLASH_BSY_BIT) == 0) {
        	dwcssi_set(bank, DWCSSI_REG_CTRLR0, DWCSSI_CTRL0_TMOD(TX_ONLY), DWCSSI_CTRL0_TMOD(0xFFFFFFFF));
            return ERROR_OK;
        }
    } while (timeval_ms() < endtime);

    LOG_ERROR("timeout");
    return ERROR_FAIL;
}

static int dwcssi_erase_sector(struct flash_bank *bank, unsigned int first, unsigned int last)
{

}


static int dwcssi_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
    struct dwcssi_flash_bank *dwcssi_info = bank->driver_priv;
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

    for (unsigned int sector = first; sector <= last; sector++) {
        if (bank->sectors[sector].is_protected) {
            LOG_ERROR("Flash sector %u protected", sector);
            return ERROR_FAIL;
        }
    }

    if (dwcssi_info->dev->erase_cmd == 0x00)
        return ERROR_FLASH_OPER_UNSUPPORTED;
}


static int dwcssi_protect(struct flash_bank *bank, int set, unsigned int first, unsigned last)
{

}

static int slow_dwcssi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{

}

static int dwcssi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count)
{

}


static int dwcssi_read_flash_id(struct flash_bank *bank, uint32_t *id)
{
    struct target *target = bank->target;
    int retval;

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
    dwcssi_txwm_wait(bank);

	/* poll WIP */
	retval = dwcssi_wait_flash_status(bank, DWCSSI_PROBE_TIMEOUT);
	if (retval != ERROR_OK)
		return retval;

	dwcssi_config(bank);
	dwcssi_set(bank, DWCSSI_REG_CTRLR0, DWCSSI_CTRL0_TMOD(RX_ONLY), DWCSSI_CTRL0_TMOD(0xFFFFFFFF));

	dwcssi_tx(bank, SPIFLASH_READ_ID);
	dwcssi_tx(bank, 0);
	dwcssi_tx(bank, 0);
	dwcssi_tx(bank, 0);

	/* read ID from Receive Register */
	*id = 0;
	if (dwcssi_rx(bank, NULL) != ERROR_OK)
		return ERROR_FAIL;
	uint8_t rx;
	if (dwcssi_rx(bank, &rx) != ERROR_OK)
		return ERROR_FAIL;
	*id = rx;
	if (dwcssi_rx(bank, &rx) != ERROR_OK)
		return ERROR_FAIL;
	*id |= (rx << 8);
	if (dwcssi_rx(bank, &rx) != ERROR_OK)
		return ERROR_FAIL;
	*id |= (rx << 16);

	dwcssi_set(bank, DWCSSI_REG_CTRLR0, DWCSSI_CTRL0_TMOD(RX_ONLY), DWCSSI_CTRL0_TMOD(0xFFFFFFFF));
	return ERROR_OK;
}

static int dwcssi_probe(struct flash_bank *bank)
{

}

static int dwcssi_auto_probe(struct flash_bank *bank)
{

}

const struct flash_driver dwcssi_flash = {
    .name = "dwcssi",
    .flash_bank_command = dwcssi_flash_bank_command,
    .erase = dwcssi_erase,
    .protect = dwcssi_protect,
    .write = dwcssi_write,
    .read = default_dwcssi_read,
    .probe = dwcssi_probe,
    .auto_probe = dwcssi_auto_probe,
    .erase_check = default_dwcssi_blank_check,
    .protect_check = dwcssi_protect_check,
    .info = get_dwcssi_info,
    .free_driver_priv = default_flash_free_driver_priv
};
