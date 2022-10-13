/*
 * Author: Tianyi Wang (tywang@anlogic.com)
 * Date:  2022-04-30
 * Modified By: Tianyi Wang (tywang@anlogic.com>)
 * Last Modified: 2022-04-30
 */

// flash support for dwcssi controller
#include "dwcssi.h"
#include "dwcssi_flash.h"


int s25fl256s_sector_init(struct flash_bank* bank, struct dwcssi_flash_bank *dwcssi_info)
{
    uint32_t sectorsize;
    unsigned int sector;
    struct flash_sector *sectors;

    sectorsize = dwcssi_info->dev->sectorsize ? dwcssi_info->dev->sectorsize : dwcssi_info->dev->size_in_bytes;
    bank->num_sectors = 0x1FE;
    bank->size = sectorsize * bank->num_sectors;

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
    }

    bank->sectors = sectors;
    dwcssi_info->probed = true;
    return ERROR_OK;
}

int default_sector_init(struct flash_bank *bank, struct dwcssi_flash_bank *dwcssi_info)
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


int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t id)
{

    int retval;

    retval = flash_probe(dwcssi_info, id);

    if(retval == ERROR_OK)
    {
        switch(id) {
            // case 0x00190201:
            //     s25fl256s_sector_init(bank, dwcssi_info);
            //     break;
            default:
                default_sector_init(bank, dwcssi_info);
                break;
        }
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

