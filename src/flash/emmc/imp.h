/*
 * File: imp.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */

#ifndef OPENOCD_FALSH_EMMC_IMP_H
#define OPENOCD_FLASH_EMMC_IMP_H
#include "core.h"
#include "driver.h"

void emmc_device_add(struct emmc_device *c);

int emmc_write_block(struct emmc_device *emmc,
    uint32_t block, uint8_t *data, uint32_t data_size);
int emmc_read_block(struct emmc_device *emmc,
    uint32_t block, uint8_t *data, uint32_t data_size);

int emmc_probe(struct emmc_device *emmc);

#endif
