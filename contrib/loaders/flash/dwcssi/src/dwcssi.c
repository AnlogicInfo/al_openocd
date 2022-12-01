/*
 * Author: Tianyi Wang (tywang@anlogic.com)
 * Date:  2022-05-11
 * Modified By: Tianyi Wang (tywang@anlogic.com>)
 * Last Modified: 2022-05-11
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define	SPIFLASH_BSY		0
#define SPIFLASH_BSY_BIT	(1 << SPIFLASH_BSY)	/* WIP Bit of SPI SR */
#define SPIFLASH_WRITE_ENABLE	0x06 /* Write Enable */
#define SPIFLASH_READ_STATUS	0x05 /* Read Status Register */


/*Register offsets*/
#define     DWCSSI_REG_CTRLR0                         0x0
#define     DWCSSI_REG_CTRLR1                         0x4
#define     DWCSSI_REG_SSIENR                         0x8
#define     DWCSSI_REG_MWCR                           0xC
#define     DWCSSI_REG_SER                            0x10
#define     DWCSSI_REG_BAUDR                          0x14
#define     DWCSSI_REG_TXFTLR                         0x18
#define     DWCSSI_REG_RXFTLR                         0x1c
#define     DWCSSI_REG_TXFLR                          0x20
#define     DWCSSI_REG_RXFLR                          0x24
#define     DWCSSI_REG_SR                             0x28
#define     DWCSSI_REG_IMR                            0x2C
#define     DWCSSI_REG_ISR                            0x30
#define     DWCSSI_REG_RISR                           0x34
#define     DWCSSI_REG_TXOICR                         0x38
#define     DWCSSI_REG_RXOICR                         0x3c
#define     DWCSSI_REG_RXUICR                         0x40
#define     DWCSSI_REG_MSTICR                         0x44
#define     DWCSSI_REG_ICR                            0x48
#define     DWCSSI_REG_DMACR                          0x4c
#define     DWCSSI_REG_DMATDLR                        0x50
#define     DWCSSI_REG_DMARDLR                        0x54
#define     DWCSSI_REG_IDR                            0x58
#define     DWCSSI_REG_SSIC_VERSION_ID                0x5c
#define     DWCSSI_REG_DRx_START                      0x60
// #define     DWCSSI_REG_DRx[36]                        0x60+i*0x4, i=[0..35]
#define     DWCSSI_REG_RX_SAMPLE_DELAY                0xf0
#define     DWCSSI_REG_SPI_CTRLR0                     0xf4
#define     DWCSSI_REG_DDR_DRIVE_EDGE                 0xf8
#define     DWCSSI_REG_XIP_MODE_BITS                  0xfc

/*Fields*/
#define     DWCSSI_CTRLR0_DFS(x)                      (((x) & 0xF) << 0)
#define     DWCSSI_CTRLR0_TMOD(x)                     (((x) & 0x3) << 10)
#define     DWCSSI_CTRLR0_SPI_FRF(x)                  (((x) & 0x3) << 22)

#define     DWCSSI_CTRLR1_NDF(x)                      (((x) & 0xFFFF) << 0)
#define     DWCSSI_SSIC_EN(x)                         (((x) & 0x1) << 0)
#define     DWCSSI_SER(x)                             (((x) & 0x3) << 0)
#define     DWCSSI_BAUDR_SCKDV(x)                     (((x) & 0x7FFF) << 1)

#define     DWCSSI_TXFTLR_TFT(x)                      (((x) & 0xFF) << 0)
#define     DWCSSI_TXFTLR_TXFTHR(x)                   (((x) & 0xFF) << 16)

#define     DWCSSI_RXFTLR_RFT(x)                      (((x) & 0xFF) << 0)

#define     DWCSSI_SR_BUSY(x)                         (((x) & 0x1) << 0)
#define     DWCSSI_SR_TFNF(x)                         (((x) & 0x1) << 1)
#define     DWCSSI_SR_TFE(x)                          (((x) & 0x1) << 2)
#define     DWCSSI_SR_RFNE(x)                         (((x) & 0x1) << 3)

#define     DWCSSI_ISR_TXEIS(x)                       (((x) & 0x1) << 0)

