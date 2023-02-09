
#ifndef LOADER_DWCSSI_H
#define LOADER_DWCSSI_H

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

// void dwcssi_config_eeprom(volatile uint32_t *ctrl_base, uint32_t len);
// void dwcssi_config_tx(volatile uint32_t *ctrl_base, uint8_t frf, uint32_t tx_total_len, uint32_t tx_start_lv);
// int dwcssi_txwm_wait(volatile uint32_t *ctrl_base);
// int dwcssi_wait_flash_idle(volatile uint32_t *ctrl_base);
uint32_t wait_fifo(uint32_t *work_area_start);
int dwcssi_txwm_wait(volatile uint32_t *ctrl_base);
void dwcssi_disable(volatile uint32_t *ctrl_base);
void dwcssi_enable(volatile uint32_t *ctrl_base);
int dwcssi_flash_wr_en(volatile uint32_t *ctrl_base, uint8_t frf);
int dwcssi_wait_flash_idle(volatile uint32_t *ctrl_base);
int dwcssi_tx_buf_no_wait(volatile uint32_t *ctrl_base, const uint8_t* in_buf, uint32_t in_cnt);

int dwcssi_write_buffer(volatile uint32_t *ctrl_base, const uint8_t *buffer, uint32_t offest, uint32_t len, uint32_t flash_info);
int dwcssi_read_page(volatile uint32_t *ctrl_base, uint8_t *buffer, uint32_t offset, uint32_t len, uint32_t qread_cmd);
#endif