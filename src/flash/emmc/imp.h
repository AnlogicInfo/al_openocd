/*
 * File: imp.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */

#ifndef OPENOCD_FALSH_EMMC_IMP_H
#define OPENOCD_FLASH_EMMC_IMP_H
#include <stdbool.h>

#include "core.h"
#include "driver.h"
#include <target/target.h>


void emmc_device_add(struct emmc_device *c);
int emmc_probe(struct emmc_device *emmc);

int emmc_write_data_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t address);
int emmc_write_image(struct emmc_device *emmc, uint32_t *buffer, uint32_t address, int size, int sync);
int emmc_read_data_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t address);

#endif
