/*
 * File: dwcmshc_subs.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-11-08
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-11-08
 */
#include "dwcmshc_subs.h"
int dwcmshc_mio_init(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	target_addr_t mio_addr, emio_addr;
	uint32_t mio_val, value = 0, status = ERROR_OK;
	uint8_t mio_num, mio_start, mio_end;

	if (dwcmshc_emmc->io_location == 0) {
		mio_start = 40;
		mio_end = 50;
		mio_val = 0xb;
		emio_addr = EMIO_SEL11;
	} else {
		mio_start = 10;
		mio_end = 16;
		mio_val = 0xa;
		emio_addr = EMIO_SEL12;
	}

	for (mio_num = mio_start; mio_num < mio_end; mio_num = mio_num + 1) {
		mio_addr = MIO_BASE + (mio_num << 2);
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

	return status;
}

int dwcmshc_fast_mode(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	target_addr_t addr;
	uint32_t status = ERROR_OK;
	uint8_t num;
	for (num = 0; num < 10; num = num + 1) {
		addr = FAST_MODE_BASE + (num << 3);
		status = target_write_u32(target,  addr, 0x88000007);
		if (status != ERROR_OK)
			return status;
	}

	return status;
}

static int dwcmshc_wait_clk(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;

	int64_t start = timeval_ms();
	CLK_CTRL_R clk_ctrl;
	clk_ctrl.d16 = 0;
	while (1) {
		target_read_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_CLK_CTRL_R, &clk_ctrl.d16);
		if (clk_ctrl.bit.internal_clk_stable)
			break;
		int64_t now = timeval_ms();
		if ((now-start) > TIMEOUT_5S)
			return ERROR_TIMEOUT_REACHED;
	}

	return ERROR_OK;
}

static int dwcmshc_emmc_wait_reg_u32(struct emmc_device *emmc, target_addr_t addr, uint32_t val, uint32_t mask, int64_t timeout)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	int64_t start = timeval_ms();
	uint32_t rd_val = 0;

	while (1) {
		target_read_u32(target, dwcmshc_emmc->ctrl_base + addr, &rd_val);
		if ((rd_val & mask) == val)
			break;

		int64_t now = timeval_ms();
		if ((now - start) > timeout)
			return ERROR_TIMEOUT_REACHED;
	}
	return ERROR_OK;

}

static int dwcmshc_emmc_clear_status(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;

	target_write_u32(target, (dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_R), ~0);
	return ERROR_OK;
}

static int dwcmshc_emmc_wait_ctl(struct emmc_device *emmc)
{
	PSTATE_REG_R pstate_val, pstate_mask;

	pstate_val.bit.cmd_inhibit = 0;
	pstate_val.bit.cmd_inhibit_dat = 0;
	pstate_mask.bit.cmd_inhibit = 1;
	pstate_mask.bit.cmd_inhibit_dat = 1;

	dwcmshc_emmc_wait_reg_u32(emmc, OFFSET_PSTATE_REG, pstate_val.d32, pstate_mask.d32, TIMEOUT_1S);
	dwcmshc_emmc_clear_status(emmc);

	return ERROR_OK;
}

static int dwcmshc_emmc_set_pwr_ctrl(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;

	target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_HOST_CTRL1_R, 0x0000f00);
	return ERROR_OK;
}

static int dwcmshc_emmc_set_host_ctrl1(struct emmc_device *emmc, uint32_t val, uint32_t mask)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	uint32_t reg_val = 0;

	target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_HOST_CTRL1_R, &reg_val);
	reg_val &= ~mask;
	reg_val |= val;
	target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_HOST_CTRL1_R, reg_val);
	target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_HOST_CTRL1_R, &reg_val);
	return ERROR_OK;
}

