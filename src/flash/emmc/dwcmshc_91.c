#include "dwcmshc_subs.h"
#include <string.h>

EMMC_DEVICE_COMMAND_HANDLER(dr91_dwcmshc_emmc_device_command)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc;
	uint32_t base;
	uint8_t io_location;
    if (CMD_ARGC < 4) {
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

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
    dwcmshc_emmc->elf_dir = NULL;
    if (CMD_ARGC >= 5) {
        dwcmshc_emmc->elf_dir = strdup(CMD_ARGV[4]);
    }

	return ERROR_OK;
}


static int dr91_dwcmshc_mio_init(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	target_addr_t mio_addr, emio_addr, mio_base;
	uint32_t mio_val, value = 0, status = ERROR_OK;
	uint8_t mio_num, mio_start, mio_end;

	if (dwcmshc_emmc->io_location == 0) {
		mio_start = 40;
		mio_end = 50;
		mio_base = DR91_201_BASE_ADDR;
		mio_val = 0xb;
		emio_addr = DR91_EMIO_SEL11;
	} else {
		mio_start = 10;
		mio_end = 16;
		mio_base = DR91_BASE_ADDR;
		mio_val = 0xa;
		emio_addr = DR91_EMIO_SEL12;
	}

	for (mio_num = mio_start; mio_num < mio_end; mio_num = mio_num + 1) {
		mio_addr = mio_base + (mio_num << 2);
		status = target_read_u32(target, mio_addr, &value);
		if (status != ERROR_OK)
			return status;
		if (value != mio_val) {
			status = target_write_u32(target,  mio_addr, mio_val);
			if (status != ERROR_OK)
				return status;
		}
		LOG_DEBUG("mio init addr %"TARGET_PRIxADDR " val %x", mio_addr, mio_val);
	}

	status = target_write_u32(target, emio_addr, 0x1);

	LOG_DEBUG("emio init addr %"TARGET_PRIxADDR " val %x", emio_addr, 0x1);

	return status;
}



static int dr91_dwcmshc_fast_mode(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	target_addr_t addr;
	uint32_t status = ERROR_OK, value;
	uint8_t num;
	if(dwcmshc_emmc->io_location == 0) {
		for (num = 40; num < 50; num = num + 1) {
			addr = DR91_MIO_PARM_BASE + (num << 3);
			if(num == 40)
				value = 0x04000007;
			else
				value = 0x05000007;
			status = target_write_u32(target,  addr, value);
			LOG_DEBUG("fast mode addr %"TARGET_PRIxADDR " val %x", addr, value);
			if (status != ERROR_OK)
				return status;
		}
	} else {
		for (num = 10; num < 16; num = num + 1) {
			addr = DR91_FAST_MODE_BASE + (num << 3);
			if(num == 12)
				value = 0x0c000007;
			else
				value = 0x0d000007;
			status = target_write_u32(target,  addr, value);
			LOG_DEBUG("fast mode addr %"TARGET_PRIxADDR " val %x", addr, value);
			if (status != ERROR_OK)
				return status;
		}
	}

	return status;

}


static int dr91_dwcmshc_emmc_init(struct emmc_device *emmc, uint32_t* in_field)
{
	int status = ERROR_OK;
	struct target *target = emmc->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

	status = dr91_dwcmshc_mio_init(emmc);
	dr91_dwcmshc_fast_mode(emmc);

	dwcmshc_emmc_init(emmc, in_field);

	return status;
}

const struct emmc_flash_controller dr91_dwcmshc_emmc_controller = {
    .name = "dwcmshc_91",
    .usage = "bank_id driver target base io [elf_dir]",
    .emmc_device_command = dr91_dwcmshc_emmc_device_command,
    .reset = dwcmshc_emmc_reset,
    .write_image = dwcmshc_emmc_write_image,
    .write_block_data = dwcmshc_emmc_write_block,
    .read_block_data = dwcmshc_emmc_read_block,
    .verify_image = dwcmshc_emmc_verify,
    .emmc_ready = dwcmshc_emmc_ready,
    .init = dr91_dwcmshc_emmc_init,
};
