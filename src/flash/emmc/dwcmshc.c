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
	uint8_t io_location;
	if (CMD_ARGC != 4)
		return ERROR_COMMAND_SYNTAX_ERROR;

	dwcmshc_emmc = malloc(sizeof(struct dwcmshc_emmc_controller));
	if (!dwcmshc_emmc) {
		LOG_ERROR("no memory for emmc controller");
		return ERROR_FAIL;
	}

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], base);
	COMMAND_PARSE_NUMBER(u8, CMD_ARGV[3], io_location);
	emmc->controller_priv = dwcmshc_emmc;
	dwcmshc_emmc->probed = false;
	dwcmshc_emmc->io_location = io_location;
	dwcmshc_emmc->ctrl_base = base;
	dwcmshc_emmc->flash_loader.dev_info = (struct dwcmshc_emmc_controller *) dwcmshc_emmc;
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

	status = dwcmshc_mio_init(emmc);
	dwcmshc_emmc_ctl_init(emmc);
	dwcmshc_emmc_interrupt_init(emmc);

	status = dwcmshc_emmc_card_init(emmc, in_field);
	if (status != ERROR_OK)
		return ERROR_FAIL;

	status = dwcmshc_emmc_rd_ext_csd(emmc, in_field + 8);

	dwcmshc_emmc_set_clk_ctrl(emmc, MMC_CC_CLK_CARD_OPER, 1);

	dwcmshc_emmc_set_bus_width(emmc);

	dwcmshc_fast_mode(emmc);
	return status;
}


static int dwcmshc_emmc_reset(struct emmc_device *emmc)
{
	return dwcmshc_emmc_cmd_reset(emmc, EMMC_CMD0_PARA_GO_IDLE_STATE);
}

static int dwcmshc_emmc_write_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr)
{
	int retval;

	retval = slow_dwcmshc_emmc_write_block(emmc, buffer, addr);
	return retval;
}


static int dwcmshc_emmc_write_image(struct emmc_device* emmc, uint8_t *buffer, uint32_t addr, int size)
{
	int retval = ERROR_OK;

	retval = dwcmshc_emmc_async_write_image(emmc, buffer, addr, size);

	if (retval != ERROR_OK) {
		LOG_ERROR("async write fail, try sync write");
		retval = dwcmshc_emmc_sync_write_image(emmc, buffer, addr, size);
	}

	return retval;
}

static int dwcmshc_emmc_read_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr)
{
	slow_dwcmshc_emmc_read_block(emmc, buffer, addr);
	return ERROR_OK;
}

static int find_difference(struct emmc_device *emmc, const uint8_t *buffer, uint32_t size, uint32_t offset)
{
	if (size == 1)
		return offset;

	uint32_t half = size/2;
	uint32_t checksum_image, checksum_target;

	LOG_INFO("find difference size %x offset %x", size, offset);
	image_calculate_checksum(buffer, half, &checksum_image);
	LOG_INFO("checksum image %x", checksum_image);
	dwcmshc_checksum(emmc, buffer, offset, half, &checksum_target);
	LOG_INFO("checksum target %x", checksum_target);
	LOG_DEBUG("find difference size %x offset %x image %x target %x", half, offset, checksum_image, checksum_target);
	if (checksum_image != checksum_target)
		return find_difference(emmc, buffer, half, offset);
	else
		return find_difference(emmc, buffer + half, size - half, offset + half);
}

static int dwcmshc_emmc_verify(struct emmc_device *emmc, const uint8_t *buffer, uint32_t addr, uint32_t count)
{
	int retval = ERROR_OK;
	uint32_t target_crc = 0, image_crc;
	uint32_t fail_location;

	retval = image_calculate_checksum(buffer, count, &image_crc);
	if (retval != ERROR_OK)
		return retval;

	retval = dwcmshc_checksum(emmc, buffer, addr, count, &target_crc);
	if (retval != ERROR_OK)
		return retval;

	if (~image_crc != ~target_crc) {
		LOG_ERROR("checksum image %x target %x", image_crc, target_crc);
		fail_location = find_difference(emmc, buffer, count, addr);
		LOG_ERROR("verify failed at %x", fail_location);
		retval = ERROR_FAIL;
	} else {
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
	.write_block_data = dwcmshc_emmc_write_block,
	.read_block_data = dwcmshc_emmc_read_block,
	.verify_image = dwcmshc_emmc_verify,
	.emmc_ready = dwcmshc_emmc_ready,
	.init = dwcmshc_emmc_init,
};

