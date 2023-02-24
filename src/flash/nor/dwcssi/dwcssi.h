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

#include <flash/nor/imp.h>
#include <flash/nor/spi.h>
#include <jtag/jtag.h>
#include <target/image.h>
#include <flash/loader_io.h>
#include "dwcssi_flash.h"

// qspi flash mio defines
#define     MIO_BASE                                  0xF8803000

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

#define     DWCSSI_SAMPLE_DELAY_SE(x)                 (((x) & 0x1) << 16)
#define     DWCSSI_SAMPLE_DELAY_RSD(x)                (((x) & 0xFF) << 0)


typedef union dwcssi_spi_ctrlr0_t
{
    uint32_t reg_val;
    struct
    {
        uint32_t TRANS_TYPE         :2;   /*[1:0]-Address and instruction transfer format.*/
        uint32_t ADDR_L             :4;   /*[5:2]-This bit defines Length of Address to be transmitted.*/
        uint32_t RSVD_6             :1;   /*[6]-RSVD*/
        uint32_t XIP_MD_BIT_EN      :1;   /*[7]-Mode bits enable in XIP mode.*/
        uint32_t INST_L             :2;   /*[9:8]-Dual/Quad/Octal mode instruction length in bits.*/
        uint32_t RSVD_10            :1;   /*[10]-RSVD*/
        uint32_t WAIT_CYCLES        :5;   /*[15:11]-Wait cycles in Dual/Quad/Octal mode between control frames transmit and data reception.*/
        uint32_t SPI_DDR_EN         :1;   /*[16]-SPI DDR Enable bit.*/
        uint32_t INST_DDR_EN        :1;   /*[17]-Instruction DDR Enable bit.*/
        uint32_t SPI_RXDS_EN        :1;   /*[18]-Read data strobe enable bit.*/
        uint32_t XIP_DFS_HC         :1;   /*[19]-Fix DFS for XIP transfers.*/
        uint32_t XIP_INST_EN        :1;   /*[20]-XIP instruction enable bit.*/
        uint32_t SSIC_XIP_CONT_XFER_EN  :1; /*[21]-Enable continuous transfer in XIP mode.*/
        uint32_t RSVD_23_22         :2;   /*[23:22]-RSVD*/
        uint32_t SPI_DM_EN          :1;   /*[24]-SPI data mask enable bit.*/
        uint32_t SPI_RXDS_SIG_EN    :1;   /*[25]-Enable rxds signaling during address and command phase of Hypebus transfer.*/
        uint32_t XIP_MBL            :2;   /*[27:26]-XIP Mode bits length.*/
        uint32_t RSVD_28            :1;   /*[28]-RSVD*/
        uint32_t XIP_PREFETCH_EN    :1;   /*[29]-Enables XIP pre-fetch functionality in DWC_ssi. */
        uint32_t CLK_STRETCH_EN     :1;   /*[30]-Enables clock stretching capability in SPI transfers.*/
        uint32_t RSVD_31            :1;   /*[31]-RSVD*/
    } reg_fields;
} dwcssi_spi_ctrlr0_t;

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
#define     DWCSSI_SAMPLE_DELAY_SE_MASK                DWCSSI_SAMPLE_DELAY_SE(0xFFFFFFFF)
#define     DWCSSI_SAMPLE_DELAY_RSD_MASK               DWCSSI_SAMPLE_DELAY_RSD(0xFFFFFFFF)


#define     DISABLE                                   0
#define     ENABLE                                    1

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

/*SPI_CTRLR0 define*/
#define     TRANS_TYPE_TT0                            0
#define     TRANS_TYPE_TT1                            1
#define     TRANS_TYPE_TT2                            2
#define     TRANS_TYPE_TT3                            3

#define     ADDR_L0                                   0
#define     ADDR_L24                                  6
#define     ADDR_L28                                  7
#define     ADDR_L32                                  8

#define     INST_L8                                   2

#define     MBL_2                                     0
#define     MBL_4                                     1
#define     MBL_8                                     2
#define     MBL_16                                    3

#define     STANDARD_SPI_MODE                         0
#define     DUAL_SPI_MODE                             1
#define     QUAD_SPI_MODE                             2
#define     QPI_MODE                                  3

/* Timeout in ms */
#define     DWCSSI_CMD_TIMEOUT                        (100)
#define     DWCSSI_PROBE_TIMEOUT                      (100)
#define     DWCSSI_MAX_TIMEOUT                        (3000)


struct dwcssi_target {
    char *name;
    uint32_t tap_idcode;
    uint32_t ctrl_base;
};

struct dwcssi_trans_config {
    uint8_t  tmod;
    uint8_t spi_frf;
    uint32_t ndf;
    uint32_t tx_start_lv;
    uint32_t rx_ip_lv;

    uint8_t trans_type;
    uint8_t stretch_en;
    uint8_t addr_len;
    uint8_t wait_cycle;
};

#define RISCV     0
#define ARM       1

int dwcssi_wait_flash_idle(struct flash_bank *bank, int timeout, uint8_t* sr);
void dwcssi_config_tx(struct flash_bank *bank, uint8_t frf, uint32_t tx_total_len, uint32_t tx_start_lv);
void dwcssi_config_trans(struct flash_bank *bank, struct dwcssi_trans_config *trans_config);

int dwcssi_tx(struct flash_bank *bank, uint32_t in);
int dwcssi_txwm_wait(struct flash_bank* bank);
int dwcssi_flash_tx_cmd(struct flash_bank *bank, uint8_t *cmd, uint8_t len, uint8_t cmd_mode);
int dwcssi_rd_flash_reg(struct flash_bank *bank, uint32_t* rd_val, uint8_t cmd, uint32_t len);
int dwcssi_wr_flash_reg(struct flash_bank *bank, uint8_t *cmd, uint8_t len, uint8_t cmd_mode);

//general spi ops
int general_reset_f0(struct flash_bank *bank, uint8_t cmd_mode);
int general_reset_66_99(struct flash_bank *bank, uint8_t cmd_mode);
void general_spi_quad_rd_config(struct flash_bank *bank, uint8_t addr_len);
int general_spi_err_chk(struct flash_bank* bank);
int general_spi_quad_en(struct flash_bank* bank);
int general_spi_quad_dis(struct flash_bank* bank);
int general_spi_qpi_en(struct flash_bank* bank);
int general_spi_qpi_dis(struct flash_bank* bank);
#endif
