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

typedef enum mio_speed_t
{
    MIO_SPEED_SLOW = 0x0, 
    MIO_SPEED_MEDI = 0x2,
    MIO_SPEED_FAST = 0x7
} mio_speed_t;

typedef enum mio_pull_t
{
    MIO_PULL_DIS = 0x0,
    MIO_PULL_25K_EN = 0x4,
    MIO_PULL_10K_EN = 0x5
} mio_pull_t;

// pll defines
#define     CLK_SEL                                   0xF8801040
#define     CPU4X_DIV1_PARA                           0xF8801010
#define     CPU4X_DIV2_PARA                           0xF8801014
#define     CPU4X_DIV4_PARA                           0xF8801018
#define     CPUPLL_CTRL1                              0xF8801104
#define     CPUPLL_CTRL8                              0xF8801120
#define     CPUPLL_CTRL9                              0xF8801124
#define     CPUPLL_CTRL18                             0xF8801148
#define     CPUPLL_CTRL19                             0xF880114C
#define     CPUPLL_STATE0                             0xF8801180


// qspi flash mio defines
#define     MIO_BASE                                  0xF8803000
#define     MIO_PARA_BASE                             0xF8803800
#define     MIO_PARA1_BASE                            0xF8803804
#define     IO1000_CNT_DIV                            0xF8801030
#define     MIO_BANK201_REF                           0xF8803C04
#define     GPIO_CONFIG                               0xF8411004
#define     GPIO_OUT                                  0xF8411000

// qspi ctrl defines
#define     CFG_CTRL_QSPI                             0xF880016C

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

#define     DWCSSI_REG_INCR_INST                      0x100
#define     DWCSSI_REG_WRAP_INST                      0x104
#define     DWCSSI_REG_XIP_CNT_TIME_OUT               0x114
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

static inline void dwcssi_spi_ctrlr0_trans_type(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t trans_type)
{
    spi_ctrl0->reg_fields.TRANS_TYPE = trans_type;
}

static inline void dwcssi_spi_ctrlr0_addr_len(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t addr_l)
{
    spi_ctrl0->reg_fields.ADDR_L = addr_l;
}

static inline void dwcssi_spi_ctrlr0_xip_md_bit_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t xip_md_bit_en)
{
    spi_ctrl0->reg_fields.XIP_MD_BIT_EN = xip_md_bit_en;
}

static inline void dwcssi_spi_ctrlr0_inst_len(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t inst_l)
{
    spi_ctrl0->reg_fields.INST_L = inst_l;
}

static inline void dwcssi_spi_ctrlr0_wait_cycles(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t wait_cycles)
{
    spi_ctrl0->reg_fields.WAIT_CYCLES = wait_cycles;
}

static inline void dwcssi_spi_ctrlr0_spi_ddr_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t spi_ddr_en)
{
    spi_ctrl0->reg_fields.SPI_DDR_EN = spi_ddr_en;
}

static inline void dwcssi_spi_ctrlr0_inst_ddr_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t inst_ddr_en)
{
    spi_ctrl0->reg_fields.INST_DDR_EN = inst_ddr_en;
}

static inline void dwcssi_spi_ctrlr0_spi_rxds_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t spi_rxds_en)
{
    spi_ctrl0->reg_fields.SPI_RXDS_EN = spi_rxds_en;
}

static inline void dwcssi_spi_ctrlr0_xip_dfs_hc(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t xip_dfs_hc)
{
    spi_ctrl0->reg_fields.XIP_DFS_HC = xip_dfs_hc;
}

static inline void dwcssi_spi_ctrlr0_xip_inst_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t xip_inst_en)
{
    spi_ctrl0->reg_fields.XIP_INST_EN = xip_inst_en;
}

static inline void dwcssi_spi_ctrlr0_ssic_xip_cont_xfer_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t ssic_xip_cont_xfer_en)
{
    spi_ctrl0->reg_fields.SSIC_XIP_CONT_XFER_EN = ssic_xip_cont_xfer_en;
}

static inline void dwcssi_spi_ctrlr0_spi_dm_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t spi_dm_en)
{
    spi_ctrl0->reg_fields.SPI_DM_EN = spi_dm_en;
}

static inline void dwcssi_spi_ctrlr0_spi_rxds_sig_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t spi_rxds_sig_en)
{
    spi_ctrl0->reg_fields.SPI_RXDS_SIG_EN = spi_rxds_sig_en;
}

static inline void dwcssi_spi_ctrlr0_xip_mbl(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t xip_mbl)
{
    spi_ctrl0->reg_fields.XIP_MBL = xip_mbl;
}

static inline void dwcssi_spi_ctrlr0_xip_prefetch_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t xip_prefetch_en)
{
    spi_ctrl0->reg_fields.XIP_PREFETCH_EN = xip_prefetch_en;
}