int dwcmshc_emmc_set_clk_ctrl(struct emmc_device *emmc, bool mode, uint32_t div)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	CLK_CTRL_R clk_ctrl;

	/*ctrl clock reset*/
	if (dwcmshc_emmc->io_location == 0) {
		target_write_u32(target, CFG_CTRL_SDIO1, 8);
		target_write_u32(target, CFG_CTRL_SDIO1, 0);
	} else {
		target_write_u32(target, CFG_CTRL_SDIO2, 8);
		target_write_u32(target, CFG_CTRL_SDIO2, 0);
	}

	clk_ctrl.d16 = 0;
	clk_ctrl.bit.clk_gen_select = mode;
	clk_ctrl.bit.freq_sel = div & 0xFF;
	clk_ctrl.bit.upper_freq_sel = (div >> 8) & 0x03;
	clk_ctrl.bit.internal_clk_en = MMC_CC_INTER_CLK_ENABLE;


	target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_CLK_CTRL_R, clk_ctrl.d16);

	if (dwcmshc_wait_clk(emmc) != ERROR_OK)
		return ERROR_TIMEOUT_REACHED;

	clk_ctrl.bit.pll_enable = MMC_CC_PLL_ENABLE;
	target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_CLK_CTRL_R, clk_ctrl.d16);
	if (dwcmshc_wait_clk(emmc) != ERROR_OK)
		return ERROR_TIMEOUT_REACHED;


	clk_ctrl.bit.sd_clk_en = MMC_CC_SD_CLK_ENABLE;
	target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_CLK_CTRL_R, clk_ctrl.d16);
	if (dwcmshc_wait_clk(emmc) != ERROR_OK)
		return ERROR_TIMEOUT_REACHED;

	return ERROR_OK;
}

static int dwcmshc_emmc_set_tout(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;

	TOUT_CTRL_R tout_ctrl;
	tout_ctrl.d8 = 0;
	tout_ctrl.bit.tout_cnt = MMC_TC_TOUT_CNT_2_27;
	target_write_u8(target, dwcmshc_emmc->ctrl_base + OFFSET_TOUT_CTRL_R, tout_ctrl.d8);
	return ERROR_OK;
}

int dwcmshc_emmc_ctl_init(struct emmc_device *emmc)
{
	dwcmshc_emmc_set_pwr_ctrl(emmc);
	dwcmshc_emmc_set_tout(emmc);
	dwcmshc_emmc_set_clk_ctrl(emmc, MMC_CC_CLK_CARD_INIT, 0);

	return ERROR_OK;
}

int dwcmshc_emmc_interrupt_init(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	uint32_t init_val;

	ERROR_INT_STAT_EN_R error_int_stat_en;
	NORMAL_INT_STAT_EN_R normal_int_stat_en;

	error_int_stat_en.d16 = 0;
	normal_int_stat_en.d16 = 0;

	normal_int_stat_en.bit.cmd_complete_stat_en = MMC_NORMAL_INT_STAT_EN;
	normal_int_stat_en.bit.xfer_complete_stat_en = MMC_NORMAL_INT_SIGN_EN;
	normal_int_stat_en.bit.bgap_event_stat_en = MMC_NORMAL_INT_SIGN_EN;
	normal_int_stat_en.bit.buf_wr_ready_stat_en = MMC_NORMAL_INT_SIGN_EN;
	normal_int_stat_en.bit.buf_rd_ready_stat_en = MMC_NORMAL_INT_SIGN_EN;
	normal_int_stat_en.bit.int_a_stat_en = MMC_NORMAL_INT_SIGN_EN;

	error_int_stat_en.bit.cmd_tout_err_stat_en = MMC_ERR_INT_SIGN_EN;
	error_int_stat_en.bit.cmd_crc_err_stat_en = MMC_ERR_INT_SIGN_EN;
	error_int_stat_en.bit.cmd_end_bit_err_stat_en = MMC_ERR_INT_SIGN_EN;
	error_int_stat_en.bit.cmd_idx_err_stat_en = MMC_ERR_INT_SIGN_EN;
	error_int_stat_en.bit.data_tout_err_stat_en = MMC_ERR_INT_SIGN_EN;
	error_int_stat_en.bit.data_crc_err_stat_en = MMC_ERR_INT_SIGN_EN;
	error_int_stat_en.bit.data_end_bit_err_stat_en = MMC_ERR_INT_SIGN_EN;

	init_val = (error_int_stat_en.d16 << 16) | normal_int_stat_en.d16;
	target_write_u32(target, (dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_EN_R), init_val);
	return ERROR_OK;
}

