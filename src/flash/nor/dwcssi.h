/*
 * Author: Tianyi Wang (tywang@anlogic.com)
 * Date:  2022-04-27
 * Modified By: Tianyi Wang (tywang@anlogic.com>)
 * Last Modified: 2022-04-27
 */
#ifndef OPENOCD_FLASH_NOR_DWCSSI_H
#define OPENOCD_FLASH_NOR_DWCSSI_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include "spi.h"
#include <jtag/jtag.h>
#include <helper/time_support.h>
#include <target/algorithm.h>
#include "target/riscv/riscv.h"


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
#define     DWCSSI_SR_BUSY(x)                         (((x) & 0x1) << 0)
#define     DWCSSI_SR_TFNF(x)                         (((x) & 0x1) << 1)
#define     DWCSSI_SR_TFE(x)                          (((x) & 0x1) << 2)
#define     DWCSSI_SR_RFNE(x)                         (((x) & 0x1) << 3)
#define     DWCSSI_ISR_TXEIS(x)                       (((x) & 0x1) << 0)

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
#define     DWCSSI_SR_BUSY_MASK                        DWCSSI_SR_BUSY(0xFFFFFFFF)        
#define     DWCSSI_SR_TFE_MASK                         DWCSSI_SR_TFE(0xFFFFFFFF)
#define     DWCSSI_SR_TFTNF_MASK                       DWCSSI_SR_TFNF(0xFFFFFFFF)
#define     DWCSSI_SR_RFNE_MASK                        DWCSSI_SR_RFNE(0xFFFFFFFF)
#define     DWCSSI_ISR_TXEIS_MASK                      DWCSSI_ISR_TXEIS(0xFFFFFFFF)      

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

/* Timeout in ms */
#define     DWCSSI_CMD_TIMEOUT                        (100)
#define     DWCSSI_PROBE_TIMEOUT                      (100)
#define     DWCSSI_MAX_TIMEOUT                        (3000)

struct dwcssi_flash_bank {
    bool probed;
    target_addr_t ctrl_base;
    uint32_t      flash_start_offset;
    const struct flash_device *dev;
};

struct dwcssi_target {
    char *name;
    uint32_t tap_idcode;
    uint32_t ctrl_base;
};


// flash support

int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t id);
int flash_sector_check(struct flash_bank *bank, uint32_t offset, uint32_t count);

int flash_check_status(uint8_t status);

#endif
