#include "dwcssi.h"
uint32_t wait_fifo(uint32_t *buf_start)
{
    uint32_t wp = 0, rp = 0;
    while(wp == rp)
    {
        wp = * (volatile uint32_t *) buf_start;
        rp = *(volatile uint32_t *) (buf_start + 1);
    }
    return rp;
}


static void dwcssi_read_reg(volatile uint32_t *ctrl_base, uint32_t *value, uint32_t address)
{

    *value = ctrl_base[address / 4];
}

static int dwcssi_get_bits(volatile uint32_t *ctrl_base, uint32_t address, uint32_t bitmask, uint32_t index)
{
    uint32_t rd_value, ret_value;

    dwcssi_read_reg(ctrl_base, &rd_value, address);

    ret_value = (rd_value & bitmask) >> index;
    return ret_value;
}


static void dwcssi_write_reg(volatile uint32_t *ctrl_base, uint32_t address, uint32_t value)
{
    ctrl_base[address/4] = value;
}

// static void dwcssi_set_bits(volatile uint32_t *ctrl_base, uint32_t address, uint32_t value, uint32_t bitmask)
// {
//     uint32_t rd_value, wr_value;
//     dwcssi_read_reg(ctrl_base, &rd_value,address);
//     wr_value = (rd_value & ~bitmask) | (value & bitmask);
//     dwcssi_write_reg(ctrl_base, address, wr_value);
// }

int dwcssi_txwm_wait(volatile uint32_t *ctrl_base)
{
    uint32_t timeout = 0;
    // TX fifo empty
    while(1)
    {
        if(dwcssi_get_bits(ctrl_base, DWCSSI_REG_SR, DWCSSI_SR_TFE_MASK, 2))
            break;
        timeout = timeout + 1;
        if(timeout > TIMEOUT)
            return ERROR_DWCSSI_TXWM_WAIT;
    }

    timeout = 0;
    while (1) {
        if(!(dwcssi_get_bits(ctrl_base, DWCSSI_REG_SR, DWCSSI_SR_BUSY_MASK, 0)))
            break;
        timeout = timeout + 1;
        if(timeout > TIMEOUT)
            return ERROR_DWCSSI_TXWM_WAIT;
    }

    return ERROR_OK;
}

static int dwcssi_tx(volatile uint32_t *ctrl_base, uint32_t in)
{
    uint32_t fifo_not_full=0;

    while(1) 
    {
        fifo_not_full = dwcssi_get_bits(ctrl_base, DWCSSI_REG_SR, DWCSSI_SR_TFTNF_MASK, 1);
        if(fifo_not_full)
        {
            dwcssi_write_reg(ctrl_base, DWCSSI_REG_DRx_START, in);
            return ERROR_OK;
        }
    }

    // printf("tx timeout\n");
    return ERROR_DWCSSI_TX;
}


void dwcssi_tx_block(volatile uint32_t *ctrl_base, const uint8_t* in_buf, uint32_t block_size)
{
    uint32_t i;
    for(i = 0; i < block_size; i++)
    {
        dwcssi_tx(ctrl_base, *(in_buf+i));
    }
}


int dwcssi_tx_buf(volatile uint32_t *ctrl_base, const uint8_t* in_buf, uint32_t in_cnt)
{
    dwcssi_tx_block(ctrl_base, in_buf, in_cnt);

    return (dwcssi_txwm_wait(ctrl_base));
}
