/*
 * File: dwcmshc_regs.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-11-14
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-11-14
 */

/*
 * Register Offsets
 */
#ifndef OPENOCD_FLASH_EMMC_DWCMSHC_REGS_H
#define OPENOCD_FLASH_EMMC_DWCMSHC_REGS_H

#include "imp.h"
#include <helper/time_support.h>
#include <helper/binarybuffer.h>
#include <target/target.h>

#define OFFSET_SDMASA_R                          0x0 // SDMA System Address register
#define OFFSET_BLOCKSIZE_R                       0x4 // Block Size register
#define OFFSET_BLOCKCOUNT_R                      0x6 // 16-bit Block Count register
#define OFFSET_ARGUMENT_R                        0x8 // Argument register
#define OFFSET_XFER_MODE_R                       0xc // Transfer Mode register
#define OFFSET_CMD_R                             0xe // Command register
#define OFFSET_RESP01_R                          0x10 // Response Register 0/1
#define OFFSET_RESP23_R                          0x14 // Response Register 2/3
#define OFFSET_RESP45_R                          0x18 // Response Register 4/5
#define OFFSET_RESP67_R                          0x1c // Response Register 6/7
#define OFFSET_BUF_DATA_R                        0x20 // Buffer Data Port Register
#define OFFSET_PSTATE_REG                        0x24 // Present State Register
#define OFFSET_HOST_CTRL1_R                      0x28 // Host Control 1 Register
#define OFFSET_PWR_CTRL_R                        0x29 // Power Control Register
#define OFFSET_BGAP_CTRL_R                       0x2a // Block Gap Control Register
#define OFFSET_WUP_CTRL_R                        0x2b // Wakeup Control Register
#define OFFSET_CLK_CTRL_R                        0x2c // Clock Control Register
#define OFFSET_TOUT_CTRL_R                       0x2e // Timeout Control Register
#define OFFSET_SW_RST_R                          0x2f // Software Reset Register
#define OFFSET_NORMAL_INT_STAT_R                 0x30 // Normal Interrupt Status Register
#define OFFSET_ERROR_INT_STAT_R                  0x32 // Error Interrupt Status Register
#define OFFSET_NORMAL_INT_STAT_EN_R              0x34 // Normal Interrupt Status Enable Register
#define OFFSET_ERROR_INT_STAT_EN_R               0x36 // Error Interrupt Status Enable Register
#define OFFSET_NORMAL_INT_SIGNAL_EN_R            0x38 // Normal Interrupt Signal Enable Register
#define OFFSET_ERROR_INT_SIGNAL_EN_R             0x3a // Error Interrupt Signal Enable Register
#define OFFSET_AUTO_CMD_STAT_R                   0x3c // Auto CMD Status Register
#define OFFSET_HOST_CTRL2_R                      0x3e // Host Control 2 Register
#define OFFSET_CAPABILITIES1_R                   0x40 // Capabilities 1 Register - 0 to 31
#define OFFSET_CAPABILITIES2_R                   0x44 // Capabilities Register - 32 to 63
#define OFFSET_CURR_CAPABILITIES1_R              0x48 // Maximum Current Capabilities Register - 0 to 31
#define OFFSET_CURR_CAPABILITIES2_R              0x4c // Maximum Current Capabilities Register - 32 to 63
#define OFFSET_FORCE_AUTO_CMD_STAT_R             0x50 // Force Event Register for Auto CMD Error Status register
#define OFFSET_FORCE_ERROR_INT_STAT_R            0x52 // Force Event Register for Error Interrupt Status
#define OFFSET_ADMA_ERR_STAT_R                   0x54 // ADMA Error Status Register
#define OFFSET_ADMA_SA_LOW_R                     0x58 // ADMA System Address Register - Low
#define OFFSET_ADMA_SA_HIGH_R                    0x5c // ADMA System Address Register - High
#define OFFSET_PRESET_INIT_R                     0x60 // Preset Value for Initialization
#define OFFSET_PRESET_DS_R                       0x62 // Preset Value for Default Speed
#define OFFSET_PRESET_HS_R                       0x64 // Preset Value for High Speed
#define OFFSET_PRESET_SDR12_R                    0x66 // Preset Value for SDR12
#define OFFSET_PRESET_SDR25_R                    0x68 // Preset Value for SDR25
#define OFFSET_PRESET_SDR50_R                    0x6a // Preset Value for SDR50
#define OFFSET_PRESET_SDR104_R                   0x6c // Preset Value for SDR104
#define OFFSET_PRESET_DDR50_R                    0x6e // Preset Value for DDR50
#define OFFSET_PRESET_UHS2_R                     0x74 //  hold the preset value for UHS-II and HS400 speed modes

