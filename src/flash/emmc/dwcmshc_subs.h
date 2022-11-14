/*
 * File: dwcmshc_mmc.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-11-08
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-11-08
 */

#ifndef OPENOCD_FLASH_EMMC_DWCMSHC_SUBS_H
#define OPENOCD_FLASH_EMMC_DWCMSHC_SUBS_H
#include "dwcmshc_regs.h"
#include "emmc.h"

typedef struct dwcmshc_cmd_pkt_t 
{
	bool          argu_en;
	uint32_t      argument;

	CMD_R         cmd_reg;
	XFER_MODE_R   xfer_reg;
	
	// uint8_t cmd_index;
	// bool crc_chk_en;
	// bool xfer_dir;
	// uint8_t resp_len;
	uint8_t resp_type;
	uint32_t resp_buf[4];
} dwcmshc_cmd_pkt_t;

struct dwcmshc_emmc_controller {
    bool probed;
    target_addr_t      ctrl_base;
	dwcmshc_cmd_pkt_t  ctrl_cmd;
	uint32_t           io_bank_pwr;
    const struct       emmc_device *dev;
};


#define TIMEOUT_1S                          (1000*1000)
#define TIMEOUT_5S                          (5000*1000)
#define TIMEOUT_10S                         (10000*1000)
#define TIMEOUT_15S                         (15000*1000)

#define MMC_DELAY_SCALE						(2)

//io bank1 vcc ref
#define IO_BANK1_REF                         0xF8803C04ULL

#define MMC_IO_BANK1_SUPPORT_1V8(reg)	     (reg & 0x1)
#define MMC_IO_BANK1_SUPPORT_2V5(reg)	     ((reg & 0x2) >> 1)
#define MMC_IO_BANK1_SUPPORT_3V3(reg)	     ((reg & 0x4) >> 2)


// dwcmshc apis
int dwcmshc_emmc_ctl_init(struct emmc_device *emmc);
int dwcmshc_emmc_card_init(struct emmc_device *emmc);
int dwcmshc_emmc_rd_id(struct emmc_device *emmc);

#endif
