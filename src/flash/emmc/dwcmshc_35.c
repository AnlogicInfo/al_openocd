#include "dwcmshc_subs.h"
#include <string.h>

EMMC_DEVICE_COMMAND_HANDLER(dr35_dwcmshc_emmc_device_command)
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


static int dr35_dwcmshc_emmc_init(struct emmc_device *emmc, uint32_t* in_field)
{
	int status = ERROR_OK;
	struct target *target = emmc->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted");
		return ERROR_TARGET_NOT_HALTED;
	}

    if(0) {
	    dwcmshc_emmc_init(emmc, in_field);
    }

	return status;
}

const struct emmc_flash_controller dr35_dwcmshc_emmc_controller = {
    .name = "dwcmshc_35",
    .usage = "bank_id driver target base io [elf_dir]",
    .emmc_device_command = dr35_dwcmshc_emmc_device_command,
    .reset = dwcmshc_emmc_reset,
    .write_image = dwcmshc_emmc_write_image,
    .write_block_data = dwcmshc_emmc_write_block,
    .read_block_data = dwcmshc_emmc_read_block,
    .verify_image = dwcmshc_emmc_verify,
    .erase = dwcmshc_emmc_erase_range,
    .emmc_ready = dwcmshc_emmc_ready,
    .init = dr35_dwcmshc_emmc_init,
};
