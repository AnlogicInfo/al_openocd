/*
 * File: target_io.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2023-01-12
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2023-01-12
 */



#include "loader_io.h"

char *rv_reg_params[] = {"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"};
char *aarch_reg_params[] = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};

struct target *loader_init_trans_target(const char* name)
{
	struct target *trans_target;
	int retval = ERROR_OK;
	trans_target = get_target(name);
	if (trans_target == NULL) {
		LOG_ERROR("get transtarget fail");
	} else {
		if (trans_target->state != TARGET_HALTED) {
			LOG_DEBUG("halt trans target");
			target_halt(trans_target);
			retval = target_wait_state(trans_target, TARGET_HALTED, 500);
		}

		if (retval == ERROR_OK)
			return trans_target;
	}

	return NULL;
}

static int loader_init_arch(struct flash_loader *loader)
{
	struct target *target = loader->exec_target;
	int retval = ERROR_OK;
	if (strcmp(target_type_name(target), "riscv") == 0) {
		loader->trans_target = loader_init_trans_target(target_name(target));
		loader->xlen = riscv_xlen(target);
		loader->arch_info = (struct riscv_algorithm *)malloc(sizeof(struct riscv_algorithm));
	} else {
		target = get_first_target("aarch64");
		if (target == NULL)
			return ERROR_FAIL;

		loader->trans_target = loader_init_trans_target(target_name(target));
		loader->xlen = 64;
		loader->arch_info = (struct aarch64_algorithm *) malloc(sizeof(struct aarch64_algorithm));
		((struct aarch64_algorithm *) loader->arch_info)->common_magic = AARCH64_COMMON_MAGIC;
		((struct aarch64_algorithm *) loader->arch_info)->core_mode = ARMV8_64_EL0T;
	}

	return retval;

}


static int loader_init_reg_params(struct flash_loader *loader, char **params_name)
{
	int i;
	enum param_direction direction;

	loader->reg_params = (struct reg_param *) malloc(sizeof(struct reg_param)*(loader->param_cnt));

	for (i = 0; i < loader->param_cnt; i++)	{
		if (i == 0)
			direction = PARAM_IN_OUT;
		else
			direction = PARAM_OUT;

		LOG_DEBUG("init parm %x dir %x", i, direction);
		init_reg_param(&loader->reg_params[i],  params_name[i], loader->xlen, direction);
	}
	return ERROR_OK;
}

static void loader_init_rv_code(struct flash_loader *loader, struct code_src *srcs)
{
	struct target *target = loader->trans_target;
	int code_index;
	loader->xlen = riscv_xlen(target);
	if (loader->xlen == 32)
		code_index = RV32_SRC;
	else
		code_index = RV64_SRC;
	loader->code_src = &srcs[code_index];
}

static void loader_init_aarch64_code(struct flash_loader *loader, struct code_src *srcs)
{
	loader->xlen = 64;
	loader->code_src = &srcs[AARCH64_SRC];
}


static int loader_init_code(struct flash_loader *loader, struct code_src *srcs)
{
	struct target *trans_target = loader->trans_target;

	if (strcmp(target_type_name(trans_target), "riscv") == 0) {
		loader_init_rv_code(loader, srcs);
		loader_init_reg_params(loader, rv_reg_params);
	} else {
		loader_init_aarch64_code(loader, srcs);
		loader_init_reg_params(loader, aarch_reg_params);
	}

	loader->code_area = DIV_ROUND_UP(loader->code_src->size, loader->block_size) * loader->block_size;

	return ERROR_OK;
}


static int loader_init(struct flash_loader *loader, struct code_src *srcs)
{
	int retval;

	retval = loader_init_arch(loader);
	if (retval != ERROR_OK)
		return ERROR_FAIL;
	loader_init_code(loader, srcs);
	return ERROR_OK;
}

static int loader_code_to_wa(struct flash_loader *loader)
{
	int retval;
	unsigned wa_size;
	struct working_area **area = &(loader->copy_area);
	struct target *target = loader->exec_target;

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
			return ERROR_BUF_TOO_SMALL;
		}
	}
	target_write_buffer(target, (*area)->address, loader->code_src->size, loader->code_src->bin);

	return wa_size;
}

static int loader_data_to_wa(struct flash_loader *loader, const uint8_t *data)
{
	int retval;

	retval = target_write_buffer(loader->trans_target, loader->buf_start, loader->data_size, data);

	if (retval != ERROR_OK)
		return retval;

	return ERROR_OK;
}

