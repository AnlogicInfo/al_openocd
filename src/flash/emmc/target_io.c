#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "imp.h"
#include "core.h"
#include "target_io.h"
#include <helper/binarybuffer.h>
#include <target/arm.h>
#include <target/armv7m.h>
#include <target/algorithm.h>
#include <target/riscv/riscv.h>


static int target_code_to_working_area(struct target *target,
	const uint8_t *code, unsigned code_size,
	unsigned additional, struct working_area **area)
{
	int retval;
	unsigned size = code_size + additional;

	/* REVISIT this assumes size doesn't ever change.
	 * That's usually correct; but there are boards with
	 * both large and small page chips, where it won't be...
	 */

	/* make sure we have a working area */
	if (!*area) {
		retval = target_alloc_working_area(target, size, area);
		if (retval != ERROR_OK) {
			LOG_INFO("%s: no %d byte buffer", __func__, (int) size);
			return ERROR_EMMC_NO_BUFFER;
		}
	}
    // LOG_INFO("workarea addr " TARGET_ADDR_FMT, (*area)->address);
    target_write_buffer(target, (*area)->address, code_size, code);
	/* buffer code in target endianness */
	// target_buffer_set_u32_array(target, code_buf, code_size, code);

	// /* copy code to work area */
	// retval = target_write_memory(target, (*area)->address,
	// 		4, code_size / 4, code_buf);

	return retval;
}

static const uint8_t riscv32_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_riscv_32.inc"
};

static const uint8_t riscv64_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_riscv_64.inc"
};

static const uint8_t aarch64_bin[] = {
#include "../../../contrib/loaders/flash/emmc/dwcmshc/build/emmc_aarch_64.inc"
};


static int target_set_algorithm(struct target_emmc_loader *loader, uint8_t *data, target_addr_t offset, int size, struct reg_param* reg_params)
{
    struct target *target = loader->target;
    target_addr_t  target_buf;
    const uint8_t *target_code_src;
    int target_code_size = 0, retval = ERROR_OK;
    uint32_t xlen;
    
    if(strncmp(target_name(target), "riscv", 4) == 0)
    {
        xlen = riscv_xlen(target);
        init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
        init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
        init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
        init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);
        if(xlen == 32)
        {
            target_code_src = riscv32_bin;
            target_code_size = sizeof(riscv32_bin);
        }
        else
        {
            target_code_src = riscv64_bin;
            target_code_size = sizeof(riscv64_bin);
        }
    }
    else
    {
        xlen = 64;
        init_reg_param(&reg_params[0], "x0", xlen, PARAM_IN_OUT);
        init_reg_param(&reg_params[1], "x1", xlen, PARAM_OUT);
        init_reg_param(&reg_params[2], "x2", xlen, PARAM_OUT);
        init_reg_param(&reg_params[3], "x3", xlen, PARAM_OUT);

        target_code_src = aarch64_bin;
        target_code_size = sizeof(aarch64_bin);
    }

    // copy code to work area
    if(loader->op != TARGET_EMMC_WRITE || !loader->copy_area)
    {
        // LOG_INFO("target wr code size %x to workarea", target_code_size);
        retval = target_code_to_working_area(target, target_code_src, target_code_size, 
        loader->chunk_size, &loader->copy_area);

        if(retval != ERROR_OK)
            return retval;
    }
    target_buf = loader->copy_area->address + target_code_size;
    loader->op = TARGET_EMMC_WRITE;
    // copy data to work area

    retval = target_write_buffer(target, target_buf, size, data);

    if(retval != ERROR_OK)
        return retval;

    // LOG_INFO("target set reg parm ctrl base " TARGET_ADDR_FMT ,loader->ctrl_base);
    // LOG_INFO("target set reg parm offset %llx", offset);
    // LOG_INFO("target set reg parm buf base " TARGET_ADDR_FMT , target_buf);
    // LOG_INFO("target set reg parm size %x", size);
    
    buf_set_u64(reg_params[0].value, 0, xlen, loader->ctrl_base);
    buf_set_u64(reg_params[1].value, 0, xlen, offset);
    buf_set_u64(reg_params[2].value, 0, xlen, target_buf);
    buf_set_u64(reg_params[3].value, 0, xlen, size);

    return ERROR_OK;
}


int target_emmc_write(struct target_emmc_loader *loader, uint8_t *data, target_addr_t offset, int size)
{
    struct target *target = loader->target;
    struct reg_param reg_params[4];
    int retval = ERROR_OK;
    // set up algorithm
    retval = target_set_algorithm(loader, data, offset, size, reg_params);

    // run algorithm
    retval = target_run_algorithm(target, 0, NULL, 
                        ARRAY_SIZE(reg_params), reg_params, 
                        loader->copy_area->address, 0, 10000, NULL);
	destroy_reg_param(&reg_params[0]);
	destroy_reg_param(&reg_params[1]);
	destroy_reg_param(&reg_params[2]);
    destroy_reg_param(&reg_params[3]);

    if(retval != ERROR_OK){
        LOG_ERROR("error executing hosted EMMC write");
    }

    // target_free_working_area(target, loader->copy_area);
    return retval;
}