#define     DWCSSI_SPI_CTRLR0_TRANS_TYPE              (((x) & 0x3) << 0)
#define     DWCSSI_SPI_CTRLR0_ADDR_L                  (((x) & 0xF) << 2)
#define     DWCSSI_SPI_CTRLR0_INST_L                  (((x) & 0x3) << 8)
#define     DWCSSI_SPI_CTRLR0_WAIT_CYCLES             (((x) & 0x1F) << 11)
#define     DWCSSI_SPI_CTRLR0_CLK_STRETCH_EN          (((x) & 0x1) << 30)
/*Masks*/
#define     DWCSSI_CTRLR0_DFS_MASK                     DWCSSI_CTRLR0_DFS(0xFFFFFFFF)     
#define     DWCSSI_CTRLR0_TMOD_MASK                    DWCSSI_CTRLR0_TMOD(0xFFFFFFFF)    
#define     DWCSSI_CTRLR0_SPI_FRF_MASK                 DWCSSI_CTRLR0_SPI_FRF(0xFFFFFFFF) 
#define     DWCSSI_CTRLR1_NDF_MASK                     DWCSSI_CTRLR1_NDF(0xFFFFFFFF)     
#define     DWCSSI_SSIC_EN_MASK                        DWCSSI_SSIC_EN(0xFFFFFFFF)        
#define     DWCSSI_SER_MASK                            DWCSSI_SER(0xFFFFFFFF)            
#define     DWCSSI_BAUDR_SCKDV_MASK                    DWCSSI_BAUDR_SCKDV(0xFFFFFFFF)    
#define     DWCSSI_TXFTLR_TFT_MASK                     DWCSSI_TXFTLR_TFT(0xFFFFFFFF)
#define     DWCSSI_TXFTLR_TXFTHR_MASK                  DWCSSI_TXFTLR_TXFTHR(0xFFFFFFFF)  
#define     DWCSSI_RXFTLR_RFT_MASK                     DWCSSI_RXFTLR_RFT(0xFFFFFFFF)
#define     DWCSSI_SR_BUSY_MASK                        DWCSSI_SR_BUSY(0xFFFFFFFF)        
#define     DWCSSI_SR_TFE_MASK                         DWCSSI_SR_TFE(0xFFFFFFFF)
#define     DWCSSI_SR_TFTNF_MASK                       DWCSSI_SR_TFNF(0xFFFFFFFF)
#define     DWCSSI_SR_RFNE_MASK                        DWCSSI_SR_RFNE(0xFFFFFFFF)
#define     DWCSSI_ISR_TXEIS_MASK                      DWCSSI_ISR_TXEIS(0xFFFFFFFF)      

#define     DWCSSI_SPI_CTRLR0_TRANS_TYPE_MASK           DWCSSI_SPI_CTRLR0_TRANS_TYPE(0xFFFFFFFF)
#define     DWCSSI_SPI_CTRLR0_ADDR_L_MASK               DWCSSI_SPI_CTRLR0_ADDR_L(0xFFFFFFFF)
#define     DWCSSI_SPI_CTRLR0_INST_L_MASK               DWCSSI_SPI_CTRLR0_INST_L(0xFFFFFFFF)
#define     DWCSSI_SPI_CTRLR0_WAIT_CYCLES_MASK          DWCSSI_SPI_CTRLR0_WAIT_CYCLES(0xFFFFFFFF)
#define     DWCSSI_SPI_CTRLR0_CLK_STRETCH_EN_MASK       DWCSSI_SPI_CTRLR0_CLK_STRETCH_EN(0xFFFFFFFF)

/*DFS define*/
#define     DFS_BYTE                                  (7)    // 7+1=8 bits=byte

/*TMOD define*/
#define     TX_AND_RX                                 0
#define     TX_ONLY                                   1
#define     RX_ONLY                                   2
#define     EEPROM_READ                               3

/*SPI_FRF define*/
#define     SPI_FRF_X1_MODE                           0
#define     SPI_FRF_X2_MODE                           1
#define     SPI_FRF_X4_MODE                           2
#define     SPI_FRF_X8_MODE                           3


#define TIMEOUT   1000

#define DEBUG
#ifdef DEBUG
#define ERROR_STACK(x)		    (x)
#define ERROR_DWCSSI_TXWM_WAIT	0x200000
#define ERROR_DWCSSI_TX		    0x100000
#define ERROR_DWCSSI_RX		    0x100000
#define ERROR_DWCSSI_WIP		0x500000
#else
#define ERROR_STACK(x)		    0
#define ERROR_DWCSSI_TXWM_WAIT	1
#define ERROR_DWCSSI_TX		    1
#define ERROR_DWCSSI_RX		    1
#define ERROR_DWCSSI_WIP		1
#endif
#define ERROR_OK                0

