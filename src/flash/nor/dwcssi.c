/*
* @Author: Tianyi Wang
* @Date:   2022-04-12 10:48:48
* @Last Modified by:   Tianyi Wang
* @Last Modified time: 2022-04-12 13:38:17
*/
/*
The AL9000 QSPI controller is based on DWC_ssi 1.02a
*/

struct dwcssi_flash_bank {
	bool probed;
	target_addr_t ctrl_base;
	const struct flash_device *dev;
}


FLASH_BANK_COMMAND_HANDLER(dwcssi_flash_bank_command)
{
	struct dwcssi_flash_bank_command *dwcssi_info;

	LOG_DEBUG("%s", __func__);

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	dwcssi_info = malloc(sizeof(struct dwcssi_flash_bank));
	if(!dwcssi_info) {
		LOG_ERROR("not enough memory");
		return ERROR_FAIL;
	}

	bank->driver_priv = dwcssi_info;
	dwcssi_info->probed = false;
	dwcssi_info->ctrl_base = 0;
	if (CMD_ARGC >= 7) {
		COMMAND_PARSE_ADDRESS(CMD_ARGV[6], dwcssi_info->ctrl_base);
		LOG_DEBUG("ASSUMING FESPI device at ctrl_base = " TARGET_ADDR_FMT,
				dwcssi_info->ctrl_base);
	}
	return ERROR_OK;
}

static int dwcssi_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{

	
}

const struct flash_driver dwcssi_flash = {
	.name = "dwcssi",
	.flash_bank_command = dwcssi_flash_bank_command,
	.erase = dwcssi_erase,
	.protect = dwcssi_protect,
	.write = dwcssi_write,
	.read = default_dwcssi_read,
	.probe = dwcssi_probe,
	.auto_probe = dwcssi_auto_probe,
	.erase_check = default_dwcssi_blank_check,
	.protect_check = dwcssi_protect_check,
	.info = get_dwcssi_info,
	.free_driver_priv = default_flash_free_driver_priv
};
