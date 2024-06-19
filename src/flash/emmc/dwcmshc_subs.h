/*
 * File: dwcmshc_mmc.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-11-08
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-11-08
 */

#ifndef OPENOCD_FLASH_EMMC_DWCMSHC_SUBS_H
#define OPENOCD_FLASH_EMMC_DWCMSHC_SUBS_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "dwcmshc_regs.h"
#include <target/image.h>
#include <flash/loader_io.h>


typedef struct dwcmshc_cmd_pkt_t 
{
	uint8_t       argu_en;
	uint32_t      argument;

	CMD_R         cmd_reg;
	XFER_MODE_R   xfer_reg;
	uint8_t       resp_type;
	uint32_t      resp_buf[4];
} dwcmshc_cmd_pkt_t;

struct dwcmshc_emmc_controller {
	bool                probed;
	uint8_t             io_location;
	target_addr_t       ctrl_base;
	dwcmshc_cmd_pkt_t   ctrl_cmd;
	uint32_t            io_bank_pwr;
	struct flash_loader flash_loader;
};


#define TIMEOUT_1S                          (1000)
#define TIMEOUT_5S                          (5000)
#define TIMEOUT_10S                         (10000)
#define TIMEOUT_15S                         (15000)

#define MMC_DELAY_SCALE						(2)

//AL9000 config
#define MIO_BASE                             0xF8803000ULL
#define EMIO_SEL11                           0xF880342CULL
#define EMIO_SEL12                           0xF8803430ULL
#define CFG_CTRL_SDIO1                       0xF8800150ULL
#define CFG_CTRL_SDIO2                       0xF8800154ULL

#define IO_BANK1_REF                         0xF8803C04ULL
#define FAST_MODE_BASE                       0xF8803940ULL

#define MMC_IO_BANK1_SUPPORT_1V8(reg)	     (reg & 0x1)
#define MMC_IO_BANK1_SUPPORT_2V5(reg)	     ((reg & 0x2) >> 1)
#define MMC_IO_BANK1_SUPPORT_3V3(reg)	     ((reg & 0x4) >> 2)


// dwcmshc apis
int dwcmshc_mio_init(struct emmc_device *emmc);
int dwcmshc_fast_mode(struct emmc_device *emmc);
int dwcmshc_emmc_ctl_init(struct emmc_device *emmc);
int dwcmshc_emmc_cmd_reset(struct emmc_device *emmc, uint32_t argument);
int dwcmshc_emmc_interrupt_init(struct emmc_device *emmc);
int dwcmshc_emmc_card_init(struct emmc_device *emmc, uint32_t* in_field);
int dwcmshc_emmc_rd_id(struct emmc_device *emmc);

int dwcmshc_emmc_set_bus_width(struct emmc_device *emmc);
int dwcmshc_emmc_rd_ext_csd(struct emmc_device *emmc, uint32_t* buf);
int dwcmshc_emmc_set_clk_ctrl(struct emmc_device *emmc, bool mode, uint32_t div);

int dwcmshc_emmc_async_write_image(struct emmc_device* emmc, uint8_t *buffer, target_addr_t addr, int image_size);

int dwcmshc_emmc_sync_write_image(struct emmc_device* emmc, uint8_t *buffer, target_addr_t addr, int image_size);

int slow_dwcmshc_emmc_write_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr);
int slow_dwcmshc_emmc_read_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr);

int dwcmshc_checksum(struct emmc_device *emmc, const uint8_t *buffer, uint32_t addr, uint32_t count, uint32_t* crc);
#endif
