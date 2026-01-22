#include "dwcmshc.h"

// void emmc_dwcmshc(volatile uint32_t *ctrl_base, int block_cnt, uint32_t *work_area_start, uint32_t *work_area_end, uint32_t block_addr)
void emmc_dwcmshc(volatile uint32_t *ctrl_base, int32_t block_size, int count, uint32_t *buf_start, uint32_t *buf_end, uint32_t block_addr)
{
    uint32_t* rp;
    while(count > 0)
    {
        rp = (uint32_t*) emmc_wait_fifo(buf_start);
        // wrap rp when reaches workarea end
        emmc_write_block(ctrl_base, block_addr, rp);

        block_addr += 1;
        rp += (block_size>>2);

        if(rp == buf_end)
            rp = buf_start + 2;
        // store rp
        *(buf_start + 1) = (uint32_t) rp;
        // update count
        count -= block_size;
    }
}