typedef union
{
    uint16_t d16;
    struct {
		uint16_t xfer_block_size:12;
		uint16_t sdma_buf_bdary:3;
		uint16_t rsvd_blocksize15:1;
    } bit;

} BLOCKSIZE_R;

typedef union
{
    uint16_t d16;
    struct {
	    uint16_t dma_en:1;
	    uint16_t block_count_enable:1;
	    uint16_t auto_cmd_enable:2;
	    uint16_t data_xfer_dir:1;
	    uint16_t multi_blk_sel:1;
	    uint16_t resp_type:1;
	    uint16_t resp_err_chk_enable:1;
	    uint16_t resp_int_disable:1;
	    uint16_t rsvd:7;
    }bit;
}XFER_MODE_R;

typedef union
{
    uint16_t d16;
    struct {
        uint16_t resp_type_select:2;
	    uint16_t sub_cmd_flag:1;
	    uint16_t cmd_crc_chk_enable:1;
	    uint16_t cmd_idx_chk_enable:1;
	    uint16_t data_present_sel:1;
	    uint16_t cmd_type:2;
	    uint16_t cmd_index:6;
	    uint16_t rsvd_15_14:2;
    }bit;
}CMD_R;

typedef union
{
	uint32_t d32;
	struct {
	uint32_t cmd_inhibit:1;
	uint32_t cmd_inhibit_dat:1;
	uint32_t dat_line_active:1;
	uint32_t re_tune_req:1;
	uint32_t dat_7_4_mask:4;
	uint32_t wr_xfer_active:1;
	uint32_t rd_xfer_active:1;
	uint32_t buf_wr_enable:1;
	uint32_t buf_rd_enable:1;
	uint32_t rsvd_15_12:4;
	uint32_t card_inserted:1;
	uint32_t card_stable:1;
	uint32_t card_detect_pin_level:1;
	uint32_t wr_protect_sw_lvl:1;
	uint32_t dat_3_0:4;
	uint32_t cmd_line_lvl:1;
	uint32_t host_reg_vol:1;
	uint32_t rsvd_26:1;
	uint32_t cmd_issue_err:1;
	uint32_t sub_cmd_stat:1;
	uint32_t in_dormant_st:1;
	uint32_t lane_sync:1;
	uint32_t uhs2_if_detect:1;
	}bit;
}PSTATE_REG_R;

typedef union
{
	uint8_t d8;
	struct {
		uint8_t led_ctrl:1;
		uint8_t dat_xfer_width:1;
		uint8_t high_speed_en:1;
		uint8_t dma_sel:2;
		uint8_t extdat_xfer:1;
		uint8_t card_detect_test_lvl:1;
		uint8_t card_detect_sig_sel:1;
	}bit;
}HOST_CTRL1_R;

typedef union
{
	uint8_t d8;
	struct {
        uint8_t sd_bus_pwr_vdd1:1;
	    uint8_t sd_bus_vol_vdd1:3;
	    uint8_t sd_bus_pwr_vdd2:1;
	    uint8_t sd_bus_vol_vdd2:3;
	}bit;
}PWR_CTRL_R;

typedef union
{
	uint8_t d8;
	struct {
        uint8_t stop_bg_req:1;
	    uint8_t continue_req:1;
	    uint8_t rd_wait_ctrl:1;
	    uint8_t int_at_bgap:1;
	    uint8_t rsvd_7_4:4;
	}bit;
}BGAP_CTRL_R;

typedef union
{
	uint8_t d8;
	struct {
        uint8_t card_int:1;
	    uint8_t card_insert:1;
	    uint8_t card_removal:1;
	    uint8_t rsvd_7_3:5;
	}bit;
} WUP_CTRL_R;

typedef union
{
	uint16_t d16;
	struct {
	uint16_t	internal_clk_en:1;
	uint16_t	internal_clk_stable:1;
	uint16_t	sd_clk_en:1;
	uint16_t	pll_enable:1;
	uint16_t   rsvd_4:1;
	uint16_t	clk_gen_select:1;
	uint16_t   upper_freq_sel:2;
	uint16_t   freq_sel:8;
    }bit;
} CLK_CTRL_R;


