#include "dwcssi.h"
#include "dwcssi_flash.h"

int flash_id_parse(struct dwcssi_flash_bank *dwcssi_info,  uint32_t* id)
{
    dwcssi_info->dev = NULL;

    dwcssi_info->dev = NULL;
    for(const struct flash_device *p = flash_devices; p->name; p++)
    {
        if(p->device_id == *id) {
            LOG_INFO("flash finded id %x name %s", *id, p->name);
            dwcssi_info->dev = p;
            dwcssi_info->probed = true;
            break;
        }
    }

    if(!dwcssi_info->dev)
    {
        LOG_ERROR("Unknown flash device (ID 0x%08" PRIx32 ")", *id);
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
    if(dwcssi_info->dev->size_in_bytes <= 0x200000)
    {
        dwcssi_info->addr_len = ADDR_L24;
    }
    else
    {
        dwcssi_info->addr_len = ADDR_L32;
    }

    // LOG_INFO("init addr len %d", dwcssi_info->addr_len << 2);

    bank->sectors = sectors;
    dwcssi_info->probed = true;
    return ERROR_OK;
}


int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t *id)
{

    int retval;

    retval = flash_id_parse(dwcssi_info, id);

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

// int flash_status_err(uint8_t status)
// {
//     uint8_t err_bits = 0;
//     int fail_flag = 0;

//     err_bits = FLASH_STATUS_ERR(status);

//     fail_flag = (err_bits != 0); 

//     return fail_flag;
// }


