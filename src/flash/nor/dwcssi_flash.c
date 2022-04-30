/*
 * Author: Tianyi Wang (tywang@anlogic.com)
 * Date:  2022-04-30
 * Modified By: Tianyi Wang (tywang@anlogic.com>)
 * Last Modified: 2022-04-30
 */

// flash support for dwcssi controller
#include "dwcssi.h"

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
        sectors[sector].offset = sector * sectorsize + 0x20000;
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
    }

    bank->sectors = sectors;
    dwcssi_info->probed = true;
    return ERROR_OK;
}

int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t id)
{

    int retval;

    retval = flash_probe(dwcssi_info, id);

    switch(id) {
        case 0x00190201:
            s25fl256s_sector_init(bank, dwcssi_info);
            break;
        default:
            default_sector_init(bank, dwcssi_info);
            break;
    }

    return retval;

}
