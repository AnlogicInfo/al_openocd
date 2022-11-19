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

#include "dwcmshc_subs.h"



EMMC_DEVICE_COMMAND_HANDLER(dwcmshc_emmc_device_command)
{
    struct dwcmshc_emmc_controller *dwcmshc_emmc;
    uint32_t base;
    if(CMD_ARGC != 3)
        return ERROR_COMMAND_SYNTAX_ERROR;

    dwcmshc_emmc = malloc(sizeof(struct dwcmshc_emmc_controller));
    if(!dwcmshc_emmc)
    {
		LOG_ERROR("no memory for emmc controller");
        return ERROR_FAIL;
    }

    COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], base);
    emmc->controller_priv = dwcmshc_emmc;
    dwcmshc_emmc->probed = false;
    dwcmshc_emmc->ctrl_base = base;

    return ERROR_OK;    
}


static int dwcmshc_emmc_command(struct emmc_device *emmc, uint8_t command)
{

    return ERROR_OK;
}

static int dwcmshc_emmc_read_resp(struct emmc_device *emmc, uint8_t resp_len, uint32_t* resp_buf)
{
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;

    if(resp_len == EMMC_RESP_LEN_48)
    {
        *resp_buf = dwcmshc_emmc->ctrl_cmd.resp_buf[0];
        return ERROR_OK;
    }
    else if(resp_len == EMMC_RESP_LEN_136)
    {
        *resp_buf = dwcmshc_emmc->ctrl_cmd.resp_buf[0];
        *(resp_buf + 1)= dwcmshc_emmc->ctrl_cmd.resp_buf[1];
        *(resp_buf + 2)= dwcmshc_emmc->ctrl_cmd.resp_buf[2];
        *(resp_buf + 3)= dwcmshc_emmc->ctrl_cmd.resp_buf[3];
        return ERROR_OK;
    }
    else
    {
        
        LOG_ERROR("invalid resp len");
        return ERROR_FAIL;
    }


}


static int dwcmshc_emmc_init(struct emmc_device *emmc)
{
    int status = ERROR_OK;
    LOG_INFO("dwcmshc init");
    status = dwcmshc_mio_init(emmc);
    dwcmshc_emmc_ctl_init(emmc);
    dwcmshc_emmc_interrupt_init(emmc);
    dwcmshc_emmc_card_init(emmc);
    return status;
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
    .read_resp = dwcmshc_emmc_read_resp,
    .reset = dwcmshc_emmc_reset,
    .write_block_data = dwcmshc_emmc_write_block,
    .read_block_data = dwcmshc_emmc_read_block,
    .emmc_ready = dwcmshc_emmc_ready,
    .init = dwcmshc_emmc_init,
};

