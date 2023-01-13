/*
 * File: target_io.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2023-01-12
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2023-01-12
 */



#include "target_io.h"

struct target *target_init_trans_target(char* name)
{
    struct target *trans_target;
    int retval = ERROR_OK;
    trans_target = get_target(name);
    if(trans_target == NULL)
    {
        LOG_ERROR("get transtarget fail");
    }
    else{
        if(trans_target->state != TARGET_HALTED)
        {
            LOG_DEBUG("halt trans target");
            target_halt(trans_target);
            retval = target_wait_state(trans_target, TARGET_HALTED, 500);
        }
        if(retval == ERROR_OK)
            return trans_target;
    }

    return NULL;
}

int target_init_arch(struct target_loader *loader)
{
    struct target *target = loader->exec_target;
    if(strncmp(target_name(target), "riscv", 4) == 0)
    {
        loader->trans_target = target_init_trans_target("riscv.cpu");
        loader->xlen = riscv_xlen(target);
        loader->arch_info = (struct riscv_algorithm *)malloc(sizeof(struct riscv_algorithm));
    }
    else
    {
        loader->trans_target = target_init_trans_target("al9000.a35.0");
        loader->xlen = 64;
        loader->arch_info = (struct aarch64_algorithm *) malloc(sizeof(struct aarch64_algorithm));
    }

    return ERROR_OK;
}

// int target_init_loader()
// {


// }


int target_flash_write_async(struct target_loader *loader, uint8_t *data, target_addr_t addr)
{

    return ERROR_OK;
};


