/*
 * File: target_io.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2023-01-12
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2023-01-12
 */

#ifndef OPENOCD_FLASH_NOR_TARGET_IO_H
#define OPENOCD_FLASH_NOR_TARGET_IO_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <helper/binarybuffer.h>
#include <helper/time_support.h>
#include <target/arm.h>
#include <target/armv7m.h>
#include <target/aarch64.h>
#include <target/riscv/riscv.h>
#include <target/algorithm.h>


struct target_loader {
    // targets setting
    struct target *exec_target;
    struct target *trans_target;
    void *arch_info;

    // param setting
    struct reg_param  *reg_params;
    target_addr_t     ctrl_base;
    uint8_t           xlen;
    int               block_size;

    // code wa setting
    struct working_area *copy_area;
    const uint8_t *code_src;
    int code_area;

    // data wa setting
    int data_size;
    int buf_start;
};

int target_flash_write_async(struct target_loader *loader, uint8_t *data, target_addr_t addr);

#endif

