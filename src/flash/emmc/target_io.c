
#include "target_io.h"


int target_sel_code(struct target_emmc_loader *loader, struct target_code_srcs srcs, struct reg_param* reg_params, uint32_t block_size)
{
    struct target *target = loader->target;
    uint32_t code_size;    
    if(strncmp(target_name(target), "riscv", 4) == 0)
    {
        loader->xlen = riscv_xlen(target);
        init_reg_param(&reg_params[0], "a0", loader->xlen, PARAM_IN_OUT);
        init_reg_param(&reg_params[1], "a1", loader->xlen, PARAM_OUT);
        init_reg_param(&reg_params[2], "a2", loader->xlen, PARAM_OUT);
        init_reg_param(&reg_params[3], "a3", loader->xlen, PARAM_OUT);
        if(loader->xlen == 32)
        {
        loader->code_src = srcs.riscv32_bin;
            code_size = srcs.riscv32_size;
        }
        else
        {
            loader->code_src = srcs.riscv64_bin;
            code_size = srcs.riscv64_size;
        }
    }
    else
    {
        loader->xlen = 64;
        init_reg_param(&reg_params[0], "x0", loader->xlen, PARAM_IN_OUT);
        init_reg_param(&reg_params[1], "x1", loader->xlen, PARAM_OUT);
        init_reg_param(&reg_params[2], "x2", loader->xlen, PARAM_OUT);
        init_reg_param(&reg_params[3], "x3", loader->xlen, PARAM_OUT);

        loader->code_src = srcs.aarch64_bin;
        code_size = srcs.aarch64_size;
    }

    loader->code_size = (code_size/block_size +1) * block_size;


    return ERROR_OK;
}


static int target_code_to_working_area(struct target_emmc_loader * loader)
{
	int retval;
	unsigned wa_size = loader->code_size + loader->data_size;
    struct working_area **area = &(loader->copy_area);
    struct target* target = loader->target;

	/* REVISIT this assumes size doesn't ever change.
	 * That's usually correct; but there are boards with
	 * both large and small page chips, where it won't be...
	 */

	/* make sure we have a working area */
	if (!*area) {
		retval = target_alloc_working_area(target, wa_size, area);
		if (retval != ERROR_OK) {
			LOG_INFO("%s: no %d byte buffer", __func__, (int) wa_size);
			return ERROR_EMMC_NO_BUFFER;
		}
	}
    // LOG_INFO("workarea addr " TARGET_ADDR_FMT, (*area)->address);
    target_write_buffer(target, (*area)->address, loader->code_size, loader->code_src);

	return retval;
}

static int target_set_wa(struct target_emmc_loader *loader, uint8_t *data, target_addr_t addr)
{
    struct target *target = loader->target;
    target_addr_t  data_wa;
    int retval = ERROR_OK;    

    // copy code to work area
    if(loader->op != TARGET_EMMC_WRITE || !loader->copy_area)
    {
        // LOG_DEBUG("target wr code size %x to workarea", loader->code_size);
        retval = target_code_to_working_area(loader);

        if(retval != ERROR_OK)
            return retval;
    }
    data_wa = loader->copy_area->address + loader->code_size;
    loader->op = TARGET_EMMC_WRITE;
    // copy data to work area

    retval = target_write_buffer(target, data_wa, loader->data_size, data);

    if(retval != ERROR_OK)
        return retval;
    
    buf_set_u64(loader->reg_params[0].value, 0, loader->xlen, loader->ctrl_base);
    buf_set_u64(loader->reg_params[1].value, 0, loader->xlen, addr);
    buf_set_u64(loader->reg_params[2].value, 0, loader->xlen, data_wa);
    buf_set_u64(loader->reg_params[3].value, 0, loader->xlen, loader->data_size);

    // LOG_DEBUG("target set reg parm ctrl base " TARGET_ADDR_FMT ,loader->ctrl_base);
    // LOG_DEBUG("target set reg parm addr %llx", addr);
    // LOG_DEBUG("target set reg parm buf base " TARGET_ADDR_FMT , data_wa);
    // LOG_DEBUG("target set reg parm size %x", loader->data_size);

    return ERROR_OK;
}


int target_emmc_write(struct target_emmc_loader *loader, uint8_t *data, target_addr_t addr)
{
    struct target *target = loader->target;
    int retval = ERROR_OK;
    // set up algorithm
    retval = target_set_wa(loader, data, addr);

    // run algorithm
    retval = target_run_algorithm(target, 0, NULL, 
                        4, loader->reg_params, 
                        loader->copy_area->address, 0, 10000, NULL);

    if(retval != ERROR_OK){
        LOG_ERROR("error executing hosted EMMC write");
    }
    return retval;
}
