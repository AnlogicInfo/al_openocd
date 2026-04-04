/*
 * File: loader_io.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2023-01-12
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2023-01-12
 */



#include "loader_io.h"
#include <target/image.h>
#include <helper/align.h>
#include <string.h>
#include <stdlib.h>

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
		target = get_first_target("pstap");
		if(target == NULL)
			target = get_first_target("riscv");
		loader->trans_target = loader_init_trans_target(target_name(target));
		loader->xlen = riscv_xlen(target);
		loader->arch_info = (struct riscv_algorithm *)malloc(sizeof(struct riscv_algorithm));
	} else {
		target = get_first_target("pstap");
		if(target == NULL)
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
	struct target *target = loader->exec_target;
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
	struct target *exec_target = loader->exec_target;

	if (strcmp(target_type_name(exec_target), "riscv") == 0) {
		loader_init_rv_code(loader, srcs);
		loader_init_reg_params(loader, rv_reg_params);
	} else {
		loader_init_aarch64_code(loader, srcs);
		loader_init_reg_params(loader, aarch_reg_params);
	}

	loader->code_area = DIV_ROUND_UP(loader->code_src->size, loader->block_size) * loader->block_size;

	return ERROR_OK;
}

static int loader_init_code_area(struct flash_loader *loader, struct code_src *srcs)
{
	struct target *exec_target = loader->exec_target;

	if (strcmp(target_type_name(exec_target), "riscv") == 0) {
		loader_init_rv_code(loader, srcs);
	} else {
		loader_init_aarch64_code(loader, srcs);
	}
	loader->code_area = DIV_ROUND_UP(loader->code_src->size, loader->block_size) * loader->block_size;
	return ERROR_OK;
}

