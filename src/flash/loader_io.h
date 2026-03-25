/*
 * File: loader_io.h
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
#include <stddef.h>
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
    uint32_t entry_offset;
};

/* 
 * Loader Shared Memory Layout
 * Explicitly defines the protocol between Host and Target.
 * Using explicit padding to maintain 0x40 (64-byte) alignment/spacing if required.
 */
struct loader_shmem_map {
    /* Page 0: Parameter Passing Area */
    struct {
        uint32_t flash_addr;           // Offset: 0x00
        uint8_t  _pad0[60];            // Padding to 0x40
        
        uint32_t image_size;           // Offset: 0x40
        uint8_t  _pad1[60];            // Padding to 0x80
        
        uint32_t bank_id;              // Offset: 0x80
        uint8_t  _pad2[60];            // Padding to 0xC0
    } params;
    
    /* Explicitly define gap to match 256-byte (0x100) Page 0 size */
    uint8_t _reserved_gap[0x100 - sizeof(struct {
        uint32_t flash_addr;
        uint8_t  _pad0[60];
        uint32_t image_size;
        uint8_t  _pad1[60];
        uint32_t bank_id;
        uint8_t  _pad2[60];
    })];

    /* Page 1: FIFO Data Buffer Starts Here */
    uint8_t fifo_buffer[]; 
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
    int ddr_en;
    int data_size;
    target_addr_t buf_start;
    target_addr_t ctrl_flag_addr;
    target_addr_t next_addr_addr;

    uint32_t *section_block_addrs;
    uint32_t *section_offsets_blocks;
    unsigned int section_count;
};


int loader_flash_write_sync(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size);
int loader_flash_write_async(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size);
int loader_flash_write_async_pp(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size);
int loader_flash_write_pp(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size, int bank_number);
int loader_flash_read_pp(struct flash_loader *loader, struct code_src *srcs, uint8_t *data, target_addr_t addr, int image_size, int bank_number);
int loader_flash_write(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size, int bank_number);
int loader_flash_crc(struct flash_loader *loader, struct code_src *srcs, target_addr_t addr, uint32_t* target_crc);
int loader_flash_crc_mem_parm(struct flash_loader *loader, struct code_src *srcs, target_addr_t addr, int bank_number, uint32_t* target_crc);
int loader_load_elf(struct code_src *src, const char *filename);
#endif
