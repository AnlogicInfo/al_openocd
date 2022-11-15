/*
 * File: dwcmshc_subs.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-11-08
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-11-08
 */

#include "dwcmshc_subs.h"


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

static int dwcmshc_emmc_wait_reg_u32(struct emmc_device *emmc, target_addr_t addr, uint32_t val, uint32_t mask, int64_t timeout)
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

// static int dwcmshc_emmc_wait_reg_u16(struct emmc_device *emmc, target_addr_t addr, uint16_t val, uint16_t mask, int64_t timeout)
// {
//     struct target *target = emmc->target;
//     struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
//     int64_t start = timeval_ms();
//     uint16_t rd_val = 0;

//     while(1)
//     {
//         target_read_u16(target, dwcmshc_emmc->ctrl_base + addr, &rd_val);
//         if((rd_val & mask) == val)
//             break;

//         int64_t now = timeval_ms();
//         if((now - start) > timeout)
//             return ERROR_TIMEOUT_REACHED;
//     }
//     return ERROR_OK;
// }

// static int dwcmshc_emmc_check_pstate(struct emmc_device *emmc, uint32_t expect_val, uint32_t mask)
// {
//     struct target *target = emmc->target;
//     struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
//     int64_t start = timeval_ms();
//     PSTATE_REG_R pstate;

//     while(1)
//     {
//         target_read_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_PSTATE_REG, &pstate.d32);
//         if((pstate.d32 & mask) == expect_val)
//             break;

//         int64_t now = timeval_ms();
//         if((now - start)> TIMEOUT_1S)
//             return ERROR_TIMEOUT_REACHED;
//     }
//     return ERROR_OK;
// }


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
        pwr_ctrl.bit.sd_bus_vol_vdd1 = EMMC_PC_SBV_VDD1_1V8;
    else
        pwr_ctrl.bit.sd_bus_vol_vdd1 = MMC_PC_SBV_VDD1_3V3;
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
    if(dwcmshc_wait_clk(emmc) != ERROR_OK)
        return ERROR_TIMEOUT_REACHED;
    clk_ctrl.bit.sd_clk_en = MMC_CC_SD_CLK_ENABLE;
    if(dwcmshc_wait_clk(emmc) != ERROR_OK)
        return ERROR_TIMEOUT_REACHED;
    clk_ctrl.bit.pll_enable = MMC_CC_PLL_ENABLE;
    target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_CLK_CTRL_R, clk_ctrl.d16);
    if(dwcmshc_wait_clk(emmc) != ERROR_OK )
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
    dwcmshc_emmc_set_host_ctrl(emmc);
    dwcmshc_emmc_set_pwr_ctrl(emmc);
    dwcmshc_emmc_set_clk_ctrl(emmc);
    dwcmshc_emmc_set_tout(emmc);

    return ERROR_OK;
}



// static int dwcmshc_emmc_interrupt_init(struct emmc_device *emmc)
// {
//     struct target *target = emmc->target;
//     struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 


//     ERROR_INT_STAT_EN_R error_int_stat_en;
//     NORMAL_INT_STAT_EN_R normal_int_stat_en;

//     error_int_stat_en.d16 = 0;
//     normal_int_stat_en.d16 = 0;

//     normal_int_stat_en.bit.cmd_complete_stat_en = MMC_NORMAL_INT_STAT_EN;
//     normal_int_stat_en.bit.xfer_complete_stat_en = MMC_NORMAL_INT_SIGN_EN;
//     normal_int_stat_en.bit.bgap_event_stat_en = MMC_NORMAL_INT_SIGN_EN;
//     normal_int_stat_en.bit.buf_wr_ready_stat_en = MMC_NORMAL_INT_SIGN_EN;
//     normal_int_stat_en.bit.buf_rd_ready_stat_en = MMC_NORMAL_INT_SIGN_EN;
//     normal_int_stat_en.bit.int_a_stat_en = MMC_NORMAL_INT_SIGN_EN;

//     error_int_stat_en.bit.cmd_tout_err_stat_en = MMC_ERR_INT_SIGN_EN;
//     error_int_stat_en.bit.cmd_crc_err_stat_en = MMC_ERR_INT_SIGN_EN;
//     error_int_stat_en.bit.cmd_end_bit_err_stat_en = MMC_ERR_INT_SIGN_EN;
//     error_int_stat_en.bit.cmd_idx_err_stat_en = MMC_ERR_INT_SIGN_EN;
//     error_int_stat_en.bit.data_tout_err_stat_en = MMC_ERR_INT_SIGN_EN;
//     error_int_stat_en.bit.data_crc_err_stat_en = MMC_ERR_INT_SIGN_EN;
//     error_int_stat_en.bit.data_end_bit_err_stat_en = MMC_ERR_INT_SIGN_EN;

