#include "dwcssi.h"

static uint8_t buf[256] = {0};

int flash_dwcssi(volatile uint32_t *ctrl_base, int32_t page_size, int count, uint32_t *buf_start, uint32_t *buf_end, uint32_t offset, uint32_t qprog_cmd, uint32_t spictrl)
{
    uint8_t *rp;
    uint32_t retval = 0;

    uint32_t cur_count;

    while(count > 0)
    {
        rp = (uint8_t *) wait_fifo(buf_start);
        if (count < page_size)
            cur_count = count;
        else
            cur_count = page_size;

        retval = dwcssi_write_buffer(ctrl_base, rp, offset, cur_count, qprog_cmd, spictrl);

        // page_offset = 0;
        rp += cur_count;
        if(rp == buf_end)
            rp = buf_start + 2;
        *(buf_start + 1) = (uint32_t) rp;

        offset += cur_count;
        count  -= cur_count;
    }

    return retval;
}