typedef union
{
	uint8_t d8;
	struct {
    uint8_t   tout_cnt:4;
	uint8_t   rsvd_7_4:4;
    }bit;
} TOUT_CTRL_R;


typedef union
{
	uint8_t d8;
	struct {
    uint8_t	sw_rst_all:1;
	uint8_t   sw_rst_cmd:1;
	uint8_t	sw_rst_dat:1;
	uint8_t   rsvd_7_3:5;
    }bit;
} SW_RST_R;


typedef union
{
    uint16_t d16;
    struct {
	    uint16_t	 cmd_complete:1;
	    uint16_t	 xfer_complete:1;
	    uint16_t	 bgap_event:1;
	    uint16_t	 dma_interrupt:1;
	    uint16_t    buf_wr_ready:1;
	    uint16_t	 buf_rd_ready:1;
	    uint16_t    card_insertion:1;
	    uint16_t    card_removal:1;
	    uint16_t    card_interrupt:1;
	    uint16_t    int_a:1;
	    uint16_t    int_b:1;
	    uint16_t    int_c:1;
	    uint16_t    re_tune_event:1;
	    uint16_t    fx_event:1;
	    uint16_t    cqe_event:1;
	    uint16_t    err_interrupt:1;
    }bit;
} NORMAL_INT_STAT_R;

typedef union
{
    uint16_t d16;
    struct {
        uint16_t	 cmd_tout_err:1;
	    uint16_t	 cmd_crc_err:1;
	    uint16_t	 cmd_end_bit_err:1;
	    uint16_t	 cmd_idx_err:1;
	    uint16_t    data_tout_err:1;
	    uint16_t	 data_crc_err:1;
	    uint16_t    data_end_bit_err:1;
	    uint16_t    cur_lmt_err:1;
	    uint16_t    auto_cmd_err:1;
	    uint16_t    adma_err:1;
	    uint16_t    tuning_err:1;
	    uint16_t    resp_err:1;
	    uint16_t    boot_ack_err:1;
	    uint16_t    vendor_err1:1;
	    uint16_t    vendor_err2:1;
	    uint16_t    vendor_err3:1;
    }bit;
} ERROR_INT_STAT_R;


typedef union
{
    uint16_t d16;
    struct {
	    uint16_t	cmd_complete_stat_en:1; /* intr when response received */
	    uint16_t    xfer_complete_stat_en:1; /* intr when data read/write xfer completed */
	    uint16_t    bgap_event_stat_en:1;
	    uint16_t	dma_interrupt_stat_en:1;
	    uint16_t	buf_wr_ready_stat_en:1;
	    uint16_t    buf_rd_ready_stat_en:1;
	    uint16_t	card_insertion_stat_en:1;
	    uint16_t	card_removal_stat_en:1;
	    uint16_t    card_interrupt_stat_en:1;
	    uint16_t	int_a_stat_en:1;
	    uint16_t	int_b_stat_en:1;
	    uint16_t	int_c_stat_en:1;
	    uint16_t	re_tune_event_stat_en:1;
	    uint16_t	fx_event_stat_en:1;
	    uint16_t    cqe_event_stat_en:1;
		uint16_t	rsvd_15:1;
     }bit;
} NORMAL_INT_STAT_EN_R;

typedef union
{
    uint16_t d16;
    struct {
	    uint16_t	cmd_tout_err_stat_en:1;
	    uint16_t	cmd_crc_err_stat_en:1;
	    uint16_t	cmd_end_bit_err_stat_en:1;
	    uint16_t	cmd_idx_err_stat_en:1;
	    uint16_t    data_tout_err_stat_en:1;
	    uint16_t    data_crc_err_stat_en:1;
	    uint16_t    data_end_bit_err_stat_en:1;
	    uint16_t	cur_lmt_err_stat_en:1;
	    uint16_t    auto_cmd_err_stat_en:1;
	    uint16_t	adma_err_stat_en:1;
	    uint16_t	tuning_err_stat_en:1;
	    uint16_t	resp_err_stat_en:1;
	    uint16_t    boot_ack_err_stat_en:1;
	    uint16_t    vendor_err_stat_en1:1;
	    uint16_t	vendor_err_stat_en2:1;
	    uint16_t	vendor_err_stat_en3:1;
     }bit;
} ERROR_INT_STAT_EN_R;