static inline void dwcssi_spi_ctrlr0_clk_stretch_en(dwcssi_spi_ctrlr0_t* spi_ctrl0, uint8_t clk_stretch_en)
{
    spi_ctrl0->reg_fields.CLK_STRETCH_EN = clk_stretch_en;
}

typedef union dwcssi_xip_mode_bits_t
{
    uint32_t reg_val;
    struct
    {
        uint32_t XIP_MD_BITS         :16;
        uint32_t RSVD_XIP_MD_BITS    :16;
    } reg_fields;
} dwcssi_xip_mode_bits_t;

typedef union dwcssi_xip_incr_inst_t
{
    uint32_t reg_val;
    struct
    {
        uint32_t INCR_INST           :16;
        uint32_t RSVD_INCR_INST      :16;
    } reg_fields;
} dwcssi_xip_incr_inst_t;

typedef union dwcssi_xip_wrap_inst_t
{
    uint32_t reg_val;
    struct
    {
        uint32_t WRAP_INST           :16;
        uint32_t RSVC_INST           :16;
    } reg_fields;
} dwcssi_xip_wrap_inst_t;

typedef union dwcssi_xip_cnt_time_out_t
{
    uint32_t reg_val;
    struct
    {
        uint32_t CNT_TIMEOUT         :8;
        uint32_t RSVD_CNT_TIMEOUT    :24;
    } reg_fields;
} dwcssi_xip_cnt_time_out_t;

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


/**
 * @brief Qspi xip dfs config enum
 */
typedef enum
{
    QSPI_XipDfsChange = 0,
    QSPI_XipDfsFix    = 1
} AL_QSPI_XipDfsFixEnum;

/**
 * @brief Qspi xip mode bits length enum
 */
typedef enum
{
    QSPI_MBL_2 = 0,
    QSPI_MBL_4 = 1,
    QSPI_MBL_8 = 2,
    QSPI_MBL_16 = 3,
} AL_QSPI_XipModeBitsLengthEnum;

/**
 * @brief Qspi xip port1 norflash size enum
 */
typedef enum
{
    QSPI_XipPort1NorFlash_1MB = 7,
    QSPI_XipPort1NorFlash_2MB = 6,
    QSPI_XipPort1NorFlash_4MB = 5,
    QSPI_XipPort1NorFlash_8MB = 4,
    QSPI_XipPort1NorFlash_16MB = 3,
    QSPI_XipPort1NorFlash_32MB = 2,
    QSPI_XipPort1NorFlash_64MB = 1,
    QSPI_XipPort1NorFlash_128MB = 0
} AL_QSPI_XipPort1NorFlashSize;


#define     X1_PAGE_SIZE                              64

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

#define HIGH      1
#define LOW       0

int dwcssi_wait_flash_idle(struct flash_bank *bank, int timeout, uint8_t* sr);
void dwcssi_config_tx(struct flash_bank *bank, uint8_t frf, uint32_t tx_total_len, uint32_t tx_start_lv);
void dwcssi_config_trans(struct flash_bank *bank, struct dwcssi_trans_config *trans_config);

int dwcssi_tx(struct flash_bank *bank, uint32_t in);
int dwcssi_txwm_wait(struct flash_bank* bank);
int dwcssi_flash_tx_cmd(struct flash_bank *bank, uint8_t *cmd, uint8_t len, uint8_t cmd_mode);
int dwcssi_rd_flash_reg(struct flash_bank *bank, uint8_t* rd_val, uint8_t cmd, uint32_t len);
int dwcssi_wr_flash_reg(struct flash_bank *bank, uint8_t *cmd, uint8_t len, uint8_t cmd_mode);
int dwcssi_set_reg(struct flash_bank *bank, uint8_t cmd, uint8_t index);


uint32_t mio_pad_ctrl0(mio_speed_t speed, mio_pull_t pull_up, mio_pull_t pull_dw);
int driver_priv_init(struct flash_bank *bank, struct dwcssi_flash_bank *driver_priv);

void dwcssi_config_init(struct flash_bank *bank, uint8_t sckdv);
int dwcssi_erase(struct flash_bank *bank, unsigned int first, unsigned int last);
int dwcssi_protect(struct flash_bank *bank, int set, unsigned int first, unsigned last);
int dwcssi_protect_check(struct flash_bank *bank);
int dwcssi_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count);
int dwcssi_read(struct flash_bank *bank, uint8_t *buffer, uint32_t offset, uint32_t count);
int dwcssi_verify(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count);
int dwcssi_xip_init(struct flash_bank *bank);
int dwcssi_flash_reset(struct flash_bank *bank);
int dwcssi_probe(struct flash_bank *bank);
int dwcssi_customize(struct flash_bank *bank, uint8_t read_cmd, uint8_t pprog_cmd, uint8_t erase_cmd,
			uint32_t pagesize, uint32_t sectorsize, uint32_t size_in_bytes);
int get_driver_priv(struct flash_bank *bank, struct command_invocation *cmd);



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