static int dwcmshc_emmc_poll_int(struct emmc_device *emmc, uint8_t flag_offset, int64_t timeout)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	NORMAL_INT_STAT_R normal_int_stat;
	ERROR_INT_STAT_R error_int_stat;
	uint32_t status = ERROR_OK;
	int64_t start = timeval_ms();
	uint16_t clear_reg = 0;
	uint8_t done_flag = 0;

	while (1) {
		target_read_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_R, &normal_int_stat.d16);
		target_read_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_ERROR_INT_STAT_R, &error_int_stat.d16);
		done_flag = (normal_int_stat.d16 >> flag_offset) & 0x1;

		if (error_int_stat.d16 != 0) {
			LOG_ERROR("DWCMSHC INT STAT ERROR %x", error_int_stat.d16);
			status = ERROR_FAIL;
			break;
		} else if (done_flag)
			break;
		int64_t now = timeval_ms();
		if ((now - start) > timeout) {
			status = ERROR_TIMEOUT_REACHED;
			break;
		}
	}

	clear_reg = normal_int_stat.d16 | (((uint16_t)1) << flag_offset);
	target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_R, clear_reg);
	return status;

}

static int dwcmshc_emmc_get_resp(struct emmc_device *emmc)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt =  &(dwcmshc_emmc->ctrl_cmd);
	switch (cmd_pkt->cmd_reg.bit.resp_type_select) {
		case MMC_C_NO_RESP:
			memset(cmd_pkt->resp_buf, 0, sizeof(uint32_t) * 4);
			break;
		case MMC_C_RESP_LEN_48:
		case MMC_C_RESP_LEN_48B:
			target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_RESP01_R, cmd_pkt->resp_buf);
			break;
		case MMC_C_RESP_LEN_136:
			target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_RESP01_R, cmd_pkt->resp_buf);
			target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_RESP23_R, cmd_pkt->resp_buf + 1);
			target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_RESP45_R, cmd_pkt->resp_buf + 2);
			target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_RESP67_R, cmd_pkt->resp_buf + 3);
			break;
		default:
			memset(cmd_pkt->resp_buf, 0, sizeof(uint32_t) * 4);
			break;
	}

	return ERROR_OK;
}

static int dwcmshc_emmc_dump_resp(uint32_t* resp_buf, uint32_t* buf, uint8_t resp_len)
{
	if (resp_len == MMC_C_RESP_LEN_48) {
		buf[0] = resp_buf[0];
	} else if (resp_len == MMC_C_RESP_LEN_136) {
		buf[0] = resp_buf[0];
		buf[1] = resp_buf[1];
		buf[2] = resp_buf[2];
		buf[3] = resp_buf[3];
	} else
		LOG_ERROR("invalid resp len");

	return ERROR_OK;
}

static int dwcmshc_emmc_rd_buf(struct emmc_device *emmc, uint32_t* buf, uint32_t count)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	uint32_t i;

	for (i = 0; i < count; i++)
		target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_BUF_DATA_R, buf+i);

	return dwcmshc_emmc_poll_int(emmc, WAIT_XFER_COMPLETE, TIMEOUT_1S);

}

static int dwcmshc_emmc_wr_buf(struct emmc_device *emmc, uint32_t* buf, uint32_t count)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	uint32_t i;

	for (i = 0; i < count; i++) {
		/* LOG_INFO("wr cnt %x val %x", i, *(buf+i)); */
		target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_BUF_DATA_R, *(buf+i));
	}
	return dwcmshc_emmc_poll_int(emmc, WAIT_XFER_COMPLETE, TIMEOUT_1S);
}