typedef union
{
    uint16_t d16;
    struct {
	uint16_t	cmd_complete_signal_en:1; /* intr when response received */
	uint16_t    xfer_complete_signal_en:1; /* intr when data read/write xfer completed */
	uint16_t    bgap_event_signal_en:1;
	uint16_t	dma_interrupt_signal_en:1;
	uint16_t	buf_wr_ready_signal_en:1;
	uint16_t    buf_rd_ready_signal_en:1;
	uint16_t	card_insertion_signal_en:1;
	uint16_t	card_removal_signal_en:1;
	uint16_t    card_interrupt_signal_en:1;
	uint16_t	int_a_signal_en:1;
	uint16_t	int_b_signal_en:1;
	uint16_t	int_c_signal_en:1;
	uint16_t	re_tune_event_signal_en:1;
	uint16_t	fx_event_signal_en:1;
	uint16_t    cqe_event_signal_en:1;
    uint16_t    rscv_15:1;
    }bit;
} NORMAL_INT_SIGNAL_EN_R;

typedef union
{
    uint16_t d16;
    struct {
	uint16_t	cmd_tout_err_signal_en:1;
	uint16_t	cmd_crc_err_signal_en:1;
	uint16_t	cmd_end_bit_err_signal_en:1;
	uint16_t	cmd_idx_err_signal_en:1;
	uint16_t    data_tout_err_signal_en:1;
	uint16_t    data_crc_err_signal_en:1;
	uint16_t    data_end_bit_err_signal_en	:1;
	uint16_t	cur_lmt_err_signal_en:1;
	uint16_t    auto_cmd_err_signal_en:1;
	uint16_t	adma_err_signal_en:1;
	uint16_t	tuning_err_signal_en:1;
	uint16_t	resp_err_signal_en:1;
	uint16_t    boot_ack_err_signal_en:1;
	uint16_t    vendor_err_signal_en1:1;
	uint16_t	vendor_err_signal_en2:1;
	uint16_t	vendor_err_signal_en3:1;
    }bit;
}ERROR_INT_SIGNAL_EN_R;

typedef union
{
    uint16_t d16;
    struct {
		uint16_t	auto_cmd12_not_exec:1;
		uint16_t	auto_cmd_tout_err:1;
		uint16_t	auto_cmd_crc_err:1;
		uint16_t	auto_cmd_ebit_err:1;
		uint16_t    auto_cmd_idx_err:1;
		uint16_t	auto_cmd_resp_err:1;
		uint16_t    rsvd_6:1;
		uint16_t    cmd_not_issued_auto_cmd12:1;
		uint16_t    rsvd_15_8:8;
    }bit;
} AUTO_CMD_STAT_R;

typedef union
{
    uint16_t d16;
    struct {
		uint16_t	uhs_mode_sel:3;
		uint16_t	signaling_en:1;
		uint16_t	drv_strength_sel:2;
		uint16_t	exec_tuning:1;
		uint16_t    sample_clk_sel:1;
		uint16_t	uhs2_if_enable:1;
		uint16_t    rsvd_9:1;
		uint16_t    adma2_len_mode:1;
		uint16_t    cmd23_enable:1;
		uint16_t    host_ver4_enable:1;
		uint16_t    addressing:1;
		uint16_t    async_int_enable:1;
		uint16_t    preset_val_enable:1;
    }bit;
} HOST_CTRL2_R;

typedef struct
{
	uint32_t d32;
	struct {
	uint32_t	sdr50_support:1;
	uint32_t 	sdr104_support:1;
	uint32_t 	ddr50_support:1;
	uint32_t	uhs2_support:1;
	uint32_t	drv_typea:1;
	uint32_t 	drv_typec:1;
	uint32_t	drv_typed:1;
	uint32_t	rsvd_39:1;
	uint32_t 	retune_cnt:4;
	uint32_t	rsvd_44:1;
	uint32_t	use_tuning_sdr50:1;
	uint32_t	re_tuning_modes:2;
	uint32_t	clk_mul:8;
	uint32_t    rsvd_56_58:3;
	uint32_t	adma3_support:1;
	uint32_t	vdd2_18v_support:1;
	uint32_t	rsvd_61:1;
	uint32_t	rsvd_62_63:2;
	}bit;
}CAPABILITIES2_R;

