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