static int dwcmshc_emmc_command(struct emmc_device *emmc, uint8_t poll_flag)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	uint32_t cmd;
	dwcmshc_cmd_pkt_t *cmd_pkt = &dwcmshc_emmc->ctrl_cmd;
	uint32_t status = ERROR_OK;

	dwcmshc_emmc_wait_ctl(emmc);

	LOG_DEBUG("emmc cmd index %d", cmd_pkt->cmd_reg.bit.cmd_index);
	if (cmd_pkt->argu_en) {
		LOG_DEBUG("emmc cmd addr %" PRIx64 " argu %x", dwcmshc_emmc->ctrl_base + OFFSET_ARGUMENT_R, cmd_pkt->argument);
		target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_ARGUMENT_R, cmd_pkt->argument);
	}

	cmd = (cmd_pkt->cmd_reg.d16 << 16) | cmd_pkt->xfer_reg.d16;
	target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_XFER_MODE_R, cmd);
	LOG_DEBUG("emmc cmd addr %" PRIx64 " cmd_xfer %x", dwcmshc_emmc->ctrl_base + OFFSET_XFER_MODE_R, cmd);
	status = dwcmshc_emmc_poll_int(emmc, poll_flag, TIMEOUT_1S);
	dwcmshc_emmc_get_resp(emmc);
	return status;

}

int dwcmshc_emmc_cmd_reset(struct emmc_device *emmc, uint32_t argument)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	uint32_t status = ERROR_OK;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = argument;
	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_GO_IDLE_STATE;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_NO_RESP;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;

	status = dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);
	return status;
}

static int dwcmshc_emmc_cmd_setop(struct emmc_device *emmc)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	int status = ERROR_OK;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	OCR_R oc;
	int64_t start = timeval_ms();

	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	oc.bit.voltage_mode = dwcmshc_emmc->io_bank_pwr & 0x1;
	oc.bit.voltage2v7_3v6 = EMMC_OCR_VOLTAGE_2V7_3V6;
	oc.bit.access_mode = EMMC_OCR_ACCESS_MODE_SECTOR_MODE;

	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = oc.d32;

	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SEND_OP_COND;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;

	cmd_pkt->resp_type = EMMC_RESP_R3;

	while (1) {
		status = dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);
		if (status != ERROR_OK)
			break;
		if (cmd_pkt->resp_buf[0] >> 31)
			break;
		int64_t now = timeval_ms();
		if ((now - start) > TIMEOUT_15S)
			return ERROR_TIMEOUT_REACHED;
	}

	return status;
}

static int dwcmshc_emmc_cmd_ra(struct emmc_device *emmc)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = EMMC_CMD3_PARA_DEFAULT_VAL;

	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SET_REL_ADDR;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;
	cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_idx_chk_enable = MMC_C_CMD_IDX_CHECK_ENABLE;

	cmd_pkt->xfer_reg.bit.resp_err_chk_enable = MMC_XM_RESP_ERR_CHK_ENABLE;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
	cmd_pkt->resp_type = EMMC_RESP_R1;
	dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);

	return ERROR_OK;
}


static int dwcmshc_emmc_cmd_cid(struct emmc_device *emmc, uint32_t* buf)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	int status = ERROR_OK;

	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = EMMC_CMD3_PARA_DEFAULT_VAL;
	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_ALL_SEND_CID;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_136;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
	cmd_pkt->resp_type = EMMC_RESP_R2;

	status = dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);
	if (status != ERROR_OK) {
		LOG_ERROR("emmc cmd cid error");
		return ERROR_FAIL;
	}
	dwcmshc_emmc_dump_resp(cmd_pkt->resp_buf, buf, MMC_C_RESP_LEN_136);
	return status;
}

static int dwcmshc_emmc_cmd6_hs_switch(struct emmc_device *emmc)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	AL_MMC_Cmd6ArgUnion argu;
	int status;
	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));

	argu.Reg = 0;
	argu.Emmc.Index = AL_MMC_CMD6_EMMC_FUNC_BUS_WIDTH;
		/* 1-bit enum 1 >> 2 -> 0b00, 4-bit enum 4 >> 2 -> 0b01, 8-bit enum 8 >> 2 -> 0b10 */
	argu.Emmc.Value  = 4 >> 2;
	argu.Emmc.Access = AL_MMC_CMD6_EMMC_ACCESS_WR_BYTE;
	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = argu.Reg;
	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_HS_SWITCH;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
	status = dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);
	return status;
}

