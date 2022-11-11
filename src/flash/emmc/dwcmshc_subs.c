/*
 * File: dwcmshc_subs.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-11-08
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-11-08
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include "dwcmshc_subs.h"
#include <target/target.h>

static int dwcmshc_wait_clk(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 

    int64_t start = timeval_ms();
    CLK_CTRL_R clk_ctrl;
    clk_ctrl.d16 = 0;
    while(1)
    {
        target_read_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_CLK_CTRL_R, &clk_ctrl.d16);
        if(clk_ctrl.bit.internal_clk_stable)
            break;
        int64_t now = timeval_ms();
        if((now-start) > TIMEOUT_5S)
            return ERROR_TIMEOUT_REACHED;    
    }

    return ERROR_OK;
}

static int dwcmshc_emmc_wait_reg_u32(struct emmc_device *emmc, target_addr_t addr, uint32_t val, uint32_t mask, uint64_t timeout)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
    int64_t start = timeval_ms();
    uint32_t rd_val = 0;

    while(1)
    {
        target_read_u32(target, dwcmshc_emmc->ctrl_base + addr, &rd_val);
        if((rd_val & mask) == val)
            break;

        int64_t now = timeval_ms();
        if((now - start) > timeout)
            return ERROR_TIMEOUT_REACHED;
    }
    return ERROR_OK;

}


static int dwcmshc_emmc_wait_reg_u16(struct emmc_device *emmc, target_addr_t addr, uint16_t val, uint16_t mask, uint64_t timeout)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
    int64_t start = timeval_ms();
    uint32_t rd_val = 0;

    while(1)
    {
        target_read_u16(target, dwcmshc_emmc->ctrl_base + addr, &rd_val);
        if((rd_val & mask) == val)
            break;

        int64_t now = timeval_ms();
        if((now - start) > timeout)
            return ERROR_TIMEOUT_REACHED;
    }
    return ERROR_OK;

}

static int dwcmshc_emmc_check_pstate(struct emmc_device *emmc, uint32_t expect_val, uint32_t mask)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
    int64_t start = timeval_ms();
    PSTATE_REG_R pstate;

    while(1)
    {
        target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_PSTATE_REG, &pstate.d32);
        if((pstate.d32 & mask) == expect_val)
            break;

        int64_t now = timeval_ms();
        if((now - start)> TIMEOUT_1S)
            return ERROR_TIMEOUT_REACHED;
    }
    return ERROR_OK;
}


static int dwcmshc_emmc_clear_status(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 

    target_write_u32(target, (dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_R), ~0);
}

static int dwcmshc_emmc_command_prepare(struct emmc_device *emmc)
{
    PSTATE_REG_R pstate_val, pstate_mask;

    pstate_val.bit.cmd_inhibit = 0;
    pstate_val.bit.cmd_inhibit_dat = 0;
    pstate_mask.bit.cmd_inhibit = 1;
    pstate_mask.bit.cmd_inhibit_dat = 1;

    dwcmshc_emmc_wait_reg_u32(emmc, OFFSET_PSTATE_REG, pstate_val.d32, p_state_mask.d32, MMC_CHECK_LINE_INHIBIT_TIMEOUT_VAL);
    dwcmshc_emmc_clear_status(emmc);
}


static int dwcmshc_emmc_set_host_ctrl(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 

    HOST_CTRL1_R host_ctrl;
    host_ctrl.d8 = 0;
    host_ctrl.bit.dma_sel = MMC_HC1_DMA_SEL_ADMA2;
    target_write_u8(target, dwcmshc_emmc->ctrl_base + OFFSET_HOST_CTRL1_R, host_ctrl.d8);
    return ERROR_OK;
}


static int dwcmshc_emmc_set_pwr_ctrl(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
    PWR_CTRL_R pwr_ctrl;
    // vdd1 pwr on support
    target_read_u32(target, IO_BANK1_REF, &dwcmshc_emmc->io_bank_pwr);
    if(dwcmshc_emmc->io_bank_pwr == 1)
        pwr_ctrl.bit.sd_bus_pwr_vdd1 = EMMC_PC_SBV_VDD1_1V8;
    else
        pwr_ctrl.bit.sd_bus_pwr_vdd1 = MMC_PC_SBV_VDD1_3V3;
    target_write_u8(target, dwcmshc_emmc->ctrl_base + OFFSET_PWR_CTRL_R, pwr_ctrl.d8);
    return ERROR_OK;
}


static int dwcmshc_emmc_set_clk_ctrl(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 

    CLK_CTRL_R clk_ctrl;
    clk_ctrl.d16 = 0;
    clk_ctrl.bit.internal_clk_en = MMC_CC_INTER_CLK_ENABLE;
    if(dwcmshc_wait_clk(target, dwcmshc_emmc) != ERROR_OK)
        return ERROR_TIMEOUT_REACHED;
    clk_ctrl.bit.sd_clk_en = MMC_CC_SD_CLK_ENABLE;
    if(dwcmshc_wait_clk(target, dwcmshc_emmc) != ERROR_OK)
        return ERROR_TIMEOUT_REACHED;
    clk_ctrl.bit.pll_enable = MMC_CC_PLL_ENABLE;
    target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_CLK_CTRL_R, clk_ctrl.d16);
    if(dwcmshc_wait_clk(target, dwcmshc_emmc) != ERROR_OK )
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
}

static int dwcmshc_emmc_ctl_init(struct emmc_device *emmc)
{
    dwcmshc_emmc_set_host_ctrl(emmc);
    dwcmshc_emmc_set_pwr_ctrl(emmc);
    dwcmshc_emmc_set_clk_ctrl(emmc);
    dwcmshc_emmc_set_tout(emmc);

    return ERROR_OK;
}



static int dwcmshc_emmc_interrupt_init(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 


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

    target_write_u32(target, (dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_EN_R), ((error_int_stat_en.d16 << 16) | normal_int_stat_en.d16));
    return ERROR_OK;
}

static int dwcmshc_emmc_wait_command(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
    int64_t start = timeval_ms();
    NORMAL_INT_STAT_R normal_int_stat;
    ERROR_INT_STAT_R error_int_stat;
    uint32_t status = ERROR_OK;

    while(1)
    {
        target_read_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_R, &normal_int_stat.d16);
        target_read_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_ERROR_INT_STAT_R, &error_int_stat.d16);
        if(error_int_stat.d16 != 0)
        {
            LOG_ERROR("DWCMSHC CMD ERROR %x", error_int_stat.d16);
            status = ERROR_FAIL;
            break;
        }
        else if(normal_int_stat.bit.cmd_complete)
            break;        
        int64_t now = timeval_ms();
        if((now - start) > timeout)
        {
            status = ERROR_TIMEOUT_REACHED;
            break;
        }   
    }

    //clear cmd_complete
    normal_int_stat.bit.cmd_complete = 1;
    target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_R, normal_int_stat.d16);

    return status;

}


static int dwcmshc_emmc_command(struct emmc_device *emmc, dwcmshc_cmd_pkt_t *cmd_pkt)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
    NORMAL_INT_STAT_R normal_int_stat;
    ERROR_INT_STAT_R error_int_stat;
    CMD_R cmd;
    XFER_MODE_R xfer_mode;
    uint32_t status = ERROR_OK;

    cmd.bit.cmd_index = cmd_pkt->cmd_index;
    cmd.bit.resp_type_select = cmd_pkt->resp_len;
    xfer_mode.bit.data_xfer_dir = cmd_pkt->xfer_dir;

    if(cmd_pkt->argu_en)
        target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_ARGUMENT_R, cmd_pkt->argument);
    target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_XFER_MODE_R, ((cmd.d16 << 16) | xfer_mode.d16));

    status = dwcmshc_emmc_wait_command(emmc);

    if(cmd_pkt.resp_len =MMC_C_NO_RESP)

    return status;

}

static void dwcmshc_emmc_cmd_reset(struct emmc_device *emmc, uint32_t argument)
{
    dwcmshc_cmd_pkt_t cmk_pkt;
    meset(&cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
    cmd_pkt.argu_en = ARGU_EN;
    cmd_pkt.argument = argument;
    cmd_pkt.cmd_index = SD_CMD_GO_IDLE_STATE;
    cmd_pkt.xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
    cmd_pkt.resp_len = MMC_C_NO_RESP;
    dwcmshc_emmc_command_prepare(emmc);
    dwcmshc_emmc_command(emmc, &cmd_pkt);
}

static int dwcmshc_emmc_cmd_setop(struct emmc_device *emmc)
{
    dwcmshc_cmd_pkt_t cmk_pkt;

    meset(&cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
    oc.bit.voltage_mode = dwcmshc_emmc->io_bank_pwr & 0x1;
    oc.bit.voltage2v7_3v6 = EMMC_OCR_VOLTAGE_2V7_3V6;
    oc.bit.access_mode = EMMC_OCR_ACCESS_MODE_SECTOR_MODE;
    cmd_pkt.argu_en = ARGU_EN;
    cmd_pkt.argument = oc.d32;
    cmd_pkt.cmd_index = SD_CMD_SEND_OP_COND;
    cmd_pkt.xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
    cmd_pkt.resp_len = MMC_C_RESP_LEN_48;

    while(1)
    {

        dwcmshc_emmc_command_prepare(emmc);
        dwcmshc_emmc_command(emmc, &cmd_pkt);
        target_read_u32(target, OFFSET_RESP01_R, &resp01);
        if((resp01 >> 31) & 0x1)
            break;
        int64_t now = timeval_ms();
        if((now - start) > TIMEOUT_15S)
            return ERROR_TIMEOUT_REACHED;
    }

    return ERROR_OK;
}


static int dwcmshc_emmc_card_init(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
    int64_t start = timeval_ms();
    uint32 resp01 = 0;
    OCR_R oc;

    //reset card
    dwcmshc_emmc_cmd_reset(emmc, EMMC_CMD0_PARA_GO_IDLE_STATE)
    // alive_sleep(10);

    // set op cond



    // get cid
    dwcmshc_emmc_command_prepare(emmc);
    dwcmshc_emmc_command(emmc, ARGU_EN, 0, SD_CMD_ALL_SEND_CID, MMC_XM_DATA_XFER_DIR_READ, MMC_C_RESP_LEN_136);

    return ERROR_OK;
}

static int dwcmshc_emmc_rd_id(struct emmc_device *emmc)
{
    return ERROR_OK;
}

static int dwcmshc_emmc_init(emmc_device *emmc)
{
    dwcmshc_emmc_ctl_init(emmc);
    dwcmshc_emmc_card_init(emmc);
    dwcmshc_emmc_rd_id(emmc);
    return ERROR_OK;
}