static int dwcssi_txwm_wait(volatile uint32_t *ctrl_base);
static void dwcssi_disable(volatile uint32_t *ctrl_base);
static void dwcssi_enable(volatile uint32_t *ctrl_base);
static int dwcssi_wait_flash_idle(volatile uint32_t *ctrl_base);
static int dwcssi_write_buffer(volatile uint32_t *ctrl_base, const uint8_t *buffer, uint32_t offest, uint32_t len, uint32_t flash_info);


int flash_dwcssi(volatile uint32_t *ctrl_base, uint32_t page_size, const uint8_t *buffer, uint32_t offset, uint32_t count, uint32_t flash_info)
{
    uint32_t result = 0;
    result = dwcssi_txwm_wait(ctrl_base);
    if(result != ERROR_OK)
        return result | ERROR_STACK(0x10000);
    
    result = dwcssi_wait_flash_idle(ctrl_base);    
    if(result != ERROR_OK)
    {
        result |= ERROR_STACK(0x2000);
        goto err;
    }

    uint32_t page_offset = offset & (page_size - 1);

    while(count > 0)
    {
        uint32_t cur_count;

        if(page_offset + count > page_size)
            cur_count = page_size - page_offset;
        else 
            cur_count = count;

        result = dwcssi_write_buffer(ctrl_base, buffer, offset, cur_count, flash_info);
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

static void dwcssi_set_bits(volatile uint32_t *ctrl_base, uint32_t address, uint32_t value, uint32_t bitmask)
{
    uint32_t rd_value, wr_value;
    dwcssi_read_reg(ctrl_base, &rd_value,address);
    wr_value = (rd_value & ~bitmask) | (value & bitmask);
    dwcssi_write_reg(ctrl_base, address, wr_value);
}

static void dwcssi_disable(volatile uint32_t *ctrl_base)
{
    uint32_t ssic_en;

    dwcssi_read_reg(ctrl_base, &ssic_en,DWCSSI_REG_SSIENR);
    dwcssi_write_reg(ctrl_base, DWCSSI_REG_SSIENR, ssic_en & ~DWCSSI_SSIC_EN(1));
}


static void dwcssi_enable(volatile uint32_t *ctrl_base)
{
    uint32_t ssic_en;
    dwcssi_read_reg(ctrl_base, &ssic_en,DWCSSI_REG_SSIENR);
    dwcssi_write_reg(ctrl_base, DWCSSI_REG_SSIENR, ssic_en | DWCSSI_SSIC_EN(1));
}

static void dwcssi_config_CTRLR0(volatile uint32_t *ctrl_base, uint32_t dfs, uint32_t spi_ref, uint32_t tmod)
{
    uint32_t val, mask;
    val = DWCSSI_CTRLR0_DFS(dfs) | DWCSSI_CTRLR0_SPI_FRF(spi_ref) | DWCSSI_CTRLR0_TMOD(tmod);
    mask = DWCSSI_CTRLR0_DFS_MASK | DWCSSI_CTRLR0_SPI_FRF_MASK | DWCSSI_CTRLR0_TMOD_MASK; 

    dwcssi_set_bits(ctrl_base, DWCSSI_REG_CTRLR0, val, mask);
}

static void dwcssi_config_CTRLR1(volatile uint32_t *ctrl_base, uint32_t ndf)
{
    dwcssi_set_bits(ctrl_base, DWCSSI_REG_CTRLR1, DWCSSI_CTRLR1_NDF(ndf), DWCSSI_CTRLR1_NDF_MASK);   
}


static void dwcssi_config_TXFTLR(volatile uint32_t *ctrl_base, uint32_t tft, uint32_t txfthr)
{
    uint32_t val, mask;
    
    val  = DWCSSI_TXFTLR_TFT(tft) | DWCSSI_TXFTLR_TXFTHR(txfthr);
    mask = DWCSSI_TXFTLR_TFT_MASK | DWCSSI_TXFTLR_TXFTHR_MASK;
    dwcssi_set_bits(ctrl_base, DWCSSI_REG_TXFTLR, val, mask);
}

static void dwcssi_config_eeprom(volatile uint32_t *ctrl_base, uint32_t len)
{
    dwcssi_disable(ctrl_base);
    dwcssi_config_CTRLR0(ctrl_base, DFS_BYTE, SPI_FRF_X1_MODE, EEPROM_READ);
    dwcssi_config_CTRLR1(ctrl_base, len-1);
    dwcssi_config_TXFTLR(ctrl_base, 0, 0);
    dwcssi_enable(ctrl_base);
}

static void dwcssi_config_tx(volatile uint32_t *ctrl_base, uint8_t frf, uint32_t tx_total_len, uint32_t tx_start_lv)
{
    dwcssi_disable(ctrl_base);
    dwcssi_config_CTRLR0(ctrl_base, DFS_BYTE, frf, TX_ONLY);
    dwcssi_config_TXFTLR(ctrl_base, 0, tx_start_lv);

    if(frf == SPI_FRF_X4_MODE)
    {
        dwcssi_config_CTRLR1(ctrl_base, tx_total_len - 1);
        dwcssi_set_bits(ctrl_base, DWCSSI_REG_SPI_CTRLR0, 0x40000220, 0xFFFFFFFF);
    }

    dwcssi_enable(ctrl_base);
}

/*dwc base functions*/
static int dwcssi_txwm_wait(volatile uint32_t *ctrl_base)
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

static int dwcssi_tx_buf(volatile uint32_t *ctrl_base, const uint8_t* in_buf, uint32_t in_cnt)
{
    uint32_t i;
    for(i = 0; i < in_cnt; i++)
    {
        // LOG_INFO("tx buf %x data %x", i, *(in_buf+i));
        dwcssi_tx(ctrl_base, *(in_buf+i));
    }

    return (dwcssi_txwm_wait(ctrl_base));
}

static int dwcssi_rx(volatile uint32_t *ctrl_base, uint8_t *out)
{
    uint32_t value;
    uint32_t dwcssi_sr;

    dwcssi_sr = dwcssi_get_bits(ctrl_base, DWCSSI_REG_SR, 0xFFFFFFFF, 0);
    // if(dwcssi_get_bits(ctrl_base, DWCSSI_REG_SR, DWCSSI_SR_RFNE_MASK, 3))
    if((dwcssi_sr >> 3) & 0x1) //rx fifo not empty
    {
        dwcssi_read_reg(ctrl_base, &value,DWCSSI_REG_DRx_START);
        // printf("rx value %x\n", value);
        if(out)
            *out = value & 0xFF;
        return ERROR_OK;            
    }
    
    return ERROR_DWCSSI_RX;
}

static int dwcssi_wait_flash_idle(volatile uint32_t *ctrl_base)
{
    uint8_t rx;
    uint32_t rx_len = 1;
    uint32_t result;
    // printf("check flash status\n");

    dwcssi_config_eeprom(ctrl_base, rx_len);

    while(1)
    {
        result = dwcssi_tx(ctrl_base, SPIFLASH_READ_STATUS);
        if (result != ERROR_OK)
	    	return result | ERROR_STACK(0x000200);
        if (dwcssi_txwm_wait(ctrl_base) != ERROR_OK)
	    	return result | ERROR_STACK(0x000300);
        if(dwcssi_rx(ctrl_base, &rx) == ERROR_OK)
        {
            if((rx & SPIFLASH_BSY_BIT) == 0)
            {
                // printf("flash status %x idle\n", rx);
                return (ERROR_OK);
            }
        }
    }

}

static int dwcssi_flash_wr_en(volatile uint32_t *ctrl_base, uint8_t frf)
{
    uint8_t tx_start_lv = 0;

    dwcssi_config_tx(ctrl_base, frf, 0, tx_start_lv);
    dwcssi_tx(ctrl_base, SPIFLASH_WRITE_ENABLE);
    dwcssi_txwm_wait(ctrl_base);
    return ERROR_OK;
}

static int dwcssi_write_buffer(volatile uint32_t *ctrl_base, const uint8_t *buffer, uint32_t offset, uint32_t len, uint32_t flash_info)
{

    // printf("dwcssi slow write offset %x len %x\n", offset, len);
    dwcssi_flash_wr_en(ctrl_base, SPI_FRF_X1_MODE);
    dwcssi_config_tx(ctrl_base, SPI_FRF_X4_MODE, len, 0x4);
    dwcssi_tx(ctrl_base, flash_info);
    dwcssi_tx(ctrl_base, offset);
    dwcssi_tx_buf(ctrl_base, buffer, len);
    return dwcssi_wait_flash_idle(ctrl_base);
}

