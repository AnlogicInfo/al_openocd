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
    dwcmshc_emmc->loader.target = emmc->target;
    dwcmshc_emmc->loader.copy_area = NULL;
    dwcmshc_emmc->loader.ctrl_base = base;

    dwcmshc_emmc->flash_loader.dev_info = (struct dwcmshc_emmc_controller*) dwcmshc_emmc;
    dwcmshc_emmc->flash_loader.set_params_priv = NULL;
    dwcmshc_emmc->flash_loader.exec_target = emmc->target;
    dwcmshc_emmc->flash_loader.copy_area = NULL;
    dwcmshc_emmc->flash_loader.ctrl_base = base;

    return ERROR_OK;    
}


static int dwcmshc_emmc_command(struct emmc_device *emmc, uint8_t command, uint32_t argument)
{

    return ERROR_OK;
}

static int dwcmshc_emmc_init(struct emmc_device *emmc, uint32_t* in_field)
{
    int status = ERROR_OK;
    struct target *target = emmc->target;

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    // LOG_INFO("emmc mio init");
    status = dwcmshc_mio_init(emmc);
    // LOG_INFO("emmc ctl init");
    dwcmshc_emmc_ctl_init(emmc);
    // LOG_INFO("emmc interrupt init");
    dwcmshc_emmc_interrupt_init(emmc);

    // LOG_INFO("emmc card init");
    dwcmshc_emmc_card_init(emmc, in_field);

    // LOG_INFO("emmc rd ext csd");
    status = dwcmshc_emmc_rd_ext_csd(emmc, in_field + 8);

    // LOG_INFO("emmc set clk");
    dwcmshc_emmc_set_clk_ctrl(emmc, MMC_CC_CLK_CARD_OPER, 0);
    return status;
}


static int dwcmshc_emmc_reset(struct emmc_device *emmc)
{
    return dwcmshc_emmc_cmd_reset(emmc, EMMC_CMD0_PARA_GO_IDLE_STATE);
}
static int dwcmshc_emmc_write_image(struct emmc_device* emmc, uint32_t *buffer, uint32_t addr, int size)

{
    int retval = ERROR_OK;

    retval = async_dwcmshc_emmc_write_image(emmc, buffer, addr, size);
    if(retval != ERROR_OK)
    {
        LOG_ERROR("async write fail, try sync write");
        retval = fast_dwcmshc_emmc_write_image(emmc, buffer, addr, size);
    }

    return retval;
}

static int dwcmshc_emmc_read_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr)
{
    slow_dwcmshc_emmc_read_block(emmc, buffer, addr);
    return ERROR_OK;
}

static int dwcmshc_emmc_verify(struct emmc_device *emmc, const uint8_t *buffer, uint32_t addr, uint32_t count)
{
    int retval = ERROR_OK;
    uint32_t target_crc = 0, image_crc;

    retval = image_calculate_checksum(buffer, count, &image_crc);
    if(retval != ERROR_OK)
        return retval;

    retval = dwcmshc_checksum(emmc, buffer, addr, count, &target_crc);
    if(retval != ERROR_OK)
        return retval;

    if(~image_crc != ~target_crc)
    {
        LOG_ERROR("checksum image %x target %x", image_crc, target_crc);
        retval = ERROR_FAIL;
    }
    else
    {
        LOG_INFO("checksum %x verify succeeded ", image_crc);
        retval = ERROR_OK;
    }

    return retval;
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
    .write_image = dwcmshc_emmc_write_image,
    .read_block_data = dwcmshc_emmc_read_block,
    .verify_image = dwcmshc_emmc_verify,
    .emmc_ready = dwcmshc_emmc_ready,
    .init = dwcmshc_emmc_init,
};

