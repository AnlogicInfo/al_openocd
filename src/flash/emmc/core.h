/*
 * File: core.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */

#ifndef OPENOCD_FLASH_EMMC_CORE_H
#define OPENOCD_FLASH_EMMC_CORE_H
#include <flash/common.h>

struct emmc_device 
{
    const char *name;
    struct target *target;
    struct emmc_flash_controller *controller;
    void *controller_priv;
    struct emmc_manufacture *manufacturer;
    struct emmc_info *device;
    int    block_size;
    struct emmc_device *next;
};

enum 
{
    EMMC_MFR_SAMSUNG = 0x15
};

struct emmc_manufacture {
    int id;
    const char *name;
};

struct emmc_info {
    int mfr_id;
    int id;
    int chip_size;
    const char *name;
};


struct emmc_device *get_emmc_device_by_num(int num);

int emmc_read_data_block(struct emmc_device *emmc, uint8_t *data, uint32_t size);
int emmc_write_data_block(struct emmc_device *emmc, uint8_t *data, uint32_t size);
int emmc_read_status(struct emmc_device *emmc, uint8_t *status);

int emmc_register_commands(struct command_context *cmd_ctx);
COMMAND_HELPER(emmc_command_get_device, unsigned name_index, struct emmc_device **emmc);

#endif