typedef struct
{
	uint32_t d32;
	struct {
		uint32_t tout_clk_freq : 6;
		uint32_t rsvd_6 : 1;
		uint32_t tout_clk_unit : 1;
		uint32_t base_clk_freq : 8;
		uint32_t max_blk_len : 2;
		uint32_t embedded_8_bit : 1;
		uint32_t adma2_support : 1;
		uint32_t rsvd_20 : 1;
		uint32_t high_speed_support : 1;
		uint32_t sdma_support : 1;
		uint32_t sus_res_support : 1;
		uint32_t volt_33 : 1;
		uint32_t volt_30 : 1;
		uint32_t volt_18 : 1;
		uint32_t sys_addr_64_v4 : 1;
		uint32_t sys_addr_64_v3 : 1;
		uint32_t async_int_support : 1;
		uint32_t slot_type_r : 1;
	}bit;
}CAPABILITIES1_R;

typedef struct
{
	uint32_t d32;
	struct {
		uint32_t max_cur_33v : 8;
		uint32_t max_cur_30v : 8;
		uint32_t max_cur_18v : 8;
		uint32_t rsvd_31_24 : 8;
	}bit;
}CURR_CAPABILITIES1_R;

typedef struct
{
	uint32_t d32;
	struct {
		uint32_t max_cur_vdd2_18v : 8;
		uint32_t rsvd_63_40 : 24;
	}bit;
}CURR_CAPABILITIES2_R;

typedef struct
{
	uint16_t d16;
	struct {
		uint16_t force_auto_cmd12_not_exec : 1;
		uint16_t force_auto_cmd_tout_err : 1;
		uint16_t force_auto_cmd_crc_err : 1;
		uint16_t force_auto_cmd_ebit_err : 1;
		uint16_t force_auto_cmd_idx_err : 1;
		uint16_t force_auto_cmd_resp_err : 1;
		uint16_t rsvd_6 : 1;
		uint16_t force_cmd_not_issued_auto_cmd12 : 1;
		uint16_t rsvd_15_8 : 8;
	}bit;
} FORCE_AUTO_CMD_STAT_R;


typedef struct
{
	uint16_t d16;
	struct {
		uint16_t force_cmd_tout_err : 1;
		uint16_t force_cmd_crc_err : 1;
		uint16_t force_cmd_end_bit_err : 1;
		uint16_t force_cmd_idx_err : 1;
		uint16_t force_data_tout_err : 1;
		uint16_t force_data_crc_err : 1;
		uint16_t force_data_end_bit_err : 1;
		uint16_t force_cur_lmt_err : 1;
		uint16_t force_auto_cmd_err : 1;
		uint16_t force_adma_err : 1;
		uint16_t force_tuning_err : 1;
		uint16_t force_resp_err : 1;
		uint16_t force_boot_ack_err : 1;
		uint16_t force_vendor_err1 : 1;
		uint16_t force_vendor_err2 : 1;
		uint16_t force_vendor_err3 : 1;
	}bit;
}FORCE_ERROR_INT_STAT_R;


typedef struct
{
	uint32_t d32;
	struct {
		uint32_t adma_err_states : 2;
		uint32_t adma_len_err : 1;
		uint32_t rsvd_31_3 : 29;
	}bit;
}ADMA_ERR_STAT_R;

typedef struct
{
	uint16_t d16;
	struct {
		uint16_t freq_sel_val_init : 10;
		uint16_t clk_gen_sel_val_init : 1;
		uint16_t rsvd_13_11 : 3;
		uint16_t drv_sel_val_init : 2;
	}bit;
} PRESET_INIT_R;

typedef struct
{
	uint16_t d16;
	struct {
		uint16_t freq_sel_val_ds : 10;
		uint16_t clk_gen_sel_val_ds : 1;
		uint16_t rsvd_29_27 : 3;
		uint16_t drv_sel_val_ds : 2;
	}bit;
}PRESET_DS_R;


typedef struct
{
	uint16_t d16;
	struct {
		uint16_t freq_sel_val_0 : 10;
		uint16_t clk_gen_sel_val_0 : 1;
		uint16_t rsvd_13_11 : 3;
		uint16_t drv_sel_val_0 : 2;
	}bit;
}PRESET_HS_R;