static int dwcmshc_emmc_cmd_desel(struct emmc_device *emmc)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);

	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = EMMC_CMD3_PARA_DEFAULT_VAL;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
	cmd_pkt->xfer_reg.bit.resp_err_chk_enable = MMC_XM_RESP_ERR_CHK_ENABLE;

	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SEL_DESEL_CARD;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48B;
	cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_idx_chk_enable = MMC_C_CMD_IDX_CHECK_ENABLE;

	dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);
	return ERROR_OK;
}



static int dwcmshc_emmc_cmd_ext_csd(struct emmc_device *emmc, uint32_t* buf)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	int retval = ERROR_OK;

	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = 0;

	cmd_pkt->xfer_reg.bit.block_count_enable = MMC_XM_BLOCK_COUNT_ENABLE;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
	cmd_pkt->xfer_reg.bit.resp_err_chk_enable = MMC_XM_RESP_ERR_CHK_ENABLE;

	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;
	cmd_pkt->cmd_reg.bit.data_present_sel = MMC_C_DATA_PRESENT;
	cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_idx_chk_enable = MMC_C_CMD_IDX_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_HS_SEND_EXT_CSD;
	dwcmshc_emmc_command(emmc, WAIT_BUF_RD_READY);
	retval = dwcmshc_emmc_rd_buf(emmc, buf, 128);
	return retval;
}

static int dwcmshc_emmc_cmd_csd(struct emmc_device *emmc, uint32_t* buf)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	int status = ERROR_OK;

	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = EMMC_CMD3_PARA_DEFAULT_VAL;

	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SEND_CSD;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_136;
	cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;

	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
	cmd_pkt->resp_type = EMMC_RESP_R2;

	status = dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);
	if (status != ERROR_OK) {
		LOG_ERROR("emmc csd cmd fail");
		return ERROR_FAIL;
	}
	dwcmshc_emmc_dump_resp(cmd_pkt->resp_buf, buf, MMC_C_RESP_LEN_136);
	return status;
}

static int dwcmshc_emmc_cmd_set_block_length(struct emmc_device *emmc, uint32_t block_len)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));

	/* set controller block length reg */
	target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_BLOCKSIZE_R, block_len);

	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = block_len;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_WRITE;
	cmd_pkt->xfer_reg.bit.resp_err_chk_enable = MMC_XM_RESP_ERR_CHK_ENABLE;

	cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_idx_chk_enable = MMC_C_CMD_IDX_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;
	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SET_BLOCKLEN;

	return dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);
}

static int dwcmshc_emmc_cmd_17_read_single_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t block_addr)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	uint32_t rd_cnt = emmc->device->block_size >> 2;
	int retval = ERROR_OK;

	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = block_addr;

	cmd_pkt->xfer_reg.bit.block_count_enable = MMC_XM_BLOCK_COUNT_ENABLE;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;

	cmd_pkt->xfer_reg.bit.resp_err_chk_enable = MMC_XM_RESP_ERR_CHK_ENABLE;

	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;
	cmd_pkt->cmd_reg.bit.data_present_sel = MMC_C_DATA_PRESENT;
	cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_idx_chk_enable = MMC_C_CMD_IDX_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_READ_SINGLE_BLOCK;
	dwcmshc_emmc_command(emmc, WAIT_BUF_RD_READY);
	retval = dwcmshc_emmc_rd_buf(emmc, buffer, rd_cnt);
	return retval;
}

static int dwcmshc_emmc_cmd_set_block_count(struct emmc_device *emmc, uint32_t block_cnt)
{
	struct target *target = emmc->target;
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));

	target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_BLOCKCOUNT_R, block_cnt);

	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = block_cnt;
	cmd_pkt->xfer_reg.bit.block_count_enable = MMC_XM_BLOCK_COUNT_ENABLE;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_WRITE;
	cmd_pkt->xfer_reg.bit.resp_err_chk_enable = MMC_XM_RESP_ERR_CHK_ENABLE;

	cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_idx_chk_enable = MMC_C_CMD_IDX_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;
	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SET_BLOCK_COUNT;

	return dwcmshc_emmc_command(emmc, WAIT_CMD_COMPLETE);
}

