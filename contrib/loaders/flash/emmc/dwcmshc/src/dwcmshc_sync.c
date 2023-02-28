#include "dwcmshc.h"

void emmc_dwcmshc(volatile uint32_t *ctrl_base, uint32_t offset, const uint32_t *buffer, int size_in_bytes)
{
    uint32_t i, int_val;
    uint8_t done_flag;

    while(size_in_bytes > 0)
    {
        reg_write((ctrl_base + ARGUMENT_R), offset);
        reg_write((ctrl_base + XFER_CMD_R), WR_SINGLE_BLK);
        // poll int val
        while(1)
        {
            int_val = reg_read(ctrl_base + NORMAL_ERROR_INT_R);
            done_flag = (int_val >> INT_BUF_WR_READY) & 0x1;
            if(done_flag && ((int_val >> 16) ==0))
                break;
        }

        reg_write(ctrl_base + NORMAL_ERROR_INT_R, int_val | (1<<INT_BUF_WR_READY));

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

        size_in_bytes -= BLOCK_SIZE;
        offset += BLOCK_SIZE;
        buffer += BLOCK_SIZE_IN_WORD;
    }


}