typedef struct
{
	uint16_t d16;
	struct {
		uint16_t freq_sel_val_1 : 10;
		uint16_t clk_gen_sel_val_1 : 1;
		uint16_t rsvd_13_11 : 3;
		uint16_t drv_sel_val_1 : 2;
	}bit;
} PRESET_SDR_12_R;


typedef struct
{
	uint16_t d16;
	struct {
		uint16_t freq_sel_val_0 : 10;
		uint16_t clk_gen_sel_val_0 : 1;
		uint16_t rsvd_13_11 : 3;
		uint16_t drv_sel_val_0 : 2;
	}bit;
}PRESET_SDR25_R;

typedef struct
{
	uint16_t d16;
	struct {
		uint16_t freq_sel_val_1 : 10;
		uint16_t clk_gen_sel_val_1 : 1;
		uint16_t rsvd_13_11 : 3;
		uint16_t drv_sel_val_1 : 2;
	}bit;
}PRESET_SDR50_R;

typedef struct
{
	uint16_t d16;
	struct {
		uint16_t freq_sel_val_0 : 10;
		uint16_t clk_gen_sel_val_0 : 1;
		uint16_t rsvd_13_11 : 3;
		uint16_t drv_sel_val_0 : 2;
	}bit;
}PRESET_SDR104_R;


typedef struct
{
	uint16_t d16;
	struct {
		uint16_t freq_sel_val_1 : 10;
		uint16_t clk_gen_sel_val_1 : 1;
		uint16_t rsvd_13_11 : 3;
		uint16_t drv_sel_val_1 : 2;
	}bit;
}PRESET_DDR50_R;

typedef struct
{
	uint32_t d32;
	struct {
		uint32_t freq_sel_val : 10;
		uint32_t clk_gen_sel_val : 1;
		uint32_t rsvd_13_11 : 3;
		uint32_t drv_sel_val : 2;
		uint32_t rsvd_31_24:16;
	}bit;
}PRESET_UHS2;

#define ARGU_EN                         1
#define ARGU_DIS                        0

//host_ctrl1 register param
#define MMC_HC1_LED_CTRL_OFF			0x0
#define MMC_HC1_LED_CTRL_ON				0x1
#define MMC_HC1_DAT_XFER_WIDTH_1BIT		0x0
#define MMC_HC1_DAT_XFER_WIDTH_4BIT		0x1
#define MMC_HC1_NORMAL_SPEED			0x0
#define MMC_HC1_HIGH_SPEED				0x1
#define MMC_HC1_DMA_SEL_SDMA			0x0
#define MMC_HC1_DMA_SEL_RSV				0x1
#define MMC_HC1_DMA_SEL_ADMA2			0x2
#define MMC_HC1_DMA_SEL_ADMA3			0x3
#define MMC_HC1_EXT_DAT_XFER_DEF		0x0		
#define MMC_HC1_EXT_DAT_XFER_8BIT		0x1

//pwr_ctrl register param
#define MMC_PC_SBP_VDD1_OFF				0x0	//SD BUS PWR
#define MMC_PC_SBP_VDD1_ON				0x1
#define MMC_PC_SBV_VDD1_RSV				0x4	//SD BUS VOLTAGE 0x0~0x4 reserved
#define SD_PC_SBV_VDD1_1V8				0x5
#define SD_PC_SBV_VDD1_3V0				0x6
#define EMMC_PC_SBV_VDD1_1V2			0x5
#define EMMC_PC_SBV_VDD1_1V8			0x6
#define MMC_PC_SBV_VDD1_3V3				0x7

//xfer_mode register param
#define MMC_XM_DMA_DISABLE				0x0
#define MMC_XM_DMA_ENABLE				0x1
#define MMC_XM_BLOCK_COUNT_DISABLE		0x0
#define MMC_XM_BLOCK_COUNT_ENABLE		0x1
#define MMC_XM_AUTO_CMD_DISABLE			0x0	//in SDIO, this field must be 0
#define MMC_XM_AUTO_CMD12_ENABLE		0x1
#define MMC_XM_AUTO_CMD23_ENABLE		0x2
#define MMC_XM_AUTO_CMD_AUTO_ENABLE		0x3
#define MMC_XM_DATA_XFER_DIR_WRITE		0x0
#define MMC_XM_DATA_XFER_DIR_READ		0x1
#define MMC_XM_SEL_SINGLE_BLOCK			0x0
#define MMC_XM_SEL_MULTI_BLOCK			0x1
#define MMC_XM_RESP_R1					0x0	//Memory
#define MMC_XM_RESP_R5					0x1	//SDIO
#define MMC_XM_RESP_ERR_CHK_DISABLE		0x0
#define MMC_XM_RESP_ERR_CHK_ENABLE		0x1
#define MMC_XM_RESP_INT_ENABLE			0x0
#define MMC_XM_RESP_INT_DISABLE			0x1