static int dwcmshc_emmc_cmd_24_write_single_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr)
{
	struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;

	dwcmshc_cmd_pkt_t *cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
	memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
	uint32_t wr_cnt = 128;

	cmd_pkt->argu_en = ARGU_EN;
	cmd_pkt->argument = addr/emmc->device->block_size;
	cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_WRITE;
	cmd_pkt->xfer_reg.bit.block_count_enable = MMC_XM_BLOCK_COUNT_ENABLE;
	cmd_pkt->xfer_reg.bit.resp_err_chk_enable = MMC_XM_RESP_ERR_CHK_ENABLE;

	cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;

	cmd_pkt->cmd_reg.bit.data_present_sel = MMC_C_DATA_PRESENT;
	cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_idx_chk_enable = MMC_C_CMD_IDX_CHECK_ENABLE;
	cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_WRITE_SINGLE_BLOCK;
	dwcmshc_emmc_command(emmc, WAIT_BUF_WR_READY);
	dwcmshc_emmc_wr_buf(emmc, buffer, wr_cnt);
	return ERROR_OK;
}



int dwcmshc_emmc_card_init(struct emmc_device *emmc, uint32_t* buf)
{
	int status = ERROR_OK;

	status = dwcmshc_emmc_cmd_reset(emmc, EMMC_CMD0_PARA_GO_IDLE_STATE);
	if (status != ERROR_OK)
		return ERROR_FAIL;

	status = dwcmshc_emmc_cmd_setop(emmc);
	if (status != ERROR_OK)
		return status;

	status = dwcmshc_emmc_cmd_cid(emmc, buf);

	dwcmshc_emmc_cmd_ra(emmc);
	dwcmshc_emmc_cmd_csd(emmc, buf + 4);

	dwcmshc_emmc_cmd_desel(emmc);

	return status;
}


int dwcmshc_emmc_set_bus_width(struct emmc_device *emmc)
{
	uint32_t status = ERROR_OK;

	status = dwcmshc_emmc_cmd6_hs_switch(emmc);
	if (status != ERROR_OK) {
		LOG_ERROR("emmc cmd6 hs switch error");
	}

	dwcmshc_emmc_set_host_ctrl1(emmc, 0x02, 0x02);

	return status;
}

int dwcmshc_emmc_rd_ext_csd(struct emmc_device *emmc, uint32_t* buf)
{
	int retval = ERROR_OK;

	dwcmshc_emmc_cmd_set_block_length(emmc, 512);
	dwcmshc_emmc_cmd_set_block_count(emmc, 1);
	retval = dwcmshc_emmc_cmd_ext_csd(emmc, buf);

	return retval;
}

int slow_dwcmshc_emmc_write_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr)
{
	dwcmshc_emmc_cmd_set_block_length(emmc, 512);
	dwcmshc_emmc_cmd_set_block_count(emmc, 1);
	dwcmshc_emmc_cmd_24_write_single_block(emmc, buffer, addr);
	return ERROR_OK;
}

static const uint8_t riscv32_sync_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_sync_riscv_32.inc"
};

static const uint8_t riscv64_sync_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_sync_riscv_64.inc"
};

static const uint8_t aarch64_sync_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_sync_aarch_64.inc"
};

static const uint8_t riscv32_async_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_async_riscv_32.inc"
};

static const uint8_t riscv64_async_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_async_riscv_64.inc"
};

static const uint8_t aarch64_async_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_async_aarch_64.inc"
};

static struct code_src async_srcs[3] = 
{
	[RV64_SRC] = {riscv64_async_bin, sizeof(riscv64_async_bin)},
	[RV32_SRC] = {riscv32_async_bin, sizeof(riscv32_async_bin)},
	[AARCH64_SRC] = {aarch64_async_bin, sizeof(aarch64_async_bin)},
};

