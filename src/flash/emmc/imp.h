#ifndef OPENOCD_FALSH_EMMC_IMP_H
#define OPENOCD_FLASH_EMMC_IMP_H
#include <stdbool.h>

#include "core.h"
#include "driver.h"
#include <target/target.h>


void emmc_device_add(struct emmc_device *c);
int emmc_probe(struct emmc_device *emmc);

int emmc_write_data_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t address);
int emmc_write_image(struct emmc_device *emmc, uint8_t *buffer, uint32_t address, int size);
int emmc_read_data_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t address);
int emmc_verify_image(struct emmc_device *emmc, uint8_t *buffer, uint32_t addr, int size);


#endif
