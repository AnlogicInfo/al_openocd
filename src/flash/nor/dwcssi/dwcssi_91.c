#include "dwcssi.h"
#include "dwcssi_flash.h"

FLASH_BANK_COMMAND_HANDLER(dr91_flash_bank_command)
{
	struct dwcssi_flash_bank *driver_priv;
	target_addr_t base;
	LOG_DEBUG("%s", __func__);

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	driver_priv = malloc(sizeof(struct dwcssi_flash_bank));
	if (!driver_priv) {
		LOG_ERROR("not enough memory");
		return ERROR_FAIL;
	}

	if (CMD_ARGC >= 7) {
		COMMAND_PARSE_ADDRESS(CMD_ARGV[6], base);
		LOG_DEBUG("ASSUMING DWCSSI device at ctrl_base = " TARGET_ADDR_FMT,
				base); 
	}

	bank->driver_priv = driver_priv;

	driver_priv->probed = false;
	driver_priv->ctrl_base = base;
	driver_priv->loader.dev_info = (struct dwcssi_flash_bank *) driver_priv;
	driver_priv->loader.set_params_priv = NULL;
	driver_priv->loader.exec_target = bank->target;
	driver_priv->loader.copy_area  = NULL;
	driver_priv->loader.ctrl_base = base;
	return ERROR_OK;
}

static int dr91_mio_init(struct flash_bank *bank)
{
	struct target *target = bank->target;
	uint8_t mio_num;
	uint32_t value = 0;

	for (mio_num = 0; mio_num < 28; mio_num = mio_num + 4) {
		if (target_read_u32(target, MIO_BASE + mio_num, &value) != ERROR_OK)
			return ERROR_FAIL;
		if (value != 1)	{
			if (target_write_u32(target,  MIO_BASE + mio_num, 1) != ERROR_OK)
				return ERROR_FAIL;
		}

	}

	return ERROR_OK;
}

static int dr91_probe(struct flash_bank *bank)
{
	struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
	int retval = ERROR_FAIL;
	LOG_INFO("probe bank %d name %s", bank->bank_number, bank->name);
	driver_priv_init(bank, driver_priv);

	if (dr91_mio_init(bank) != ERROR_OK) {
		return ERROR_FAIL;
	}
	dwcssi_config_init(bank, 20);
    retval = dwcssi_probe(bank);

    return retval;
}

static int dr91_auto_probe(struct flash_bank *bank)
{
	struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
	if (driver_priv->probed)
		return ERROR_OK;
	return dr91_probe(bank);

}


const struct flash_driver dr91_flash = {
	.name = "dwcssi_91",
	.flash_bank_command = dr91_flash_bank_command,
	.erase = dwcssi_erase,
	.protect = dwcssi_protect,
	.write = dwcssi_write,
	.read = dwcssi_read,
	.verify = dwcssi_verify,
	.xip_init = dwcssi_xip_init,
	.reset = dwcssi_flash_reset,
	.probe = dr91_probe,
	.customize = dwcssi_customize,
	.auto_probe = dr91_auto_probe,
	.erase_check = default_flash_blank_check,
	.protect_check = dwcssi_protect_check,
	.info = get_driver_priv,
	.free_driver_priv = default_flash_free_driver_priv
};