static struct code_src sync_srcs[3] = 
{
	[RV64_SRC] = {riscv64_sync_bin, sizeof(riscv64_sync_bin)},
	[RV32_SRC] = {riscv32_sync_bin, sizeof(riscv32_sync_bin)},
	[AARCH64_SRC] = {aarch64_sync_bin, sizeof(aarch64_sync_bin)},

};


int dwcmshc_emmc_async_write_image(struct emmc_device* emmc, uint8_t *buffer, target_addr_t addr, int image_size)
{
	struct dwcmshc_emmc_controller *driver_priv = emmc->controller_priv;
	struct flash_loader *loader = &driver_priv->flash_loader;
	int block_addr = addr/emmc->device->block_size;
	int retval;
	loader->work_mode = ASYNC_TRANS;
	loader->block_size = emmc->device->block_size;
	loader->image_size = image_size;
	loader->param_cnt = 6;

	dwcmshc_emmc_cmd_set_block_length(emmc, emmc->device->block_size);
	dwcmshc_emmc_cmd_set_block_count(emmc, 1);
	retval = loader_flash_write_async(loader, async_srcs, buffer, block_addr, image_size);
	return retval;
}

int dwcmshc_emmc_sync_write_image(struct emmc_device* emmc, uint8_t *buffer, target_addr_t addr, int image_size)
{
	struct dwcmshc_emmc_controller *driver_priv = emmc->controller_priv;
	struct flash_loader *loader = &driver_priv->flash_loader;
	int retval;

	loader->work_mode = SYNC_TRANS;
	loader->block_size = emmc->device->block_size;
	loader->image_size = image_size;
	loader->param_cnt = 5;

	dwcmshc_emmc_cmd_set_block_length(emmc, emmc->device->block_size);
	dwcmshc_emmc_cmd_set_block_count(emmc, 1);

	retval = loader_flash_write_sync(loader, sync_srcs, buffer, addr, image_size);

	if (retval != ERROR_OK)
		LOG_ERROR("dwcssi write sync error");

	return retval;
}

int slow_dwcmshc_emmc_read_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr)
{
	int retval = ERROR_OK;
	uint32_t block_addr = addr/emmc->device->block_size;
	retval = dwcmshc_emmc_cmd_set_block_length(emmc, 512);
	if (retval != ERROR_OK)
		LOG_ERROR("set block length error");
	retval = dwcmshc_emmc_cmd_set_block_count(emmc, 1);
	if (retval != ERROR_OK)
		LOG_ERROR("set block count error");
	retval = dwcmshc_emmc_cmd_17_read_single_block(emmc, buffer, block_addr);
	if (retval != ERROR_OK)
		LOG_ERROR("read single block addr %x error", block_addr);
	return retval;
}

static const uint8_t riscv32_crc_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_crc_riscv_32.inc"
};

static const uint8_t riscv64_crc_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_crc_riscv_64.inc"
};

static const uint8_t aarch64_crc_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_crc_aarch_64.inc"
};

static struct code_src crc_srcs[3] = 
{
	[RV64_SRC] = {riscv64_crc_bin, sizeof(riscv64_crc_bin)},
	[RV32_SRC] = {riscv32_crc_bin, sizeof(riscv32_crc_bin)},
	[AARCH64_SRC] = {aarch64_crc_bin, sizeof(aarch64_crc_bin)},
};

int dwcmshc_checksum(struct emmc_device *emmc, const uint8_t *buffer, uint32_t addr, uint32_t count, uint32_t* crc)
{
	int retval = ERROR_OK;
	struct dwcmshc_emmc_controller *driver_priv = emmc->controller_priv;
	struct flash_loader *loader = &driver_priv->flash_loader;
	int block_addr = addr/emmc->device->block_size;

	dwcmshc_emmc_cmd_set_block_length(emmc, emmc->device->block_size);
	dwcmshc_emmc_cmd_set_block_count(emmc, 1);

	loader->work_mode = CRC_CHECK;
	loader->block_size = emmc->device->block_size;
	loader->image_size = count;
	loader->param_cnt = 4;

	retval = loader_flash_crc(loader, crc_srcs, block_addr, crc);
	return retval;
}
