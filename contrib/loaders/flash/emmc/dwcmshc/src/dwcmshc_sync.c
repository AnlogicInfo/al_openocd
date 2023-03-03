#include "dwcmshc.h"

void emmc_dwcmshc(volatile uint32_t *ctrl_base, uint32_t block_size, uint32_t addr, uint8_t *buffer, int size_in_bytes)
{
    static int block_addr_shift = 0;
    uint32_t i, block_addr;
    if(block_addr_shift == 0)
    {
        for(i=0; i<32; i++)
        {
            if(((block_size>>i)&0x1) == 1)
                break;
        }

        block_addr_shift = i;
    }

    block_addr = addr >> block_addr_shift;
    while(size_in_bytes > 0)
    {
        emmc_write_block(ctrl_base, block_addr, (uint32_t *)buffer);
        size_in_bytes -= block_size;
        block_addr += 1;
        buffer += block_size;
    }
}


