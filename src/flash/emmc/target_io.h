#ifndef OPENOCD_FLASH_EMMC_TARGET_IO_H
#define OPENOCD_FLASH_EMMC_TARGET_IO_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "core.h"

#include <helper/binarybuffer.h>
#include <target/arm.h>
#include <target/armv7m.h>
#include <target/aarch64.h>
#include <target/riscv/riscv.h>
#include <target/algorithm.h>

#define SYNC_TRANS        0
#define ASYNC_TRANS       1

enum target_emmc_op {
    TARGET_EMMC_NONE,
    TARGET_EMMC_READ,
    TARGET_EMMC_WRITE,
};

enum target_arch {
    TARGET_AARCH64,
    TARGET_RISCV,
};

struct target_emmc_loader {
    int block_size;

    struct target *target;
    enum target_arch arch;
    struct working_area *copy_area;
    uint8_t xlen;
    const uint8_t *code_src;
    int code_size;
    int code_area;

    int data_size;
    int buf_start;

    int image_block_cnt;

    uint8_t async;
    struct reg_param* reg_params;
    target_addr_t ctrl_base;
    enum target_emmc_op op;
    void* arch_info;
};

struct target_code_srcs 
{
    const uint8_t *riscv64_bin;
    int riscv64_size;
    const uint8_t *riscv32_bin;
    int riscv32_size;
    const uint8_t *aarch64_bin;
    int aarch64_size;
};

int target_set_arch_info(struct target_emmc_loader *loader, void* arm_info);
int target_sel_code(struct target_emmc_loader *loader, struct target_code_srcs srcs, struct reg_param* reg_params, uint32_t block_size);
int target_emmc_write(struct target_emmc_loader *loader, uint8_t *data, target_addr_t addr);
int target_emmc_write_async(struct target* trans_target, struct target_emmc_loader *loader, uint8_t *data, target_addr_t addr);


#endif

