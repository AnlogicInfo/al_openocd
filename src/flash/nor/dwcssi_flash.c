/*
 * Author: Tianyi Wang (tywang@anlogic.com)
 * Date:  2022-04-30
 * Modified By: Tianyi Wang (tywang@anlogic.com>)
 * Last Modified: 2022-04-30
 */

// flash support for dwcssi controller
#include "dwcssi.h"
#include "dwcssi_flash.h"

int sp_flash_reset(struct flash_bank *bank)
{
    int flash_err;
    uint8_t flash_sr;
    // uint32_t flash_cr;

    LOG_INFO("reset sp flash");
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

    return ERROR_OK;
}

int sp_flash_quad_disable(struct flash_bank* bank)
{
    uint32_t flash_cr;
    dwcssi_read_flash_reg(bank, &flash_cr, FLASH_RD_CONFIG_REG_CMD, 1);
    dwcssi_wr_flash_reg(bank, FLASH_WR_CONFIG_REG_CMD, 0x00, flash_cr & (0x3C));

    return ERROR_OK;
}

int sp_flash_quad_enable(struct flash_bank* bank)
{
    uint32_t flash_cr = 0, quad_en;
    dwcssi_read_flash_reg(bank, &flash_cr, FLASH_RD_CONFIG_REG_CMD, 1);
    quad_en = (flash_cr >> 0x1) & 0x1;
    // LOG_INFO("flash cr %x bit %x", flash_cr, quad_en);
    if(quad_en == 0)
    {
        // LOG_INFO("dwcssi flash en quad");
        dwcssi_wr_flash_reg(bank, FLASH_WR_CONFIG_REG_CMD, 0x00, flash_cr | 0x2);
    }

    return ERROR_OK;
}

int flash_probe(struct dwcssi_flash_bank *dwcssi_info,  uint32_t id)
{

    dwcssi_info->dev = NULL;
    for(const struct flash_device *p = flash_devices; p->name; p++)
    {
        if(p->device_id == id) {
            dwcssi_info->dev = p;
            break;
        }
    }

    if(!dwcssi_info->dev)
    {
        LOG_ERROR("Unknown flash device (ID 0x%08" PRIx32 ")", id);
        return ERROR_FAIL;
    }

    return ERROR_OK;
}



int flash_sector_init(struct flash_bank *bank, struct dwcssi_flash_bank *dwcssi_info)
{
    uint32_t sectorsize;
    unsigned int sector;
    struct flash_sector *sectors;

    bank->size = dwcssi_info->dev->size_in_bytes;
    sectorsize = dwcssi_info->dev->sectorsize ? dwcssi_info->dev->sectorsize : dwcssi_info->dev->size_in_bytes;
    bank->num_sectors = dwcssi_info->dev->size_in_bytes / sectorsize;
    sectors = malloc(sizeof(struct flash_sector) * bank->num_sectors);

    if(!sectors) {
        LOG_ERROR("not enough memory");
        return ERROR_FAIL;
    }    
    for(sector = 0; sector < bank->num_sectors; sector++)
    {
        sectors[sector].offset = sector * sectorsize;
        sectors[sector].size = sectorsize;
        sectors[sector].is_erased = -1;
        sectors[sector].is_protected = 0;
        // LOG_INFO("sector %x offset %x", sector, sectors[sector].offset);
    }

    bank->sectors = sectors;
    dwcssi_info->probed = true;
    return ERROR_OK;
}


int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t id)
{

    int retval;

    retval = flash_probe(dwcssi_info, id);

    if(retval == ERROR_OK)
    {
        flash_sector_init(bank, dwcssi_info);
    }

    return retval;
}

int flash_sector_check(struct flash_bank *bank, uint32_t offset, uint32_t count)
{
    unsigned int sector;
    for(sector = 0; sector < bank->num_sectors; sector++)
    {
        if((offset < (bank->sectors[sector].offset + bank->sectors[sector].size))
            && ((offset + count - 1) >= bank->sectors[sector].offset ))
            break;
        else 
            continue;
    }

    if(sector == bank->num_sectors)
    {
        LOG_ERROR("Flash offset %x oversize", offset);
        return ERROR_FAIL;
    }
    else
    {
        if(bank->sectors[sector].is_protected)
        {
            LOG_ERROR("Flash sector %u protected", sector);
            return ERROR_FAIL;
        }
    }
    return ERROR_OK;
}

uint32_t flash_write_boundary_check(struct flash_bank *bank, uint32_t offset, uint32_t count)
{
    uint32_t actual_count;

    if(offset + count > bank->size)
    {
        LOG_WARNING("Write exceeds flash boundary. Discard extra data");

        actual_count = bank->size - offset;
    }
    else{
        actual_count = count;
    }

    return actual_count;
}

int flash_check_status(uint8_t status)
{
    uint8_t err_bits = 0;
    int fail_flag = 0;

    err_bits = FLASH_STATUS_ERR(status);

    fail_flag = (err_bits != 0); 

    return fail_flag;
}

uint8_t flash_check_wp(uint8_t status)
{
    uint8_t wp_bits, wp_flag = 0;
    wp_bits = FLASH_STATUS_WP(status);
    if(wp_bits != 0)
        wp_flag = 1;

    return wp_flag;
}

uint8_t flash_quad_mode(uint8_t config_reg)
{
    uint8_t quad_bit;
    quad_bit = FLASH_CONFIG_QUAD(config_reg);
    return quad_bit;
}

