#include "dwcssi.h"
#include "dwcssi_flash.h"

static int flash_id_parse(struct dwcssi_flash_bank *dwcssi_info,  uint32_t id)
{
    dwcssi_info->dev = NULL;

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
        flash_sector_init(bank, dwcssi_info);
    }

    return retval;
}
