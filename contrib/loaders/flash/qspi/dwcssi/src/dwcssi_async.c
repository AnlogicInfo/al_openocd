#include "dwcssi.h"

int flash_dwcssi(volatile uint32_t *ctrl_base, int32_t page_size, int count_in_bytes, uint32_t *buf_start, uint32_t *buf_end, uint32_t offset, uint32_t qprog_cmd)
{
    uint8_t *rp;
    uint32_t retval;
    uint32_t page_offset = offset & (page_size - 1);

    // while(count_in_bytes > 0)
    {
        uint32_t cur_count;

        asm volatile("hlt #0x0B");
        if(page_offset + count_in_bytes > page_size)
            cur_count = page_size - page_offset;
        else
            cur_count = count_in_bytes;
        
        rp = (uint8_t *) wait_fifo(buf_start);
        retval = dwcssi_write_buffer(ctrl_base, rp, offset, cur_count, qprog_cmd);

        page_offset = 0;
        rp += cur_count;
        if(rp >= buf_end)
            rp = buf_start + 2;
        *(buf_start + 1) = (uint32_t) rp;

        offset         += cur_count;
        count_in_bytes -= cur_count;
    }

    return retval;
}

