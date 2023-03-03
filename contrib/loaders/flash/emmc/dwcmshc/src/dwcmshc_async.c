#include "dwcmshc.h"

void emmc_dwcmshc(volatile uint32_t *ctrl_base, int block_cnt, uint32_t *work_area_start, uint32_t *work_area_end, uint32_t block_addr)
{
    uint32_t* rp;
    while(block_cnt > 0)
    {
        rp = (uint32_t*) emmc_wait_fifo(work_area_start);
        // wrap rp when reaches workarea end
        emmc_write_block(ctrl_base, block_addr, rp);

        block_addr += 1;
        rp += BLOCK_SIZE_IN_WORD;

        if(rp == work_area_end)
            rp = work_area_start + 2;
        // store rp
        *(work_area_start + 1) = (uint32_t) rp;
        // update count
        block_cnt -= 1;
    }

}


