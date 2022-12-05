#ifndef OPENOCD_FLASH_EMMC_TARGET_IO_H
#define OPENOCD_FLASH_EMMC_TARGET_IO_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "core.h"

#include <helper/binarybuffer.h>
#include <target/arm.h>
#include <target/armv7m.h>
#include <target/riscv/riscv.h>
#include <target/algorithm.h>

enum target_emmc_op {
    TARGET_EMMC_NONE,
    TARGET_EMMC_READ,
    TARGET_EMMC_WRITE,
};

struct target_emmc_loader {
    struct target *target;
    struct working_area *copy_area;

    uint8_t xlen;
    const uint8_t *code_src;
    int code_size;
    int data_size;
    struct reg_param* reg_params;
    target_addr_t ctrl_base;
    enum target_emmc_op op;

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
int target_sel_code(struct target_emmc_loader *loader, struct target_code_srcs srcs, struct reg_param* reg_params, uint32_t block_size);
int target_emmc_write(struct target_emmc_loader *loader, uint8_t *data, target_addr_t addr);



#endif

