#include "dwcmshc.h"

uint32_t  emmc_wait_fifo(uint32_t *work_area_start)
{
    uint32_t wp = 0, rp = 0;
    while(wp == rp)
    {
        wp = * (volatile uint32_t *) work_area_start;
        rp = *(volatile uint32_t *) (work_area_start + 1);
    }
    return rp;
}

int emmc_poll_int(volatile uint32_t *ctrl_base, uint8_t flag_offset)
{
    uint32_t int_val, clear_reg;
    uint8_t done_flag = 0;
    while(1)
    {
        int_val = reg_read(ctrl_base + NORMAL_ERROR_INT_R);
        done_flag = (int_val >> flag_offset) & 0x1;
        if(done_flag && ((int_val >> 16) ==0))
            break;
    }
    clear_reg = int_val |(1<<flag_offset);
    reg_write(ctrl_base + NORMAL_ERROR_INT_R, clear_reg);
    return 0;
}

void emmc_write_block(volatile uint32_t *ctrl_base, uint32_t offset, const uint32_t *buffer)
{
    uint32_t i, int_val;
    uint8_t done_flag;

    reg_write((ctrl_base + ARGUMENT_R), offset);
    reg_write((ctrl_base + XFER_CMD_R), WR_SINGLE_BLK);
    while(1)
    {
        int_val = reg_read(ctrl_base + NORMAL_ERROR_INT_R);
        done_flag = (int_val >> INT_BUF_WR_READY) & 0x1;
        if(done_flag && ((int_val >> 16) ==0))
            break;
    }
    for(i=0; i < BLOCK_SIZE_IN_WORD; i++)
        reg_write(ctrl_base + BUF_DATA_R, *(buffer + i));
    while(1)
    {
        int_val = reg_read(ctrl_base + NORMAL_ERROR_INT_R);
        done_flag = (int_val >> INT_XFER_COMPLETE_OFFSET) & 0x1;
        if(done_flag && ((int_val >> 16) ==0))
            break;
    }
    reg_write(ctrl_base + NORMAL_ERROR_INT_R, int_val | (1<<INT_XFER_COMPLETE_OFFSET));

}

void emmc_read_block(volatile uint32_t *ctrl_base, uint32_t *buffer, uint32_t offset, uint32_t word_cnt)
{
    uint32_t i;

    reg_write((ctrl_base + ARGUMENT_R), offset);
    reg_write((ctrl_base + XFER_CMD_R), RD_SINGLE_BLK);
    emmc_poll_int(ctrl_base, INT_BUF_RD_READY);

    for(i=0; i < word_cnt; i++)
        *(buffer + i) = reg_read(ctrl_base + BUF_DATA_R);
    emmc_poll_int(ctrl_base, INT_XFER_COMPLETE_OFFSET);
}

