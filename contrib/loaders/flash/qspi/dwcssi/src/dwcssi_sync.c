#include "dwcssi.h"

int flash_dwcssi(volatile uint32_t *ctrl_base, int32_t page_size, uint32_t offset, const uint8_t *buffer, uint32_t count, uint32_t qprog_cmd)
{
    uint32_t result = 0;
    uint32_t page_offset = offset & (page_size - 1);

    while(count > 0)
    {
        uint32_t cur_count;

        if(page_offset + count > page_size)
            cur_count = page_size - page_offset;
        else 
            cur_count = count;

        result = dwcssi_write_buffer(ctrl_base, buffer, offset, cur_count, qprog_cmd);
        if(result != ERROR_OK)
        {
            result |= ERROR_STACK(0x20000);
            goto err;
        }

        page_offset = 0;
        buffer += cur_count;
        offset += cur_count;
        count  -= cur_count;
    }

err:
    return result;

}