//cmd register param
#define MMC_C_NO_RESP					0x0
#define MMC_C_RESP_LEN_136				0x1
#define MMC_C_RESP_LEN_48				0x2
#define MMC_C_RESP_LEN_48B				0x3	//check busy after response
#define MMC_C_CMD_FLAG_MAIN				0x0
#define MMC_C_CMD_FLAG_SUB				0x1
#define MMC_C_CMD_CRC_CHECK_DISABLE		0x0
#define MMC_C_CMD_IDX_CHECK_DISABLE		0x0
#define MMC_C_CMD_CRC_CHECK_ENABLE		0x1
#define MMC_C_CMD_IDX_CHECK_ENABLE		0x1
#define MMC_C_NO_DATA_PRESENT			0x0
#define MMC_C_DATA_PRESENT				0x1
#define MMC_C_NORMAL_CMD				0x0
#define MMC_C_SUSPEND_CMD				0x1
#define MMC_C_RESUME_CMD				0x2
#define MMC_C_ABORT_CMD					0x3

//clk_ctrl register param
#define MMC_CC_INTER_CLK_DISABLE		0x0
#define MMC_CC_INTER_CLK_ENABLE			0x1
#define MMC_CC_INTER_CLK_NOT_STABLE		0x0
#define MMC_CC_INTER_CLK_STABLE			0x1
#define MMC_CC_SD_CLK_DISABLE			0x0
#define MMC_CC_SD_CLK_ENABLE			0x1
#define MMC_CC_PLL_LOW_PWR_MODE			0x0
#define MMC_CC_PLL_ENABLE				0x1
// #define MMC_CC_CLK_GEN_SEL_DIVIDED		0x0
// #define MMC_CC_CLK_GEN_SEL_PROGRAM		0x1
#define MMC_CC_CLK_CARD_INIT            0x0
#define MMC_CC_CLK_CARD_OPER            0x1

//tout_ctrl register param
#define MMC_TC_TOUT_CNT_2_13			0x0
#define MMC_TC_TOUT_CNT_2_14			0x1
#define MMC_TC_TOUT_CNT_2_15			0x2
#define MMC_TC_TOUT_CNT_2_16			0x3
#define MMC_TC_TOUT_CNT_2_17			0x4
#define MMC_TC_TOUT_CNT_2_18			0x5
#define MMC_TC_TOUT_CNT_2_19			0x6
#define MMC_TC_TOUT_CNT_2_20			0x7
#define MMC_TC_TOUT_CNT_2_21			0x8
#define MMC_TC_TOUT_CNT_2_22			0x9
#define MMC_TC_TOUT_CNT_2_23			0xA
#define MMC_TC_TOUT_CNT_2_24			0xB
#define MMC_TC_TOUT_CNT_2_25			0xC
#define MMC_TC_TOUT_CNT_2_26			0xD
#define MMC_TC_TOUT_CNT_2_27			0xE

// normal_int_stat register param
#define WAIT_CMD_COMPLETE               0
#define WAIT_XFER_COMPLETE              1
#define WAIT_BUF_WR_READY               4
#define WAIT_BUF_RD_READY               5

//normal_int_stat_en register param
#define MMC_NORMAL_INT_STAT_MASKED		0x0
#define MMC_NORMAL_INT_STAT_EN			0x1

//error_int_stat_en register param
#define MMC_ERR_INT_STAT_MASKED			0x0
#define MMC_ERR_INT_STAT_EN				0x1

//normal_int_signal_en register param
#define MMC_NORMAL_INT_SIGN_MASKED		0x0
#define MMC_NORMAL_INT_SIGN_EN			0x1

//error_int_signal_en register param
#define MMC_ERR_INT_SIGN_MASKED			0x0
#define MMC_ERR_INT_SIGN_EN				0x1

#endif