static int loader_set_params(struct flash_loader *loader, target_addr_t addr)
{
	target_addr_t buf_end = 0;

	if (loader->work_mode == SYNC_TRANS) {
		buf_set_u64(loader->reg_params[0].value, 0, loader->xlen, loader->ctrl_base);
		buf_set_u64(loader->reg_params[1].value, 0, loader->xlen, loader->block_size);
		buf_set_u64(loader->reg_params[2].value, 0, loader->xlen, addr);
		buf_set_u64(loader->reg_params[3].value, 0, loader->xlen, loader->buf_start);
		buf_set_u64(loader->reg_params[4].value, 0, loader->xlen, loader->data_size);

	} else if (loader->work_mode == ASYNC_TRANS) {
		buf_end = loader->buf_start + loader->data_size;
		buf_set_u64(loader->reg_params[0].value, 0, loader->xlen, loader->ctrl_base);
		buf_set_u64(loader->reg_params[1].value, 0, loader->xlen, loader->block_size);
		buf_set_u64(loader->reg_params[2].value, 0, loader->xlen, loader->image_size);
		buf_set_u64(loader->reg_params[3].value, 0, loader->xlen, loader->buf_start);
		buf_set_u64(loader->reg_params[4].value, 0, loader->xlen, buf_end);
		buf_set_u64(loader->reg_params[5].value, 0, loader->xlen, addr);
	} else if (loader->work_mode == CRC_CHECK) {
		buf_set_u64(loader->reg_params[0].value, 0, loader->xlen, loader->ctrl_base);
		buf_set_u64(loader->reg_params[1].value, 0, loader->xlen, loader->block_size);
		buf_set_u64(loader->reg_params[2].value, 0, loader->xlen, loader->image_size);
		buf_set_u64(loader->reg_params[3].value, 0, loader->xlen, addr);
	}

	if (loader->set_params_priv != NULL)
		loader->set_params_priv(loader);

	for (int i = 0; i < loader->param_cnt; i++) {
		LOG_DEBUG("target set %s value " TARGET_ADDR_FMT, loader->reg_params[i].reg_name ,
			*(target_addr_t *)loader->reg_params[i].value);
	}

	/*
	LOG_DEBUG("target set %s ctrl base " TARGET_ADDR_FMT, loader->reg_params[0].reg_name ,loader->ctrl_base);
	LOG_DEBUG("target set %s block size %x" , loader->reg_params[1].reg_name, loader->block_size);
	LOG_DEBUG("target set %s img block cnt %x" , loader->reg_params[2].reg_name, loader->image_size);
	LOG_DEBUG("target set %s buf start %x", loader->reg_params[3].reg_name, loader->buf_start);
	LOG_DEBUG("target set %s buf end %" PRIx64 "", loader->reg_params[4].reg_name, buf_end);
	LOG_DEBUG("target set %s addr %" PRIx64 "", loader->reg_params[5].reg_name, addr);
	*/

	return ERROR_OK;
}


static int loader_set_wa(struct flash_loader *loader, target_addr_t addr, const uint8_t *data)
{
	int wa_size = -1;

	if ((loader->op != LOADER_WRITE) || (!loader->copy_area)) {
		wa_size = loader_code_to_wa(loader);
		if (wa_size < 0)
			return ERROR_FAIL;
		loader->op = LOADER_WRITE;
		loader->buf_start = loader->copy_area->address + loader->code_area;
		if (loader->work_mode == ASYNC_TRANS) /* update data size for async write */
			loader->data_size = (((wa_size - loader->code_area)/loader->block_size) - 1) * loader->block_size + 8 ;
		else
			loader->data_size = (((wa_size - loader->code_area)/loader->block_size) - 1) * loader->block_size;
		LOG_DEBUG("init loader data_size %x", loader->data_size);
	}

	loader_set_params(loader, addr);

	if (loader->work_mode == SYNC_TRANS)	{
		LOG_DEBUG("trans %x bytes start data %x to wa %x", loader->data_size, *data, loader->buf_start);
		loader_data_to_wa(loader, data);
	}

	return ERROR_OK;
}

static void loader_exit(struct flash_loader *loader, int restore)
{
	for (int i = 0; i < loader->param_cnt; i++)
		destroy_reg_param(&loader->reg_params[i]);
	target_free_working_area_restore(loader->exec_target, loader->copy_area, restore);

	free(loader->arch_info);
}

int loader_flash_write_sync(struct flash_loader *loader, struct code_src *srcs,
		const uint8_t *data, target_addr_t addr, int image_size)
{
	int retval = ERROR_OK;
	LOG_INFO("loader write sync size %x", image_size);
	retval = loader_init(loader, srcs);
	if (retval != ERROR_OK)
		return ERROR_FAIL;

	while (image_size > 0) {
		loader->data_size = MIN(loader->data_size, image_size);

		loader_set_wa(loader, addr, data);
		retval = target_run_algorithm(loader->exec_target,
		0, NULL, loader->param_cnt, loader->reg_params,
		loader->copy_area->address,
		0, 10000,
		loader->arch_info);
		data += loader->data_size;
		addr += loader->data_size;
		image_size -= loader->data_size;
		if (retval != ERROR_OK)
			break;
	}

	loader_exit(loader, NO_RESTORE);

	return retval;
}

int loader_flash_write_async(struct flash_loader *loader, struct code_src *srcs,
		const uint8_t *data, target_addr_t addr, int image_size)
{
	uint32_t image_block_cnt;
	int retval;

	image_block_cnt = DIV_ROUND_UP(loader->image_size, loader->block_size);

	retval = loader_init(loader, srcs);
	if (retval != ERROR_OK)
		return ERROR_FAIL;
	loader_set_wa(loader, addr, data);
	retval = target_run_async_algorithm(loader->trans_target, loader->exec_target,
	data, image_block_cnt, loader->block_size,
	0, NULL, loader->param_cnt, loader->reg_params,
	loader->buf_start, loader->data_size, loader->copy_area->address, 0, loader->arch_info);

	loader_exit(loader, RESTORE);
	return retval;
};

int loader_flash_crc(struct flash_loader *loader, struct code_src *srcs, target_addr_t addr, uint32_t* target_crc)
{
	int retval = ERROR_OK;

	retval = loader_init(loader, srcs);
	if (retval != ERROR_OK)
		return ERROR_FAIL;

	loader_set_wa(loader, addr, NULL);
	retval = target_run_algorithm(loader->exec_target,
		0, NULL, loader->param_cnt, loader->reg_params,
		loader->copy_area->address,
		0, 50000,
		loader->arch_info);

	if (retval == ERROR_OK)
		*target_crc = buf_get_u32(loader->reg_params[0].value, 0, 32);


	loader_exit(loader, RESTORE);
	return retval;
}