int loader_load_elf(struct code_src *src, const char *filename)
{
	struct image img;
	int retval;
	uint64_t min_base = (uint64_t)(~0ULL);
	uint64_t max_end = 0;
	uint8_t *buf;
	size_t read_sz;

	LOG_INFO("load elf from %s", filename);
	memset(&img, 0, sizeof(img));
	retval = image_open(&img, filename, "elf");
	if (retval != ERROR_OK) {
		LOG_ERROR("fail to open image");
		return retval;
	}

	for (unsigned int i = 0; i < img.num_sections; i++) {
		uint64_t base = img.sections[i].base_address;
		uint64_t end = base + img.sections[i].size;
		if (base < min_base)
			min_base = base;
		if (end > max_end)
			max_end = end;
	}

	if (min_base == (uint64_t)(~0ULL) || max_end <= min_base) {
		LOG_ERROR("loader_load_elf: invalid min_base %" PRIx64 " or max_end %" PRIx64, min_base, max_end);
		image_close(&img);
		return ERROR_FAIL;
	}

	buf = malloc((size_t)(max_end - min_base));
	if (!buf) {
		LOG_ERROR("loader_load_elf: malloc failed, size %zu", (size_t)(max_end - min_base));
		image_close(&img);
		return ERROR_FAIL;
	}
	memset(buf, 0, (size_t)(max_end - min_base));

	for (unsigned int i = 0; i < img.num_sections; i++) {
		uint64_t base = img.sections[i].base_address;
		uint32_t size = img.sections[i].size;
		retval = image_read_section(&img, i, 0, size, buf + (size_t)(base - min_base), &read_sz);
		if (retval != ERROR_OK) {
			LOG_ERROR("loader_load_elf: read section %u failed, retval %d", i, retval);
			free(buf);
			image_close(&img);
			return ERROR_FAIL;
		}
	}

	src->bin = buf;
	src->size = (int)(max_end - min_base);
	LOG_INFO("loader_load_elf: code size %d", src->size);

	if (img.start_address_set && img.start_address >= min_base) {
		src->entry_offset = img.start_address - min_base;
		LOG_DEBUG("loader_load_elf: entry offset %" PRIx32, src->entry_offset);
	} else {
		src->entry_offset = 0;
	}

	image_close(&img);
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
	int wa_size;
	struct working_area **area = &(loader->copy_area);
	struct target *target = loader->exec_target;

	/* REVISIT this assumes size doesn't ever change.
	 * That's usually correct; but there are boards with
	 * both large and small page chips, where it won't be...
	 */
	wa_size = target_get_working_area_avail(target);

	if (wa_size < loader->code_src->size) {
		LOG_ERROR("working area size %d is smaller than code size %d", wa_size, loader->code_src->size);
		return ERROR_BUF_TOO_SMALL;
	}

	/* make sure we have a working area */
	if (!*area) {
		retval = target_alloc_working_area(target, wa_size, area);
		if (retval != ERROR_OK) {
			LOG_INFO("%s: no %d byte buffer", __func__, wa_size);
			return ERROR_BUF_TOO_SMALL;
		}
	}

	LOG_INFO("loader_code_to_wa: code size %d", loader->code_src->size);

	retval = target_write_buffer(target, (*area)->address, loader->code_src->size, loader->code_src->bin);
	if (retval != ERROR_OK) {
		LOG_ERROR("loader_code_to_wa: write code to working area failed");
		return retval;
	}

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
		if(loader->xlen == 32)
			LOG_DEBUG("target set %s value %x", loader->reg_params[i].reg_name ,
			*(uint32_t *)loader->reg_params[i].value);
		else
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

static void loader_set_mem_parms(struct flash_loader *loader, target_addr_t addr, int image_size, int bank_number)
{
	target_addr_t base = loader->exec_target->loader_buf_start;

	target_write_u32(loader->exec_target, 
			base + offsetof(struct loader_shmem_map, params.flash_addr), 
			addr);
	target_write_u32(loader->exec_target, 
			base + offsetof(struct loader_shmem_map, params.image_size), 
			image_size);
	target_write_u32(loader->exec_target, 
			base + offsetof(struct loader_shmem_map, params.bank_id), 
			bank_number);
}


static int loader_set_wa(struct flash_loader *loader, target_addr_t addr, const uint8_t *data)
{
	int wa_size = -1;

	if ((loader->op != LOADER_WRITE) || (!loader->copy_area)) {
		wa_size = loader_code_to_wa(loader);
		if (wa_size < 0)
			return ERROR_FAIL;
		loader->op = LOADER_WRITE;
		LOG_DEBUG("loader copy area " TARGET_ADDR_FMT " size %x", loader->copy_area->address, loader->code_area);
		/* Prefer user-configured buffer start/size if provided */
		if (loader->exec_target->loader_buf_cfg) {
			loader->buf_start = loader->exec_target->loader_buf_start;
			loader->data_size = loader->exec_target->loader_buf_size;
		} else {
			loader->buf_start = loader->copy_area->address + loader->code_area;
			if (loader->work_mode == ASYNC_TRANS) /* update data size for async write */
				loader->data_size = (((wa_size - loader->code_area)/loader->block_size) - 1) * loader->block_size + 8 ;
			else
				loader->data_size = (((wa_size - loader->code_area)/loader->block_size) - 1) * loader->block_size;
		}
		LOG_DEBUG("init loader data_size %x", loader->data_size);
	}

	loader_set_params(loader, addr);

	if (loader->work_mode == SYNC_TRANS)	{
		LOG_DEBUG("trans %x bytes; first byte %x; to buf " TARGET_ADDR_FMT,
			loader->data_size, data ? *data : 0, loader->buf_start);
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
	retval = loader_init(loader, srcs);
	if (retval != ERROR_OK)
		return ERROR_FAIL;

	while (image_size > 0) {
		loader->data_size = MIN(loader->data_size, image_size);

		loader_set_wa(loader, addr, data);
		retval = target_run_algorithm(loader->exec_target,
		0, NULL, loader->param_cnt, loader->reg_params,
		loader->copy_area->address + loader->code_src->entry_offset,
		0, (loader->data_size) * 2,
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

int loader_flash_write(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size, int bank_number)
{
	uint32_t image_block_cnt; 
	int retval;
	image_block_cnt = DIV_ROUND_UP(loader->image_size, loader->block_size);

	loader_init_arch(loader);
	loader_init_code_area(loader, srcs);

	retval = loader_code_to_wa(loader);
	if (retval < 0) {
		LOG_ERROR("loader code to wa failed");
		return ERROR_FAIL;
	}
	
	loader_set_mem_parms(loader, addr, image_size, bank_number);
	
	target_addr_t fifo_start = loader->exec_target->loader_buf_start + offsetof(struct loader_shmem_map, fifo_buffer);
	uint32_t fifo_size = loader->exec_target->loader_buf_size - offsetof(struct loader_shmem_map, fifo_buffer);

	retval = target_run_async_algorithm(loader->trans_target, loader->exec_target, 
	data, image_block_cnt, loader->block_size,
	0, NULL, 0, NULL,
	fifo_start, fifo_size,
	loader->copy_area->address + loader->code_src->entry_offset, 0, loader->arch_info);

	loader_exit(loader, RESTORE);
	return retval;
}

int loader_flash_write_pp(struct flash_loader *loader, struct code_src *srcs, const uint8_t *data, target_addr_t addr, int image_size, int bank_number)
{
	uint32_t image_block_cnt; 
	int retval;
	image_block_cnt = DIV_ROUND_UP(loader->image_size, loader->block_size);

	loader_init_arch(loader);
	loader_init_code_area(loader, srcs);

	retval = loader_code_to_wa(loader);
	if (retval < 0) {
		LOG_ERROR("loader code to wa failed");
		return ERROR_FAIL;
	}
	
	loader_set_mem_parms(loader, addr, image_size, bank_number);
	
	target_addr_t fifo_start = loader->exec_target->loader_buf_start + offsetof(struct loader_shmem_map, fifo_buffer);
	uint32_t fifo_size = loader->exec_target->loader_buf_size - offsetof(struct loader_shmem_map, fifo_buffer);
	LOG_INFO("loader_flash_write_pp: addr=" TARGET_ADDR_FMT ", image_size=%d, bank=%d",
		addr, image_size, bank_number);
	retval = target_run_async_algorithm_ping_pong(loader->trans_target, loader->exec_target, 
	data, image_block_cnt, loader->block_size,
	0, NULL, 0, NULL,
	fifo_start, fifo_size,
	loader->copy_area->address + loader->code_src->entry_offset, 0, loader->arch_info);

	loader_exit(loader, RESTORE);
	return retval;
}

int loader_flash_read_pp(struct flash_loader *loader, struct code_src *srcs, uint8_t *data, target_addr_t addr, int image_size, int bank_number)
{
	int retval;

	loader_init_arch(loader);
	loader_init_code_area(loader, srcs);

	retval = loader_code_to_wa(loader);
	if (retval < 0) {
		LOG_ERROR("loader code to wa failed");
		return ERROR_FAIL;
	}

	loader_set_mem_parms(loader, addr, image_size, bank_number);
	LOG_INFO("loader_flash_read_pp: addr=" TARGET_ADDR_FMT ", image_size=%d, bank=%d",
		addr, image_size, bank_number);

	target_addr_t base = loader->exec_target->loader_buf_start;
	target_addr_t buf0_flag_ptr = base + 0x0100;
	target_addr_t buf1_flag_ptr = base + 0x0140;
	target_addr_t ctrl_flag_ptr = base + 0x0180;
	target_addr_t section_start_ptr = base + 0x01C0;
	target_addr_t section_size_ptr = base + 0x0200;
	target_addr_t buf0_start_addr_ptr = base + 0x0240;
	target_addr_t buf1_start_addr_ptr = base + 0x0280;
	target_addr_t buf_size_ptr = base + 0x2C0;

	uint32_t buf_size = (loader->exec_target->loader_buf_size - 0x0300) / 2;
	buf_size = ALIGN_DOWN(buf_size, (uint32_t)loader->block_size);
	uint32_t buf0_addr = base + 0x0300;
	uint32_t buf1_addr = base + 0x0300 + buf_size;

	/* Initialize control block */
	target_write_u32(loader->trans_target, buf0_flag_ptr, 0);
	target_write_u32(loader->trans_target, buf1_flag_ptr, 0);
	target_write_u32(loader->trans_target, buf0_start_addr_ptr, buf0_addr);
	target_write_u32(loader->trans_target, buf1_start_addr_ptr, buf1_addr);
	target_write_u32(loader->trans_target, buf_size_ptr, buf_size);
	target_write_u32(loader->trans_target, section_start_ptr, addr);
	target_write_u32(loader->trans_target, section_size_ptr, DIV_ROUND_UP(image_size, loader->block_size));
	target_write_u32(loader->trans_target, ctrl_flag_ptr, 1);

	retval = target_start_algorithm(loader->exec_target,
		0, NULL, 0, NULL,
		loader->copy_area->address + loader->code_src->entry_offset, 0, loader->arch_info);
	if (retval != ERROR_OK) {
		LOG_ERROR("failed to start algorithm");
		loader_exit(loader, RESTORE);
		return retval;
	}

	int prod_idx = 0;
	int remaining = image_size;
	uint8_t *data_ptr = data;
	int total = image_size;
	int progressed = 0;
	LOG_PROC(0, total);
	while (remaining > 0) {
		target_addr_t flag_ptr = (prod_idx == 0) ? buf0_flag_ptr : buf1_flag_ptr;
		target_addr_t buf_addr = (prod_idx == 0) ? buf0_addr : buf1_addr;
		uint32_t flag_val = 0;

		int64_t start_ms = timeval_ms();
		while (1) {
			retval = target_read_u32(loader->trans_target, flag_ptr, &flag_val);
			if (retval != ERROR_OK) break;
			if (flag_val != 0) break;
			if (timeval_ms() - start_ms > 3000) {
				LOG_ERROR("target timeout during read");
				retval = ERROR_TARGET_TIMEOUT;
				break;
			}
			alive_sleep(1);
		}

		if (retval != ERROR_OK) break;

		if (flag_val == 0xFFFFFFFF) {
			LOG_ERROR("target reported read error");
			retval = ERROR_FAIL;
			break;
		}


		uint32_t read_bytes = flag_val * loader->block_size;
		if ((int)read_bytes > remaining) {
			read_bytes = remaining;
		}

		retval = target_read_buffer(loader->trans_target, buf_addr, read_bytes, data_ptr);
		if (retval != ERROR_OK) {
			LOG_ERROR("failed to read buffer");
			break;
		}

		target_write_u32(loader->trans_target, flag_ptr, 0);

		data_ptr += read_bytes;
		remaining -= read_bytes;
		progressed = total - remaining;
		if (progressed < 0)
			progressed = 0;
		if (progressed > total)
			progressed = total;
		LOG_PROC(progressed, total);
		prod_idx ^= 1;
	}

	int wait_retval = target_wait_algorithm(loader->exec_target,
		0, NULL, 0, NULL, 0, 3000, loader->arch_info);

	if (retval == ERROR_OK) {
		retval = wait_retval;
	}
	if (retval == ERROR_OK)
		LOG_PROC(total, total);

	loader_exit(loader, NO_RESTORE);
	return retval;
}

int loader_flash_crc_mem_parm(struct flash_loader *loader, struct code_src *srcs, target_addr_t addr, int bank_number, uint32_t* target_crc)
{
	int retval = ERROR_OK;
	struct target *exec_target = loader->exec_target;

	retval = loader_init_arch(loader);
	if (retval != ERROR_OK)
		return ERROR_FAIL;

	loader_init_code_area(loader, srcs);

	loader->param_cnt = 1;
	if (strcmp(target_type_name(exec_target), "riscv") == 0)
		loader_init_reg_params(loader, rv_reg_params);
	else
		loader_init_reg_params(loader, aarch_reg_params);

	retval = loader_code_to_wa(loader);
	if (retval < 0) {
		LOG_ERROR("loader code to wa failed");
		loader_exit(loader, RESTORE);
		return ERROR_FAIL;
	}

	loader_set_mem_parms(loader, addr, loader->image_size, bank_number);

	retval = target_run_algorithm(loader->exec_target,
		0, NULL, loader->param_cnt, loader->reg_params,
		loader->copy_area->address + loader->code_src->entry_offset,
		0, 10000,
		loader->arch_info);

	if (retval == ERROR_OK)
		*target_crc = buf_get_u32(loader->reg_params[0].value, 0, 32);

	loader_exit(loader, RESTORE);
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
	loader->buf_start, loader->data_size, loader->copy_area->address + loader->code_src->entry_offset, 0, loader->arch_info);

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
		loader->copy_area->address + loader->code_src->entry_offset,
		0, (loader->data_size) * 2,
		loader->arch_info);

	if (retval == ERROR_OK)
		*target_crc = buf_get_u32(loader->reg_params[0].value, 0, 32);

	loader_exit(loader, RESTORE);
	return retval;
}

