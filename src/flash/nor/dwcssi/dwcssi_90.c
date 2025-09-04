#include "dwcssi.h"
#include "dwcssi_flash.h"

FLASH_BANK_COMMAND_HANDLER(dr90_flash_bank_command)
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


static int dr90_qspi_mio_init_1v8(struct flash_bank *bank)
{
	struct target *target = bank->target;
	uint8_t mio_num;
	uint32_t value = 0;
	uint32_t mio_parm = mio_pad_ctrl0(MIO_SPEED_FAST, MIO_PULL_10K_EN, MIO_PULL_DIS);

	for (mio_num = 0; mio_num < 28; mio_num = mio_num + 4) {
		if (target_read_u32(target, MIO_BASE + mio_num, &value) != ERROR_OK)
			return ERROR_FAIL;
		if (value != 1)	{
			if (target_write_u32(target,  MIO_BASE + mio_num, 1) != ERROR_OK)
				return ERROR_FAIL;
		}

		if(target_write_u32(target, MIO_PARA_BASE + (mio_num << 1), mio_parm)!= ERROR_OK)
			return ERROR_FAIL;
	}

	return ERROR_OK;
}

static int dr90_qspi_mio_init_3v3(struct flash_bank *bank)
{
	struct target *target = bank->target;
	uint8_t mio_num;
	uint32_t value = 0;
	uint32_t mio_parm = 0x88000007;
	uint32_t mio_parm1 = 0x001e0603;

	for (mio_num = 0; mio_num < 28; mio_num = mio_num + 4) {
		if (target_read_u32(target, MIO_BASE + mio_num, &value) != ERROR_OK)
			return ERROR_FAIL;
		if (value != 1)	{
			if (target_write_u32(target,  MIO_BASE + mio_num, 1) != ERROR_OK)
				return ERROR_FAIL;
		}

		if(target_write_u32(target, MIO_PARA_BASE + (mio_num << 1), mio_parm)!= ERROR_OK)
			return ERROR_FAIL;
		
		if(target_write_u32(target, MIO_PARA1_BASE + (mio_num << 1), mio_parm1)!= ERROR_OK)
			return ERROR_FAIL;
	}

	return ERROR_OK;
}

static int cpu_mask_write(struct flash_bank *bank, uint32_t addr, uint32_t mask, uint32_t value)
{
	uint32_t reg_val;
	if (target_read_u32(bank->target, addr, &reg_val) != ERROR_OK)
		return ERROR_FAIL;
	reg_val = (reg_val & ~mask) | (value & mask);
	LOG_DEBUG("cpu mask write addr %x mask %x value %x", addr, mask, reg_val);
	if (target_write_u32(bank->target, addr, reg_val) != ERROR_OK)
		return ERROR_FAIL;
	return ERROR_OK;
}

static int cpu_pll_waitlock(struct flash_bank *bank)
{
	uint32_t pll_state0;
	int64_t start = timeval_ms();
	while (1) {
		if (target_read_u32(bank->target, CPUPLL_STATE0, &pll_state0) != ERROR_OK)
			return ERROR_FAIL;
		if (pll_state0 & 0x1)
			break;
		int64_t now = timeval_ms();
		if (now - start > 1000) {
			LOG_ERROR("cpu pll lock timeout");
			return ERROR_TARGET_TIMEOUT;
		}
	}
	return ERROR_OK;
}



static int dr90_clk_reset(struct flash_bank *bank)
{
	int ret = ERROR_OK;
	cpu_mask_write(bank, CLK_SEL, 0x10, 0x10);
	cpu_mask_write(bank, CPU4X_DIV1_PARA, 0x00ffffff, 0x00ffffff);
	cpu_mask_write(bank, CPU4X_DIV2_PARA, 0x00ffffff, 0x00555555);
	cpu_mask_write(bank, CPU4X_DIV4_PARA, 0x00ffffff, 0x00111111);
	cpu_mask_write(bank, CLK_SEL, 0x00000001, 0);
	cpu_mask_write(bank, CLK_SEL, 0x20, 0);
	cpu_mask_write(bank, CPUPLL_CTRL1, 0x00000200, 0x00000200);

	target_write_u32(bank->target, CPUPLL_CTRL9, 0x07311e4f);
	target_write_u32(bank->target, CPUPLL_CTRL8, 0x350f0f01);
	target_write_u32(bank->target, CPUPLL_CTRL19, 0x02000002);
	target_write_u32(bank->target, CPUPLL_CTRL18, 0x01000001);

	cpu_mask_write(bank, CPUPLL_CTRL1, 0x00000200, 0x00000000);
	
	ret = cpu_pll_waitlock(bank);
	cpu_mask_write(bank, CLK_SEL, 0x10, 0);
	return ret;
}

static int dr90_mio_init(struct flash_bank *bank)
{
	uint32_t io_bank_ref;    

    target_read_u32(bank->target, MIO_BANK201_REF, &io_bank_ref);

	if(io_bank_ref & 0x1) {
		if (dr90_qspi_mio_init_1v8(bank) != ERROR_OK) {
			return ERROR_FAIL;
		}
	} else if((io_bank_ref & 0x7) == 0x4) {
		if (dr90_qspi_mio_init_3v3(bank) != ERROR_OK) {
			return ERROR_FAIL;
		}
	} else {
		LOG_ERROR("QSPI MIO not configured");
		return ERROR_FAIL;
	}
    
    return ERROR_OK;
}

static void dr90_config_init(struct flash_bank *bank)
{
	uint32_t sckdv, input_clk, io_freq;
	uint32_t io1000_cnt_div, div_qspi;

	target_read_u32(bank->target, IO1000_CNT_DIV, &io1000_cnt_div);
	div_qspi = (io1000_cnt_div & 0x3F);
	input_clk = 1000/(div_qspi+1);
	io_freq = 5;
	sckdv = input_clk/(io_freq * 2);

	LOG_INFO("div_qspi %d input_clk %d io_freq %d sckdv %d", div_qspi, input_clk, io_freq, sckdv);

    dwcssi_config_init(bank, sckdv);

}

static int dr90_probe(struct flash_bank *bank)
{
	struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
    int retval = ERROR_FAIL;

    driver_priv_init(bank, driver_priv);
    dr90_clk_reset(bank);
    dr90_mio_init(bank);
    dr90_config_init(bank);
    retval = dwcssi_probe(bank);

    return retval;
}

static int dr90_auto_probe(struct flash_bank *bank)
{
	struct dwcssi_flash_bank *driver_priv = bank->driver_priv;
	if (driver_priv->probed)
		return ERROR_OK;
	return dr90_probe(bank);
}


const struct flash_driver dr90_flash = {
	.name = "dwcssi_90",
	.flash_bank_command = dr90_flash_bank_command,
	.erase = dwcssi_erase,
	.protect = dwcssi_protect,
	.hw_protect = dwcssi_set_hw_protect,
	.write = dwcssi_write,
	.read = dwcssi_read,
	.verify = dwcssi_verify,
	.xip_init = dwcssi_xip_init,
	.reset = dwcssi_flash_reset,
	.probe = dr90_probe,
	.customize = dwcssi_customize,
	.auto_probe = dr90_auto_probe,
	.erase_check = default_flash_blank_check,
	.protect_check = dwcssi_protect_check,
	.info = get_driver_priv,
	.free_driver_priv = default_flash_free_driver_priv
};
