#include "dwcssi.h"

int flash_dwcssi(volatile uint32_t *ctrl_base, int32_t block_size, int image_block_cnt, uint32_t *buf_start, uint32_t *buf_end)
{
    uint8_t *rp;
    uint32_t retval;
    while(image_block_cnt > 0)
    {
        rp = (uint8_t *) wait_fifo(buf_start);
        dwcssi_tx_block(ctrl_base, rp, block_size);
        rp += block_size;
        if(rp == buf_end)
            rp = buf_start + 2;
        *(buf_start + 1) = (uint32_t) rp;
        image_block_cnt -= 1;
    }

    retval = dwcssi_txwm_wait(ctrl_base);
    return retval;
}

