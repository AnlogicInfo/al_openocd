/*
 * File: dwcmshc.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include "dwcmshc_subs.h"
#include <target/target.h>



EMMC_DEVICE_COMMAND_HANDLER(dwcmshc_emmc_device_command)
{
    struct dwcmshc_emmc_controller *dwcmshc_emmc;

    if(CMD_ARGC < 6)
        return ERROR_COMMAND_SYNTAX_ERROR;

    dwcmshc_emmc = malloc(sizeof(struct dwcmshc_emmc_controller));
    if(!dwcmshc_emmc)
    {
        return ERROR_FAIL;
    }

    emmc->controller_priv = dwcmshc_emmc;
    dwcmshc_emmc->probed = false;
    dwcmshc_emmc->ctrl_base = 0;

    if(CMD_ARGC >= 7) {
        COMMAND_PARSE_ADDRESS(CMD_ARGV[6], dwcmshc_emmc->ctrl_base);
        LOG_DEBUG("ASSUMING DWCMSHC device at ctrl_base = " TARGET_ADDR_FMT, 
            dwcmshc_emmc->ctrl_base);
    }

    return ERROR_OK;    
}


static int dwcmshc_emmc_command(struct emmc_device *emmc, uint8_t command)
{

    return ERROR_OK;
}


static int dwcmshc_emmc_init(struct emmc_device *emmc)
{
    dwcmshc_emmc_ctl_init(emmc);
    dwcmshc_emmc_card_init(emmc);
    dwcmshc_emmc_rd_id(emmc);

    return ERROR_OK;
}

static int dwcmshc_emmc_reset(struct emmc_device *emmc)
{


    return ERROR_OK;
}

static int dwcmshc_emmc_write_block(struct emmc_device *emmc, uint8_t *data, int size)
{
    return ERROR_OK;
}

static int dwcmshc_emmc_read_block(struct emmc_device *emmc, uint8_t *data, int size)
{
    return ERROR_OK;
}

static int dwcmshc_emmc_ready(struct emmc_device *emmc, int timeout)
{
    return ERROR_OK;
}


const struct emmc_flash_controller dwcmshc_emmc_controller = {
    .name = "dwcmshc",
    .emmc_device_command = dwcmshc_emmc_device_command,
    .command = dwcmshc_emmc_command,
    .reset = dwcmshc_emmc_reset,
    .write_block_data = dwcmshc_emmc_write_block,
    .read_block_data = dwcmshc_emmc_read_block,
    .emmc_ready = dwcmshc_emmc_ready,
    .init = dwcmshc_emmc_init,
};

