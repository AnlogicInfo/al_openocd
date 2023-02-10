/*
 * File: target_io.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2023-01-12
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2023-01-12
 */

#ifndef OPENOCD_LOADER_IO_H
#define OPENOCD_LOADER_IO_H

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

#define RESTORE        1
#define NO_RESTORE     0

enum work_mode 
{
    SYNC_TRANS,
    ASYNC_TRANS,
    CRC_CHECK,
};

enum code_src_index{
    RV64_SRC,
    RV32_SRC,
    AARCH64_SRC,
};

enum loader_op {
    LOADER_NONE,
    LOADER_READ,
    LOADER_WRITE,
};

struct code_src {
    const uint8_t *bin;
    int size;
};

struct flash_loader {
    // dev info
    void *dev_info;
    // targets setting
    struct target *exec_target;
    struct target *trans_target;
    void *arch_info;
    enum work_mode work_mode;
    enum loader_op op;

    // param setting
    struct reg_param  *reg_params;
    int               param_cnt;
    void (*set_params_priv) (struct flash_loader *loader);

    target_addr_t     ctrl_base;
    uint8_t           xlen;
    int               block_size;
    int               image_size;

    // code wa setting
    struct working_area *copy_area;
    struct code_src *code_src;
    int code_area;

    // data wa setting
    int data_size;
    int buf_start;
};


int loader_flash_write_sync(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size);
int loader_flash_write_async(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size);
int loader_flash_crc(struct flash_loader *loader, struct code_src *srcs, target_addr_t addr, uint32_t* target_crc);
#endif