//     target_write_u32(target, (dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_EN_R), ((error_int_stat_en.d16 << 16) | normal_int_stat_en.d16));
//     return ERROR_OK;
// }

static int dwcmshc_emmc_wait_command(struct emmc_device *emmc, bool wait_type)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv; 
    int64_t start = timeval_ms();
    NORMAL_INT_STAT_R normal_int_stat;
    ERROR_INT_STAT_R error_int_stat;
    uint8_t wait_flag = 0;
    uint32_t status = ERROR_OK;

    while(1)
    {
        target_read_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_R, &normal_int_stat.d16);
        target_read_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_ERROR_INT_STAT_R, &error_int_stat.d16);

        if(wait_type == WAIT_CMD_COMPLETE)
            wait_flag = normal_int_stat.bit.cmd_complete;
        else 
            wait_flag = normal_int_stat.bit.xfer_complete;
        if(error_int_stat.d16 != 0)
        {
            LOG_ERROR("DWCMSHC CMD ERROR %x", error_int_stat.d16);
            status = ERROR_FAIL;
            break;
        }
        else if(wait_flag)
            break;        
        int64_t now = timeval_ms();
        if((now - start) > TIMEOUT_10S)
        {
            status = ERROR_TIMEOUT_REACHED;
            break;
        }   
    }

    //clear cmd_complete
    if(wait_type == WAIT_CMD_COMPLETE)
        normal_int_stat.bit.cmd_complete = 1;
    else
        normal_int_stat.bit.xfer_complete = 1;
    target_write_u16(target, dwcmshc_emmc->ctrl_base + OFFSET_NORMAL_INT_STAT_R, normal_int_stat.d16);

    return status;

}

static int dwcmshc_emmc_get_resp(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
    dwcmshc_cmd_pkt_t *cmd_pkt =  &(dwcmshc_emmc->ctrl_cmd);
    switch (cmd_pkt->cmd_reg.bit.resp_type_select)
    {
        case MMC_C_NO_RESP:
            memset(cmd_pkt->resp_buf, 0, sizeof(uint32_t) * 4);
            break;
        case MMC_C_RESP_LEN_48:
        case MMC_C_RESP_LEN_48B:
            target_read_u32(target, OFFSET_RESP01_R, cmd_pkt->resp_buf);
            break;
        case MMC_C_RESP_LEN_136:
            target_read_u32(target, OFFSET_RESP01_R, cmd_pkt->resp_buf);
            target_read_u32(target, OFFSET_RESP23_R, cmd_pkt->resp_buf + 1);
            target_read_u32(target, OFFSET_RESP45_R, cmd_pkt->resp_buf + 2);
            target_read_u32(target, OFFSET_RESP67_R, cmd_pkt->resp_buf + 3);
            break;
        default:
            memset(cmd_pkt->resp_buf, 0, sizeof(uint32_t) * 4);
            break;
    }
    return ERROR_OK;
}

static int dwcmshc_emmc_command(struct emmc_device *emmc)
{
    struct target *target = emmc->target;
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
    dwcmshc_cmd_pkt_t *cmd_pkt = &dwcmshc_emmc->ctrl_cmd;
    uint32_t status = ERROR_OK;

    dwcmshc_emmc_wait_ctl(emmc);

    if(cmd_pkt->argu_en)
        target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_ARGUMENT_R, cmd_pkt->argument);
    target_write_u32(target, dwcmshc_emmc->ctrl_base + OFFSET_XFER_MODE_R, ((cmd_pkt->cmd_reg.d16 << 16) | cmd_pkt->xfer_reg.d16));

    status = dwcmshc_emmc_wait_command(emmc, WAIT_CMD_COMPLETE);

    dwcmshc_emmc_get_resp(emmc);

    return status;

}

