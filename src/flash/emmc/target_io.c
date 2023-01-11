
#include "target_io.h"


int target_set_arch_info(struct target_emmc_loader *loader, struct aarch64_algorithm* arm_info, struct riscv_algorithm* riscv_info)
{
    struct target *target = loader->target;
    if(strncmp(target_name(target), "riscv", 4) == 0)
    {
        loader->arch = TARGET_RISCV;        
        loader->arch_info = riscv_info;
    }
    else{
        arm_info->common_magic = AARCH64_COMMON_MAGIC;
        arm_info->core_mode = ARMV8_64_EL0T;
        loader->arch = TARGET_AARCH64;
        loader->arch_info = arm_info;
    }
    return ERROR_OK;
}


int target_sel_code(struct target_emmc_loader *loader, struct target_code_srcs srcs, struct reg_param* reg_params, uint32_t block_size)
{
    struct target *target = loader->target;
    uint32_t code_size;    
    if(loader->arch == TARGET_RISCV)
    {
        loader->xlen = riscv_xlen(target);
        init_reg_param(&reg_params[0], "a0", loader->xlen, PARAM_IN_OUT);
        init_reg_param(&reg_params[1], "a1", loader->xlen, PARAM_OUT);
        init_reg_param(&reg_params[2], "a2", loader->xlen, PARAM_OUT);
        init_reg_param(&reg_params[3], "a3", loader->xlen, PARAM_OUT);
        if(loader->async == ASYNC_TRANS)
        {
            init_reg_param(&reg_params[4], "a4", loader->xlen, PARAM_OUT);
        }

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
        if(loader->async == ASYNC_TRANS)
        {
            init_reg_param(&reg_params[4], "x4", loader->xlen, PARAM_OUT);
        }

        loader->code_src = srcs.aarch64_bin;
        code_size = srcs.aarch64_size;
    }

    loader->code_area = (code_size/block_size +1) * block_size;
    loader->code_size = code_size;

    // LOG_INFO("code size %x padded size %x", code_size, loader->code_area);

    return ERROR_OK;
}


static int target_code_to_working_area(struct target_emmc_loader * loader)
{
	int retval;
	unsigned wa_size;
    struct working_area **area = &(loader->copy_area);
    struct target* target = loader->target;

	/* REVISIT this assumes size doesn't ever change.
	 * That's usually correct; but there are boards with
	 * both large and small page chips, where it won't be...
	 */
    wa_size = target_get_working_area_avail(target);

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

    data_wa = loader->copy_area->address + loader->code_area;
    loader->op = TARGET_EMMC_WRITE;
    // copy data to work area

    retval = target_write_buffer(target, data_wa, loader->data_size, data);

    if(retval != ERROR_OK)
        return retval;

    buf_set_u64(loader->reg_params[0].value, 0, loader->xlen, loader->ctrl_base);
    buf_set_u64(loader->reg_params[1].value, 0, loader->xlen, addr);
    buf_set_u64(loader->reg_params[2].value, 0, loader->xlen, data_wa);
    buf_set_u64(loader->reg_params[3].value, 0, loader->xlen, loader->data_size);


    // LOG_INFO("target set reg parm ctrl base %llx",*(uint64_t *) loader->reg_params[0].value);
    // LOG_INFO("target set reg parm addr %llx", *(uint64_t *) loader->reg_params[1].value);
    // LOG_INFO("target set reg parm buf base %llx", *(uint64_t *) loader->reg_params[2].value);
    // LOG_INFO("target set reg parm size %llx", *(uint64_t *) loader->reg_params[3].value);

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
                        loader->copy_area->address, 0, 10000, loader->arch_info);

    if(retval != ERROR_OK){
        LOG_ERROR("error executing target hosted sync EMMC write");
    }
    return retval;
}

static int target_set_wa_async(struct target_emmc_loader *loader, uint32_t block_size, target_addr_t addr)
{
    target_addr_t fifo_end;
    int retval = ERROR_OK;    

    // copy code to work area
    if(loader->op != TARGET_EMMC_WRITE || !loader->copy_area)
    {
        LOG_DEBUG("target wr code size %x to workarea", loader->code_size);
        retval = target_code_to_working_area(loader);

        if(retval != ERROR_OK)
            return retval;
    }

    loader->buf_start = loader->copy_area->address + loader->code_area;
    loader->data_size = (loader->data_size/block_size - 1) * block_size + 8 ; //update data size for async write
    fifo_end = loader->buf_start + loader->data_size;
    loader->op = TARGET_EMMC_WRITE;

    buf_set_u64(loader->reg_params[0].value, 0, loader->xlen, loader->ctrl_base);
    buf_set_u64(loader->reg_params[1].value, 0, loader->xlen, loader->image_block_cnt);
    buf_set_u64(loader->reg_params[2].value, 0, loader->xlen, loader->buf_start);
    buf_set_u64(loader->reg_params[3].value, 0, loader->xlen, fifo_end);
    buf_set_u64(loader->reg_params[4].value, 0, loader->xlen, addr);

    // LOG_INFO("target set reg parm ctrl base " TARGET_ADDR_FMT ,loader->ctrl_base);
    // LOG_INFO("target set reg parm img block cnt %x" , loader->image_block_cnt);
    // LOG_INFO("target set reg parm buf start %x", loader->buf_start);
    // LOG_INFO("target set reg parm buf end %llx", fifo_end);
    // LOG_INFO("target set reg parm addr %llx", addr);
    return ERROR_OK;
}

int target_emmc_write_async(struct target* trans_target, struct target_emmc_loader *loader, uint8_t *data, target_addr_t addr)
{
    struct target *exec_target = loader->target;
    int retval = ERROR_OK;
    retval = target_set_wa_async(loader, loader->block_size, addr);

    retval = target_run_async_algorithm(trans_target, exec_target, data, loader->image_block_cnt, 512, 
        0, NULL,
        5, loader->reg_params, 
        loader->buf_start, loader->data_size, loader->copy_area->address, 0, loader->arch_info);

    if(retval != ERROR_OK){
        LOG_ERROR("error executing target hosted async EMMC write");
    }
    return retval;
}