static void dwcmshc_emmc_cmd_reset(struct emmc_device *emmc, uint32_t argument)
{
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
    dwcmshc_cmd_pkt_t cmd_pkt = dwcmshc_emmc->ctrl_cmd;

    memset(&cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
    cmd_pkt.argu_en = ARGU_EN;
    cmd_pkt.argument = argument;
    cmd_pkt.cmd_reg.bit.cmd_index = SD_CMD_GO_IDLE_STATE;
    cmd_pkt.cmd_reg.bit.resp_type_select = MMC_C_NO_RESP;
    cmd_pkt.xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
    dwcmshc_emmc_command(emmc);
}

static int dwcmshc_emmc_cmd_setop(struct emmc_device *emmc)
{
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
    dwcmshc_cmd_pkt_t* cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
    OCR_R oc;
    int64_t start= timeval_ms();

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

    while(1)
    {
        dwcmshc_emmc_command(emmc);
        if(cmd_pkt->resp_buf[0] >> 31)
            break;
        int64_t now = timeval_ms();
        if((now - start) > TIMEOUT_15S)
            return ERROR_TIMEOUT_REACHED;
    }

    return ERROR_OK;
}

// static int dwcmshc_emmc_cmd_ra(struct emmc_device *emmc)
// {
//     struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
//     dwcmshc_cmd_pkt_t* cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
//     memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
//     cmd_pkt->argu_en = ARGU_EN;
//     cmd_pkt->argument = EMMC_CMD3_PARA_DEFAULT_VAL;
//     cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SET_REL_ADDR;
//     cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48;
//     cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
//     cmd_pkt->resp_type = EMMC_RESP_R1;
//     dwcmshc_emmc_command(emmc);

//     return ERROR_OK;
// }

// static int dwcmshc_emmc_cmd_csd(struct emmc_device *emmc)
// {
//     struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
//     dwcmshc_cmd_pkt_t* cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);
    
//     memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
//     cmd_pkt->argu_en = ARGU_EN;
//     cmd_pkt->argument = EMMC_CMD3_PARA_DEFAULT_VAL;
//     cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SEND_CSD;
//     cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_136;
//     cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
//     cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
//     cmd_pkt->resp_type = EMMC_RESP_R2;

//     dwcmshc_emmc_command(emmc);
//     return ERROR_OK;
// }

static int dwcmshc_emmc_cmd_cid(struct emmc_device *emmc)
{
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
    dwcmshc_cmd_pkt_t* cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);

    memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
    cmd_pkt->argu_en = ARGU_EN;
    cmd_pkt->argument = EMMC_CMD3_PARA_DEFAULT_VAL;
    cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_ALL_SEND_CID;
    cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_136;
    cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
    cmd_pkt->resp_type = EMMC_RESP_R2;

    dwcmshc_emmc_command(emmc);
    return ERROR_OK;
}

// static int dwcmshc_emmc_cmd_desel(struct emmc_device *emmc)
// {
//     struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
//     dwcmshc_cmd_pkt_t* cmd_pkt = &(dwcmshc_emmc->ctrl_cmd);

//     memset(cmd_pkt, 0, sizeof(dwcmshc_cmd_pkt_t));
//     cmd_pkt->argu_en = ARGU_EN;
//     cmd_pkt->argument = EMMC_CMD3_PARA_DEFAULT_VAL;
//     cmd_pkt->cmd_reg.bit.cmd_index = SD_CMD_SEL_DESEL_CARD;
//     cmd_pkt->cmd_reg.bit.resp_type_select = MMC_C_RESP_LEN_48B;
//     cmd_pkt->cmd_reg.bit.cmd_crc_chk_enable = MMC_C_CMD_CRC_CHECK_ENABLE;
//     cmd_pkt->cmd_reg.bit.cmd_idx_chk_enable = MMC_C_CMD_IDX_CHECK_ENABLE;
//     cmd_pkt->xfer_reg.bit.data_xfer_dir = MMC_XM_DATA_XFER_DIR_READ;
//     dwcmshc_emmc_command(emmc);
//     dwcmshc_emmc_wait_command(emmc, WAIT_XFER_COMPLETE);
//     return ERROR_OK;
// }

int dwcmshc_emmc_card_init(struct emmc_device *emmc)
{
    struct dwcmshc_emmc_controller *dwcmshc_emmc = emmc->controller_priv;
    uint32_t* resp_buf = dwcmshc_emmc->ctrl_cmd.resp_buf;
    int status = ERROR_OK;
    //reset card
    dwcmshc_emmc_cmd_reset(emmc, EMMC_CMD0_PARA_GO_IDLE_STATE);
    // alive_sleep(10);
    // set op cond
    status = dwcmshc_emmc_cmd_setop(emmc);
    if(status!= ERROR_OK)
        return status;
    // get cid
    dwcmshc_emmc_cmd_cid(emmc);

    for(int i=0; i < 4; i++)
    {
        printf("get id index %d val %x\n", i, resp_buf[i]);
    }

    // // get csd
    // dwcmshc_emmc_command_csd(emmc);

    // // sel/desel card
    // dwcmshc_emmc_cmd_desel(emmc);

    return ERROR_OK;
}


