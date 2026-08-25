/***************************************************************************
 *   Copyright (C) 2011 by Broadcom Corporation                            *
 *   Evan Hunter - ehunter@broadcom.com                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <helper/time_support.h>
#include <jtag/jtag.h>
#include "target/target.h"
#include "target/target_type.h"
#include "rtos.h"
#include "helper/log.h"
#include "helper/types.h"
#include "rtos_standard_stackings.h"
#include "target/armv7m.h"
#include "target/armv8.h"
#include "target/cortex_m.h"
#include "target/cortex_a.h"
#include "target/register.h"
#include "target/smp.h"
#include "server/gdb_server.h"

#define FREERTOS_MAX_PRIORITIES	63
#define FREERTOS_THREAD_NAME_STR_SIZE 16
#define FREERTOS_THREAD_LOCAL_SIZE 5

struct freertos_params {
	const char *target_name;
	int (*stacking)(struct rtos *rtos, const struct rtos_register_stacking **stacking,
					target_addr_t stack_ptr);
	const struct command_registration *commands;
};

struct freertos_thread_entry {
	struct list_head list;
	threadid_t threadid;
	target_addr_t tcb;
	int core_id;
	uint64_t tcb_number_gen;
	bool tcb_number_valid;
};

struct FreeRTOS {
	const struct freertos_params *param;
	threadid_t last_threadid;
	/* Keep track of which threadid we're using for which TCB.  We cannot use a
	 * 1:1 mapping because TCB's can be 64 bits, and the gdb protocol doesn't
	 * work well with thread id's that are greater than 32 bits.
	 */
	struct list_head thread_entry_list;
	/* sizeof(UBaseType_t) */
	unsigned ubasetype_size;
	/* sizeof(void *) */
	unsigned pointer_size;
	unsigned list_width;
	unsigned list_item_width;
	unsigned list_elem_next_offset;
	unsigned list_elem_next_size;
	unsigned list_elem_content_offset;
	unsigned list_elem_content_size;
	unsigned list_uxNumberOfItems_offset;
	unsigned list_uxNumberOfItems_size;
	unsigned list_next_offset;
	unsigned list_next_size;
	unsigned thread_stack_offset;
	unsigned thread_stack_size;
	unsigned thread_name_offset;
	unsigned thread_priority_offset;
	unsigned thread_priority_size;
	unsigned thread_base_priority_offset;
	unsigned thread_base_priority_size;
	unsigned thread_run_state_offset;
	unsigned thread_run_state_size;
	unsigned thread_critical_nesting_offset;
	unsigned thread_critical_nesting_size;
	unsigned thread_tcb_number_offset;
	unsigned thread_tcb_number_size;
	unsigned thread_task_number_offset;
	unsigned thread_task_number_size;
};

static int cortex_m_stacking(struct rtos *rtos, const struct rtos_register_stacking **stacking,
							 target_addr_t stack_ptr)
{
	/* Check for armv7m with *enabled* FPU, i.e. a Cortex-M4F */
	int cm4_fpu_enabled = 0;
	struct armv7m_common *armv7m_target = target_to_armv7m(rtos->target);
	if (is_armv7m(armv7m_target)) {
		if (armv7m_target->fp_feature == FPV4_SP) {
			/* Found ARM v7m target which includes a FPU */
			uint32_t cpacr;

			int retval = target_read_u32(rtos->target, FPU_CPACR, &cpacr);
			if (retval != ERROR_OK) {
				LOG_ERROR("Could not read CPACR register to check FPU state");
				return retval;
			}

			/* Check if CP10 and CP11 are set to full access. */
			if (cpacr & 0x00F00000) {
				/* Found target with enabled FPU */
				cm4_fpu_enabled = 1;
			}
		}
	}

	if (cm4_fpu_enabled == 1) {
		/* Read the LR to decide between stacking with or without FPU */
		uint32_t LR_svc = 0;
		int retval = target_read_u32(rtos->target,
				stack_ptr + 0x20,
				&LR_svc);
		if (retval != ERROR_OK) {
			LOG_OUTPUT("Error reading stack frame from FreeRTOS thread");
			return retval;
		}
		if ((LR_svc & 0x10) == 0)
			*stacking = &rtos_standard_cortex_m4f_fpu_stacking;
		else
			*stacking = &rtos_standard_cortex_m4f_stacking;
	} else {
		*stacking = &rtos_standard_cortex_m3_stacking;
	}

	return ERROR_OK;
}

static int cortex_a_stacking(struct rtos *rtos,
	const struct rtos_register_stacking **stacking,
	target_addr_t stack_ptr)
{
	(void)rtos;
	(void)stack_ptr;
	*stacking = &rtos_standard_cortex_a_fpu_stacking;
	return ERROR_OK;
}

static int nds32_stacking(struct rtos *rtos, const struct rtos_register_stacking **stacking,
							 target_addr_t stack_ptr)
{
	*stacking = &rtos_standard_nds32_n1068_stacking;
	return ERROR_OK;
}

static enum {
	STACKING_MAINLINE,
	STACKING_METAL
} riscv_freertos_stacking;
COMMAND_HANDLER(handle_riscv_freertos_stacking)
{
	if (CMD_ARGC != 1) {
		LOG_ERROR("Command takes exactly 1 parameter");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	if (!strcmp(CMD_ARGV[0], "mainline")) {
		riscv_freertos_stacking = STACKING_MAINLINE;
	} else if (!strcmp(CMD_ARGV[0], "metal")) {
		riscv_freertos_stacking = STACKING_METAL;
	} else {
		LOG_ERROR("Only two arguments are supported: mainline and metal");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	return ERROR_OK;
}

static const struct command_registration riscv_commands[] = {
	{
		.name = "riscv_freertos_stacking",
		.handler = handle_riscv_freertos_stacking,
		.mode = COMMAND_ANY,
		.usage = "mainline|metal",
		.help = "Select which FreeRTOS branch is being used. OpenOCD needs to "
		"know because different branches save thread registers on the stack "
		"in different orders. It is likely that this order on both branches will "
		"change in the future, so make sure to seek out the very latest OpenOCD if "
		"debugging is not working right."
	},
	COMMAND_REGISTRATION_DONE
};

static int riscv_stacking(struct rtos *rtos, const struct rtos_register_stacking **stacking,
							 target_addr_t stack_ptr)
{
	struct FreeRTOS *freertos = (struct FreeRTOS *) rtos->rtos_specific_params;
	LOG_DEBUG("riscv_freertos_stacking=%d", riscv_freertos_stacking);
	switch (riscv_freertos_stacking) {
		case STACKING_MAINLINE:
			if (freertos->pointer_size == 4)
				*stacking = &rtos_standard_rv32_stacking;
			else if (freertos->pointer_size == 8)
				*stacking = &rtos_standard_rv64_stacking;
			break;
		case STACKING_METAL:
			if (freertos->pointer_size == 4)
				*stacking = &rtos_metal_rv32_stacking;
			else if (freertos->pointer_size == 8)
				*stacking = &rtos_metal_rv64_stacking;
			break;
	}
	return ERROR_OK;
}

static const struct freertos_params freertos_params_list[] = {
	{
	.target_name = "cortex_m",
	.stacking = cortex_m_stacking
	},
	{
	.target_name = "hla_target",
	.stacking = cortex_m_stacking
	},
	{
	.target_name = "nds32_v3",
	.stacking = nds32_stacking,
	},
	{
	.target_name = "riscv",
	.stacking = riscv_stacking,
	.commands = riscv_commands,
	},
	{
	.target_name = "aarch64",
	.stacking = cortex_a_stacking
	},
};

static bool freertos_detect_rtos(struct target *target);
static int freertos_create(struct target *target);
static int freertos_update_threads(struct rtos *rtos);
static int freertos_get_thread_reg_list(struct rtos *rtos, threadid_t thread_id,
		struct rtos_reg **reg_list, int *num_regs);
static int freertos_get_thread_reg_value(struct rtos *rtos, threadid_t thread_id,
		uint32_t reg_num, uint32_t *size, uint8_t **value);
static int freertos_set_reg(struct rtos *rtos, uint32_t reg_num, uint8_t *reg_value);
static int freertos_smp_init(struct target *target);
static int freertos_clean(struct target *target);
static int freertos_get_symbol_list_to_lookup(struct symbol_table_elem *symbol_list[]);
static bool freertos_needs_fake_step(struct target *target, threadid_t thread_id);

struct rtos_type freertos_rtos = {
	.name = "FreeRTOS",

	.detect_rtos = freertos_detect_rtos,
	.create = freertos_create,
	.smp_init = freertos_smp_init,
	.update_threads = freertos_update_threads,
	.get_thread_reg_list = freertos_get_thread_reg_list,
	.get_thread_reg_value = freertos_get_thread_reg_value,
	.set_reg = freertos_set_reg,
	.clean = freertos_clean,
	.needs_fake_step = freertos_needs_fake_step,
	.get_symbol_list_to_lookup = freertos_get_symbol_list_to_lookup,
};

enum freertos_symbol_values {
	FREERTOS_VAL_PX_CURRENT_TCB = 0,
	FREERTOS_VAL_PX_READY_TASKS_LISTS = 1,
	FREERTOS_VAL_X_DELAYED_TASK_LIST1 = 2,
	FREERTOS_VAL_X_DELAYED_TASK_LIST2 = 3,
	FREERTOS_VAL_PX_DELAYED_TASK_LIST = 4,
	FREERTOS_VAL_PX_OVERFLOW_DELAYED_TASK_LIST = 5,
	FREERTOS_VAL_X_PENDING_READY_LIST = 6,
	FREERTOS_VAL_X_TASKS_WAITING_TERMINATION = 7,
	FREERTOS_VAL_X_SUSPENDED_TASK_LIST = 8,
	FREERTOS_VAL_UX_CURRENT_NUMBER_OF_TASKS = 9,
	FREERTOS_VAL_UX_TOP_USED_PRIORITY = 10,
	FREERTOS_VAL_PX_CURRENT_TCBS = 11,
	FREERTOS_VAL_OPENOCD_LAYOUT = 12,
};

struct symbols {
	const char *name;
	bool optional;
};

static const struct symbols freertos_symbol_list[] = {
	{ "pxCurrentTCB", false },
	{ "pxReadyTasksLists", false },
	{ "xDelayedTaskList1", false },
	{ "xDelayedTaskList2", false },
	{ "pxDelayedTaskList", false },
	{ "pxOverflowDelayedTaskList", false },
	{ "xPendingReadyList", false },
	{ "xTasksWaitingTermination", true }, /* Only if INCLUDE_vTaskDelete */
	{ "xSuspendedTaskList", true }, /* Only if INCLUDE_vTaskSuspend */
	{ "uxCurrentNumberOfTasks", false },
	{ "uxTopUsedPriority", true }, /* Unavailable since v7.5.3 */
	{ "pxCurrentTCBs", true }, /* FreeRTOS SMP */
	{ "FreeRTOSOpenOCDLayout", true },
	{ NULL, false }
};

/* TODO: */
/* this is not safe for little endian yet */
/* may be problems reading if sizes are not 32 bit long integers. */
/* test mallocs for failure */

static int freertos_read_struct_value(
	struct target *target, target_addr_t base_address, unsigned offset,
	unsigned size_bytes, uint64_t *value)
{
	uint8_t buf[size_bytes];
	int retval = target_read_buffer(target, base_address + offset, size_bytes, buf);
	*value = buf_get_u64(buf, 0, size_bytes * 8);
	return retval;
}

static char *freertos_sanitize_thread_name(const char *raw_name, threadid_t threadid)
{
	size_t in_len = strnlen(raw_name, FREERTOS_THREAD_NAME_STR_SIZE);
	char *out = calloc(in_len * 6 + 1, 1);
	if (!out)
		return NULL;

	char *p = out;
	for (size_t i = 0; i < in_len; i++) {
		unsigned char c = (unsigned char)raw_name[i];
		switch (c) {
		case '&':
			p += sprintf(p, "&amp;");
			break;
		case '<':
			p += sprintf(p, "&lt;");
			break;
		case '>':
			p += sprintf(p, "&gt;");
			break;
		case '\"':
			p += sprintf(p, "&quot;");
			break;
		case '\'':
			p += sprintf(p, "&apos;");
			break;
		default:
			if ((c >= 0x20 && c <= 0x7e) || c == '\t')
				*p++ = (char)c;
			else
				*p++ = '?';
			break;
		}
	}

	if (p == out)
		p += sprintf(p, "Task_%" PRId64, threadid);

	return out;
}

static uint64_t freertos_read_tcb_field(struct rtos *rtos,
	struct FreeRTOS *freertos, target_addr_t tcb,
	unsigned offset, unsigned size, bool *ok)
{
	uint64_t value = 0;
	*ok = false;
	if (!offset && !size)
		return 0;
	if (freertos_read_struct_value(rtos->target, tcb, offset, size, &value) == ERROR_OK)
		*ok = true;
	return value;
}

static char *freertos_build_extra_info(struct rtos *rtos,
	struct FreeRTOS *freertos,
	struct freertos_thread_entry *entry,
	const char *state_str)
{
	bool ok_prio, ok_base_prio, ok_tcbno, ok_taskno, ok_sp;
	uint64_t prio = freertos_read_tcb_field(rtos, freertos, entry->tcb,
		freertos->thread_priority_offset, freertos->thread_priority_size, &ok_prio);
	uint64_t base_prio = freertos_read_tcb_field(rtos, freertos, entry->tcb,
		freertos->thread_base_priority_offset, freertos->thread_base_priority_size, &ok_base_prio);
	uint64_t tcb_number = freertos_read_tcb_field(rtos, freertos, entry->tcb,
		freertos->thread_tcb_number_offset, freertos->thread_tcb_number_size, &ok_tcbno);
	uint64_t task_number = freertos_read_tcb_field(rtos, freertos, entry->tcb,
		freertos->thread_task_number_offset, freertos->thread_task_number_size, &ok_taskno);
	target_addr_t stack_ptr = (target_addr_t)freertos_read_tcb_field(rtos, freertos, entry->tcb,
		freertos->thread_stack_offset, freertos->thread_stack_size, &ok_sp);

	char buf[420];
	snprintf(buf, sizeof(buf),
		"State: %s, Prio: %s%" PRIu64 ", BasePrio: %s%" PRIu64
		", TCBNo: %s%" PRIu64 ", TaskNo: %s%" PRIu64
		", SP: %s0x%" TARGET_PRIxADDR,
		state_str,
		ok_prio ? "" : "?", prio,
		ok_base_prio ? "" : "?", base_prio,
		ok_tcbno ? "" : "?", tcb_number,
		ok_taskno ? "" : "?", task_number,
		ok_sp ? "" : "?", stack_ptr);

	return strdup(buf);
}

static int freertos_set_synthetic_current_execution(struct rtos *rtos)
{
	int64_t saved_threadid = rtos->current_threadid;
	rtos_free_threadlist(rtos);
	rtos->current_threadid = saved_threadid;
	rtos->thread_details = calloc(1, sizeof(struct thread_detail));
	if (!rtos->thread_details)
		return ERROR_FAIL;

	rtos->thread_count = 1;
	rtos->thread_details[0].threadid = 1;
	rtos->thread_details[0].exists = true;
	rtos->thread_details[0].thread_name_str = strdup("Current Execution");
	rtos->thread_details[0].extra_info_str = NULL;
	rtos->current_thread = 1;
	if (rtos->current_threadid <= 0)
		rtos->current_threadid = rtos->current_thread;
	if (!rtos->thread_details[0].thread_name_str)
		return ERROR_FAIL;
	return ERROR_OK;
}

typedef struct {
	enum {
		TYPE_POINTER,
		TYPE_UBASE,
		TYPE_TICKTYPE,
		TYPE_LIST_ITEM,
		TYPE_CHAR_ARRAY,
		TYPE_U32
	} type;
	unsigned offset;
	unsigned size;
} type_offset_size_t;

static unsigned populate_offset_size(struct FreeRTOS *freertos,
									 type_offset_size_t *info, unsigned count)
{
	unsigned offset = 0;
	unsigned largest = 0;
	for (unsigned i = 0; i < count; i++) {
		unsigned align = 0;
		switch (info[i].type) {
			case TYPE_UBASE:
				info[i].size = freertos->ubasetype_size;
				align = freertos->ubasetype_size;
				break;
			case TYPE_POINTER:
				info[i].size = freertos->pointer_size;
				align = freertos->pointer_size;
				break;
			case TYPE_TICKTYPE:
				info[i].size = 8;
				align = 8;
				break;
			case TYPE_LIST_ITEM:
				info[i].size = freertos->list_item_width;
				align = MAX(freertos->ubasetype_size, freertos->pointer_size);
				break;
			case TYPE_CHAR_ARRAY:
				/* size is set by the caller. */
				align = 1;
				break;
			case TYPE_U32:
				info[i].size = 4;
				align = 8;
				break;
		}

		assert(info[i].size > 0);
		assert(align > 0);

		largest = MAX(largest, align);

		if (offset & (align - 1)) {
			offset = offset & ~(align - 1);
			offset += align;
		}

		info[i].offset = offset;
		offset += info[i].size;
	}

	/* Now align offset to the largest type used, and return that as the width
	 * of the structure. */

	if (offset & (largest - 1)) {
		offset = offset & ~(largest - 1);
		offset += largest;
	}
	return offset;
}

struct freertos_openocd_layout_v1 {
	uint32_t magic;
	uint32_t version;
	uint32_t pointer_size;
	uint32_t ubasetype_size;
	uint32_t ticktype_size;
	uint32_t list_size;
	uint32_t list_item_size;
	uint32_t list_ux_items_offset;
	uint32_t list_next_offset;
	uint32_t list_elem_next_offset;
	uint32_t list_elem_owner_offset;
	uint32_t tcb_sp_offset;
	uint32_t tcb_priority_offset;
	uint32_t tcb_name_offset;
	uint32_t tcb_run_state_offset;
	uint32_t tcb_critical_nesting_offset;
	uint32_t tcb_tcb_number_offset;
	uint32_t tcb_task_number_offset;
	uint32_t tcb_base_priority_offset;
};

#define FREERTOS_OPENOCD_LAYOUT_MAGIC 0x4652544fU

static bool freertos_try_apply_layout_descriptor(struct rtos *rtos, struct FreeRTOS *freertos)
{
	if (!rtos->symbols)
		return false;

	symbol_address_t addr = rtos->symbols[FREERTOS_VAL_OPENOCD_LAYOUT].address;
	if (!addr)
		return false;

	struct freertos_openocd_layout_v1 d;
	int retval = target_read_buffer(rtos->target, addr, sizeof(d), (uint8_t *)&d);
	if (retval != ERROR_OK)
		return false;

	if (d.magic != FREERTOS_OPENOCD_LAYOUT_MAGIC || d.version != 1)
		return false;

	if (d.pointer_size == 0 || d.ubasetype_size == 0 || d.ticktype_size == 0)
		return false;

	freertos->pointer_size = d.pointer_size;
	freertos->ubasetype_size = d.ubasetype_size;
	freertos->list_width = d.list_size;
	freertos->list_item_width = d.list_item_size;
	freertos->list_uxNumberOfItems_offset = d.list_ux_items_offset;
	freertos->list_uxNumberOfItems_size = d.ubasetype_size;
	freertos->list_next_offset = d.list_next_offset;
	freertos->list_next_size = d.pointer_size;
	freertos->list_elem_next_offset = d.list_elem_next_offset;
	freertos->list_elem_next_size = d.pointer_size;
	freertos->list_elem_content_offset = d.list_elem_owner_offset;
	freertos->list_elem_content_size = d.pointer_size;

	freertos->thread_stack_offset = d.tcb_sp_offset;
	freertos->thread_stack_size = d.pointer_size;
	freertos->thread_priority_offset = d.tcb_priority_offset;
	freertos->thread_priority_size = d.ubasetype_size;
	freertos->thread_name_offset = d.tcb_name_offset;
	freertos->thread_run_state_offset = d.tcb_run_state_offset;
	freertos->thread_run_state_size = d.ubasetype_size;
	freertos->thread_critical_nesting_offset = d.tcb_critical_nesting_offset;
	freertos->thread_critical_nesting_size = d.ubasetype_size;
	freertos->thread_tcb_number_offset = d.tcb_tcb_number_offset;
	freertos->thread_tcb_number_size = d.ubasetype_size;
	freertos->thread_task_number_offset = d.tcb_task_number_offset;
	freertos->thread_task_number_size = d.ubasetype_size;
	freertos->thread_base_priority_offset = d.tcb_base_priority_offset;
	freertos->thread_base_priority_size = d.ubasetype_size;

	LOG_INFO("FreeRTOS: using ABI descriptor FreeRTOSOpenOCDLayout @ 0x%" PRIx64
		" (ptr=%u, ubase=%u, list=%u, list_item=%u)",
		addr, freertos->pointer_size, freertos->ubasetype_size,
		freertos->list_width, freertos->list_item_width);
	return true;
}

static void freertos_compute_offsets(struct rtos *rtos)
{
	struct FreeRTOS *freertos = (struct FreeRTOS *) rtos->rtos_specific_params;

	if (freertos->pointer_size != 0) {
		LOG_DEBUG("FreeRTOS: offsets already computed (pointer_size=%u, ubasetype_size=%u, list_width=%u)",
			freertos->pointer_size, freertos->ubasetype_size, freertos->list_width);
		return;
	}

	if (freertos_try_apply_layout_descriptor(rtos, freertos))
		return;

	freertos->pointer_size = 8;
	freertos->ubasetype_size = 8;
	LOG_INFO("FreeRTOS: target bits addr=%d data=%d => pointer_size=%u ubasetype_size=%u",
		target_address_bits(rtos->target), target_data_bits(rtos->target),
		freertos->pointer_size, freertos->ubasetype_size);

	/*
	 * FreeRTOS can be compiled with configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES
	 * in which case extra data is inserted and OpenOCD won't work right.
	 */

	 /* struct xLIST_ITEM  ListItem_t*/
	type_offset_size_t struct_list_item_info[] = {
		{TYPE_TICKTYPE, 0, 0},	/* TickType_t xItemValue */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxNext */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxPrevious */
		{TYPE_POINTER, 0, 0},	/* void *pvOwner */
		{TYPE_POINTER, 0, 0},	/* List_t *pvContainer */
	};

	/* struct xLIST List_t */
	type_offset_size_t struct_list_info[] = {
		{TYPE_UBASE, 0, 0},		/* UBaseType_t uxNumberOfItems */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxIndex */
		{TYPE_TICKTYPE, 0, 0},	/* TickType_t xItemValue */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxNext */
		{TYPE_POINTER, 0, 0},	/* ListItem_t *pxPrevious */
	};

	/* struct tskTaskControlBlock */
	type_offset_size_t task_control_block_info_smp[] = {
		{TYPE_POINTER, 0, 0},		/* StackType_t *pxTopOfStack */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxCoreAffinityMask */
		{TYPE_LIST_ITEM, 0, 0},		/* ListItem_t xStateListItem */
		{TYPE_LIST_ITEM, 0, 0},		/* ListItem_t xEventListItem */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxPriority */
		{TYPE_POINTER, 0, 0},		/* StackType_t *pxStack */
		{TYPE_UBASE, 0, 0},			/* BaseType_t xTaskRunState */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxTaskAttributes */
		{TYPE_CHAR_ARRAY, 0, FREERTOS_THREAD_NAME_STR_SIZE},	/* char pcTaskName[configMAX_TASK_NAME_LEN] */
		{TYPE_UBASE, 0, 0},			/* BaseType_t xPreemptionDisable */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxCriticalNesting */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxTCBNumber */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxTaskNumber */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxBasePriority */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxMutexesHeld */
		/* void * pvThreadLocalStoragePointers[FREERTOS_THREAD_LOCAL_SIZE] */
		{TYPE_UBASE, 0, FREERTOS_THREAD_LOCAL_SIZE},
		{TYPE_U32, 0, 0},			/* UBaseType_t ulRunTimeCounter */
	};

	type_offset_size_t task_control_block_info[] = {
		{TYPE_POINTER, 0, 0},		/* StackType_t *pxTopOfStack */
		{TYPE_LIST_ITEM, 0, 0},		/* ListItem_t xStateListItem */
		{TYPE_LIST_ITEM, 0, 0},		/* ListItem_t xEventListItem */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxPriority */
		{TYPE_POINTER, 0, 0},		/* StackType_t *pxStack */
		/* char pcTaskName[configMAX_TASK_NAME_LEN] */
		{TYPE_CHAR_ARRAY, 0, FREERTOS_THREAD_NAME_STR_SIZE},
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxTCBNumber */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxTaskNumber */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxBasePriority */
		{TYPE_UBASE, 0, 0},			/* UBaseType_t uxMutexesHeld */
	};

	freertos->list_item_width = populate_offset_size(
		freertos, struct_list_item_info, ARRAY_SIZE(struct_list_item_info));
	freertos->list_elem_next_offset = struct_list_item_info[1].offset;
	freertos->list_elem_next_size = struct_list_item_info[1].size;
	freertos->list_elem_content_offset = struct_list_item_info[3].offset;
	freertos->list_elem_content_size = struct_list_item_info[3].size;

	freertos->list_width = populate_offset_size(
		freertos, struct_list_info, ARRAY_SIZE(struct_list_info));
	freertos->list_uxNumberOfItems_offset = struct_list_info[0].offset;
	freertos->list_uxNumberOfItems_size = struct_list_info[0].size;
	freertos->list_next_offset = struct_list_info[3].offset;
	freertos->list_next_size = struct_list_info[3].size;

	if (rtos->target->smp) {
		populate_offset_size(
			freertos, task_control_block_info_smp, ARRAY_SIZE(task_control_block_info_smp));
		freertos->thread_stack_offset = task_control_block_info_smp[0].offset;
		freertos->thread_stack_size = task_control_block_info_smp[0].size;
		freertos->thread_priority_offset = task_control_block_info_smp[4].offset;
		freertos->thread_priority_size = task_control_block_info_smp[4].size;
		freertos->thread_run_state_offset = task_control_block_info_smp[6].offset;
		freertos->thread_run_state_size = task_control_block_info_smp[6].size;
		freertos->thread_name_offset = task_control_block_info_smp[8].offset;
		freertos->thread_critical_nesting_offset = task_control_block_info_smp[10].offset;
		freertos->thread_critical_nesting_size = task_control_block_info_smp[10].size;
		freertos->thread_tcb_number_offset = task_control_block_info_smp[11].offset;
		freertos->thread_tcb_number_size = task_control_block_info_smp[11].size;
		freertos->thread_task_number_offset = task_control_block_info_smp[12].offset;
		freertos->thread_task_number_size = task_control_block_info_smp[12].size;
		freertos->thread_base_priority_offset = task_control_block_info_smp[13].offset;
		freertos->thread_base_priority_size = task_control_block_info_smp[13].size;
	} else {
		populate_offset_size(
			freertos, task_control_block_info, ARRAY_SIZE(task_control_block_info));
		freertos->thread_stack_offset = task_control_block_info[0].offset;
		freertos->thread_stack_size = task_control_block_info[0].size;
		freertos->thread_priority_offset = task_control_block_info[3].offset;
		freertos->thread_priority_size = task_control_block_info[3].size;
		freertos->thread_name_offset = task_control_block_info[5].offset;
		freertos->thread_tcb_number_offset = task_control_block_info[6].offset;
		freertos->thread_tcb_number_size = task_control_block_info[6].size;
		freertos->thread_task_number_offset = task_control_block_info[7].offset;
		freertos->thread_task_number_size = task_control_block_info[7].size;
		freertos->thread_base_priority_offset = task_control_block_info[8].offset;
		freertos->thread_base_priority_size = task_control_block_info[8].size;
	}

	LOG_INFO("FreeRTOS: computed offsets list_width=%u list_item_width=%u",
		freertos->list_width, freertos->list_item_width);
	LOG_INFO("FreeRTOS: list offsets uxNum(off=%u,size=%u) next(off=%u,size=%u) elem_next(off=%u,size=%u) elem_owner(off=%u,size=%u)",
		freertos->list_uxNumberOfItems_offset, freertos->list_uxNumberOfItems_size,
		freertos->list_next_offset, freertos->list_next_size,
		freertos->list_elem_next_offset, freertos->list_elem_next_size,
		freertos->list_elem_content_offset, freertos->list_elem_content_size);
	LOG_INFO("FreeRTOS: tcb offsets sp(off=%u,size=%u) name(off=%u)",
		freertos->thread_stack_offset, freertos->thread_stack_size,
		freertos->thread_name_offset);
	if (rtos->target->smp) {
		LOG_INFO("FreeRTOS: tcb meta offsets prio(off=%u,size=%u) base_prio(off=%u,size=%u) run_state(off=%u,size=%u) cn(off=%u,size=%u) tcbno(off=%u,size=%u) taskno(off=%u,size=%u)",
			freertos->thread_priority_offset, freertos->thread_priority_size,
			freertos->thread_base_priority_offset, freertos->thread_base_priority_size,
			freertos->thread_run_state_offset, freertos->thread_run_state_size,
			freertos->thread_critical_nesting_offset, freertos->thread_critical_nesting_size,
			freertos->thread_tcb_number_offset, freertos->thread_tcb_number_size,
			freertos->thread_task_number_offset, freertos->thread_task_number_size);
	} else {
		LOG_INFO("FreeRTOS: tcb meta offsets prio(off=%u,size=%u) base_prio(off=%u,size=%u) tcbno(off=%u,size=%u) taskno(off=%u,size=%u)",
			freertos->thread_priority_offset, freertos->thread_priority_size,
			freertos->thread_base_priority_offset, freertos->thread_base_priority_size,
			freertos->thread_tcb_number_offset, freertos->thread_tcb_number_size,
			freertos->thread_task_number_offset, freertos->thread_task_number_size);
	}
}

struct freertos_thread_entry *thread_entry_list_find_by_tcb(
	struct list_head *list, target_addr_t tcb)
{
	struct freertos_thread_entry *t;
	list_for_each_entry(t, list, list) {
		if (t->tcb == tcb)
			return t;
	}
	return NULL;
}

struct freertos_thread_entry *thread_entry_list_find_by_id(
	struct list_head *list, threadid_t threadid)
{
	struct freertos_thread_entry *t;
	list_for_each_entry(t, list, list) {
		if (t->threadid == threadid)
			return t;
	}
	return NULL;
}

static int freertos_update_entry_generation(struct rtos *rtos,
	struct FreeRTOS *freertos,
	struct freertos_thread_entry *entry)
{
	if (!freertos->thread_tcb_number_size || !freertos->thread_tcb_number_offset)
		return ERROR_OK;

	uint64_t gen = 0;
	int retval = freertos_read_struct_value(rtos->target, entry->tcb,
		freertos->thread_tcb_number_offset,
		freertos->thread_tcb_number_size,
		&gen);
	if (retval != ERROR_OK)
		return retval;

	if (!entry->tcb_number_valid) {
		entry->tcb_number_gen = gen;
		entry->tcb_number_valid = true;
		return ERROR_OK;
	}

	if (entry->tcb_number_gen != gen) {
		entry->tcb_number_gen = gen;
		entry->threadid = ++freertos->last_threadid;
		entry->core_id = -1;
		LOG_DEBUG("FreeRTOS: TCB reuse detected at 0x%" TARGET_PRIxADDR
			" -> new tid=%" PRId64 " (uxTCBNumber=%" PRIu64 ")",
			entry->tcb, entry->threadid, gen);
	}

	return ERROR_OK;
}

static struct freertos_thread_entry *thread_entry_list_get_or_add(
	struct FreeRTOS *freertos, target_addr_t tcb)
{
	struct freertos_thread_entry *entry =
		thread_entry_list_find_by_tcb(&freertos->thread_entry_list, tcb);
	if (entry) {
		LOG_DEBUG("FreeRTOS: Reusing thread mapping TCB=0x%" TARGET_PRIxADDR " -> tid=%" PRId64,
			tcb, entry->threadid);
		return entry;
	}

	entry = calloc(1, sizeof(struct freertos_thread_entry));
	if (!entry)
		return NULL;

	entry->tcb = tcb;
	entry->threadid = ++freertos->last_threadid;
	entry->core_id = -1;
	list_add_tail(&entry->list, &freertos->thread_entry_list);
	LOG_DEBUG("FreeRTOS: New thread mapping TCB=0x%" TARGET_PRIxADDR " -> tid=%" PRId64,
		tcb, entry->threadid);

	return entry;
}

static int thread_detail_index_by_id(struct thread_detail *details,
	unsigned int count, threadid_t threadid)
{
	for (unsigned int i = 0; i < count; i++) {
		if (details[i].exists && details[i].threadid == threadid)
			return i;
	}
	return -1;
}

static struct target *freertos_find_smp_target_by_coreid(struct target *target, int core_id)
{
	if (!target->smp)
		return target;

	struct target_list *head;
	foreach_smp_target(head, target->smp_targets) {
		if ((int)head->target->coreid == core_id)
			return head->target;
	}

	return NULL;
}

static int freertos_get_live_reg_list_from_target(struct target *target,
	struct rtos_reg **reg_list, int *num_regs)
{
	struct reg **gdb_reg_list;
	int gdb_reg_count;
	int retval = target_get_gdb_reg_list(target, &gdb_reg_list, &gdb_reg_count,
		REG_CLASS_GENERAL);
	if (retval != ERROR_OK)
		return retval;

	int visible_count = 0;
	for (int i = 0; i < gdb_reg_count; i++) {
		if (!gdb_reg_list[i] || gdb_reg_list[i]->exist == false || gdb_reg_list[i]->hidden)
			continue;
		visible_count++;
	}

	*reg_list = calloc(visible_count, sizeof(struct rtos_reg));
	if (!*reg_list) {
		free(gdb_reg_list);
		return ERROR_FAIL;
	}

	int out = 0;
	for (int i = 0; i < gdb_reg_count; i++) {
		if (!gdb_reg_list[i] || gdb_reg_list[i]->exist == false || gdb_reg_list[i]->hidden)
			continue;

		if (!gdb_reg_list[i]->valid) {
			retval = gdb_reg_list[i]->type->get(gdb_reg_list[i]);
			if (retval != ERROR_OK) {
				free(*reg_list);
				*reg_list = NULL;
				free(gdb_reg_list);
				return retval;
			}
		}

		unsigned bytes = DIV_ROUND_UP(gdb_reg_list[i]->size, 8);
		if (bytes > sizeof((*reg_list)[out].value)) {
			free(*reg_list);
			*reg_list = NULL;
			free(gdb_reg_list);
			return ERROR_FAIL;
		}

		(*reg_list)[out].number = i;
		(*reg_list)[out].size = gdb_reg_list[i]->size;
		memcpy((*reg_list)[out].value, gdb_reg_list[i]->value, bytes);
		out++;
	}

	*num_regs = visible_count;
	free(gdb_reg_list);
	return ERROR_OK;
}

static int freertos_get_live_reg_value_from_target(struct target *target,
	uint32_t reg_num, uint32_t *size, uint8_t **value)
{
	struct reg **reg_list;
	int reg_count;
	int retval = target_get_gdb_reg_list_noread(target, &reg_list, &reg_count,
		REG_CLASS_ALL);
	if (retval != ERROR_OK)
		return retval;

	if ((int)reg_num >= reg_count || !reg_list[reg_num] || reg_list[reg_num]->exist == false) {
		free(reg_list);
		return ERROR_FAIL;
	}

	if (!reg_list[reg_num]->valid) {
		retval = reg_list[reg_num]->type->get(reg_list[reg_num]);
		if (retval != ERROR_OK) {
			free(reg_list);
			return retval;
		}
	}

	*size = reg_list[reg_num]->size;
	unsigned bytes = DIV_ROUND_UP(*size, 8);
	*value = malloc(bytes);
	if (!*value) {
		free(reg_list);
		return ERROR_FAIL;
	}

	memcpy(*value, reg_list[reg_num]->value, bytes);
	free(reg_list);
	return ERROR_OK;
}

static int freertos_target_for_threadid(struct connection *connection,
	threadid_t threadid, struct target **p_target)
{
	struct target *target = get_target_from_connection(connection);
	struct rtos *rtos = rtos_of_target(target);
	LOG_DEBUG("FreeRTOS: route threadid=%" PRId64 " on target '%s' (smp=%d)",
		threadid, target_name(target), target->smp ? 1 : 0);

	if (!rtos || !target->smp || !rtos->rtos_specific_params) {
		*p_target = target;
		LOG_DEBUG("FreeRTOS: route fallback to current target (no rtos/smp/params)");
		return ERROR_OK;
	}

	struct FreeRTOS *freertos = (struct FreeRTOS *)rtos->rtos_specific_params;
	struct freertos_thread_entry *entry =
		thread_entry_list_find_by_id(&freertos->thread_entry_list, threadid);
	if (!entry || entry->core_id < 0) {
		*p_target = target;
		LOG_DEBUG("FreeRTOS: threadid=%" PRId64 " has no running core mapping, fallback", threadid);
		return ERROR_OK;
	}

	struct target_list *head;
	foreach_smp_target(head, target->smp_targets) {
		if ((int)head->target->coreid == entry->core_id) {
			*p_target = head->target;
			LOG_DEBUG("FreeRTOS: threadid=%" PRId64 " routed to coreid=%d target '%s'",
				threadid, entry->core_id, target_name(*p_target));
			return ERROR_OK;
		}
	}

	*p_target = target;
	LOG_DEBUG("FreeRTOS: core mapping not found in SMP list, fallback to current target");
	return ERROR_OK;
}

static int freertos_update_threads(struct rtos *rtos)
{
	int retval;
	unsigned int tasks_found = 0;
	LOG_DEBUG("FreeRTOS: update_threads enter (target=%s, smp=%d)",
		target_name(rtos->target), rtos->target->smp ? 1 : 0);

	if (!rtos->rtos_specific_params)
		return ERROR_FAIL;

	freertos_compute_offsets(rtos);

	struct FreeRTOS *freertos = (struct FreeRTOS *) rtos->rtos_specific_params;

	if (!rtos->symbols) {
		LOG_ERROR("No symbols for FreeRTOS");
		return ERROR_FAIL;
	}

	if (rtos->symbols[FREERTOS_VAL_UX_CURRENT_NUMBER_OF_TASKS].address == 0) {
		LOG_ERROR("Don't have the number of threads in FreeRTOS");
		return ERROR_FAIL;
	}

	uint64_t thread_list_size;
	retval = freertos_read_struct_value(rtos->target,
										rtos->symbols[FREERTOS_VAL_UX_CURRENT_NUMBER_OF_TASKS].address,
										0,
										freertos->ubasetype_size,
										&thread_list_size);
	LOG_DEBUG("FreeRTOS: Read uxCurrentNumberOfTasks at 0x%" PRIx64 ", value %" PRIu64,
										rtos->symbols[FREERTOS_VAL_UX_CURRENT_NUMBER_OF_TASKS].address,
										thread_list_size);

	if (retval != ERROR_OK) {
		LOG_ERROR("Could not read FreeRTOS thread count from target");
		return retval;
	}

	if (thread_list_size > 1024) {
		LOG_WARNING("FreeRTOS: uxCurrentNumberOfTasks=%" PRIu64
			" out of expected range, using synthetic Current Execution", thread_list_size);
		return freertos_set_synthetic_current_execution(rtos);
	}

	int64_t current_threadid = rtos->current_threadid;
	rtos_free_threadlist(rtos);
	rtos->current_threadid = current_threadid;

	uint64_t max_tasks = thread_list_size;
	if (max_tasks == 0)
		max_tasks = 1;
	LOG_DEBUG("FreeRTOS: reserving thread detail slots=%" PRIu64, max_tasks);

	rtos->thread_details = calloc(max_tasks, sizeof(struct thread_detail));
	if (!rtos->thread_details) {
		LOG_ERROR("Error allocating memory for %" PRIu64 " threads", max_tasks);
		return ERROR_FAIL;
	}

	struct freertos_thread_entry *entry;
	list_for_each_entry(entry, &freertos->thread_entry_list, list)
		entry->core_id = -1;

	unsigned int active_cores = 1;
	if (rtos->target->smp) {
		active_cores = 0;
		struct target_list *head;
		foreach_smp_target(head, rtos->target->smp_targets) {
			if (target_was_examined(head->target))
				active_cores++;
		}
		if (active_cores == 0)
			active_cores = 1;
	}
	LOG_DEBUG("FreeRTOS: active examined cores=%u", active_cores);

	target_addr_t *running_tcbs = calloc(active_cores, sizeof(target_addr_t));
	int *running_coreids = calloc(active_cores, sizeof(int));
	if (!running_tcbs || !running_coreids) {
		free(running_tcbs);
		free(running_coreids);
		return ERROR_FAIL;
	}

	unsigned int running_count = 0;
	if (rtos->target->smp && rtos->symbols[FREERTOS_VAL_PX_CURRENT_TCBS].address != 0) {
		LOG_DEBUG("FreeRTOS: using SMP symbol pxCurrentTCBs @ 0x%" PRIx64,
			rtos->symbols[FREERTOS_VAL_PX_CURRENT_TCBS].address);
		unsigned int idx = 0;
		struct target_list *head;
		foreach_smp_target(head, rtos->target->smp_targets) {
			if (!target_was_examined(head->target))
				continue;

			target_addr_t tcb = 0;
			retval = freertos_read_struct_value(rtos->target,
				rtos->symbols[FREERTOS_VAL_PX_CURRENT_TCBS].address + idx * freertos->pointer_size,
				0, freertos->pointer_size, &tcb);
			if (retval != ERROR_OK) {
				free(running_tcbs);
				free(running_coreids);
				return retval;
			}
			running_tcbs[running_count] = tcb;
			running_coreids[running_count] = (int)head->target->coreid;
			LOG_DEBUG("FreeRTOS: pxCurrentTCBs[%u] coreid=%d -> TCB=0x%" TARGET_PRIxADDR,
				idx, running_coreids[running_count], tcb);
			running_count++;
			idx++;
		}
	} else if (rtos->symbols[FREERTOS_VAL_PX_CURRENT_TCB].address != 0) {
		LOG_INFO("FreeRTOS: using UP symbol pxCurrentTCB @ 0x%" PRIx64,
			rtos->symbols[FREERTOS_VAL_PX_CURRENT_TCB].address);
		target_addr_t pxCurrentTCB = 0;
		retval = freertos_read_struct_value(rtos->target,
			rtos->symbols[FREERTOS_VAL_PX_CURRENT_TCB].address,
			0, freertos->pointer_size, &pxCurrentTCB);
		if (retval != ERROR_OK) {
			free(running_tcbs);
			free(running_coreids);
			LOG_ERROR("Error reading current thread in FreeRTOS thread list");
			return retval;
		}
		running_tcbs[0] = pxCurrentTCB;
		running_coreids[0] = (int)rtos->target->coreid;
		running_count = 1;
		LOG_DEBUG("FreeRTOS: pxCurrentTCB -> coreid=%d TCB=0x%" TARGET_PRIxADDR,
			running_coreids[0], running_tcbs[0]);
	}
	LOG_DEBUG("FreeRTOS: running_count=%u", running_count);
	int halted_core_id = -1;
	if (rtos->halted_target)
		halted_core_id = (int)rtos->halted_target->coreid;

	rtos->current_thread = 0;
	for (unsigned int i = 0; i < running_count && tasks_found < max_tasks; i++) {
		if (running_tcbs[i] == 0)
			continue;

		entry = thread_entry_list_get_or_add(freertos, running_tcbs[i]);
		if (!entry) {
			free(running_tcbs);
			free(running_coreids);
			return ERROR_FAIL;
		}
		retval = freertos_update_entry_generation(rtos, freertos, entry);
		if (retval != ERROR_OK) {
			free(running_tcbs);
			free(running_coreids);
			return retval;
		}

		entry->core_id = running_coreids[i];
		LOG_DEBUG("FreeRTOS: running thread tid=%" PRId64 " tcb=0x%" TARGET_PRIxADDR " core=%d",
			entry->threadid, entry->tcb, entry->core_id);
		int existing_idx = thread_detail_index_by_id(rtos->thread_details, tasks_found, entry->threadid);
		if (existing_idx >= 0) {
			LOG_DEBUG("FreeRTOS: skip duplicate running tid=%" PRId64 " at idx=%d",
				entry->threadid, existing_idx);
			continue;
		}

		char tmp_str[FREERTOS_THREAD_NAME_STR_SIZE];
		retval = target_read_buffer(rtos->target,
			entry->tcb + freertos->thread_name_offset,
			FREERTOS_THREAD_NAME_STR_SIZE,
			(uint8_t *)&tmp_str);
		if (retval != ERROR_OK) {
			free(running_tcbs);
			free(running_coreids);
			return retval;
		}
		tmp_str[FREERTOS_THREAD_NAME_STR_SIZE - 1] = '\x00';
		if (tmp_str[0] == '\x00')
			strcpy(tmp_str, "No Name");
		LOG_DEBUG("FreeRTOS: raw name tid=%" PRId64 " tcb=0x%" TARGET_PRIxADDR " first16='%.*s'",
			entry->threadid, entry->tcb, 16, tmp_str);

		rtos->thread_details[tasks_found].threadid = entry->threadid;
		rtos->thread_details[tasks_found].exists = true;
		rtos->thread_details[tasks_found].thread_name_str =
			freertos_sanitize_thread_name(tmp_str, entry->threadid);
		if (!rtos->thread_details[tasks_found].thread_name_str) {
			free(running_tcbs);
			free(running_coreids);
			return ERROR_FAIL;
		}

		char running_str[64];
		if (rtos->target->smp)
			snprintf(running_str, sizeof(running_str), "Running, Core: %d", entry->core_id);
		else
			snprintf(running_str, sizeof(running_str), "Running");
		rtos->thread_details[tasks_found].extra_info_str =
			freertos_build_extra_info(rtos, freertos, entry, running_str);
		if (!rtos->thread_details[tasks_found].extra_info_str) {
			free(running_tcbs);
			free(running_coreids);
			return ERROR_FAIL;
		}

		if (entry->core_id == halted_core_id)
			rtos->current_thread = entry->threadid;
		else if (rtos->current_threadid == entry->threadid || rtos->current_thread == 0)
			rtos->current_thread = entry->threadid;
		tasks_found++;
	}

	uint64_t top_used_priority = 0;
	if (rtos->symbols[FREERTOS_VAL_UX_TOP_USED_PRIORITY].address == 0) {
		LOG_WARNING("FreeRTOS: uxTopUsedPriority is not defined, consult the OpenOCD manual for a work-around");
		top_used_priority = 6;
	} else {
		retval = freertos_read_struct_value(rtos->target,
			rtos->symbols[FREERTOS_VAL_UX_TOP_USED_PRIORITY].address,
			0, freertos->ubasetype_size, &top_used_priority);
		if (retval != ERROR_OK) {
			free(running_tcbs);
			free(running_coreids);
			LOG_WARNING("FreeRTOS: cannot read uxTopUsedPriority, using synthetic Current Execution");
			return freertos_set_synthetic_current_execution(rtos);
		}
	}

	if (top_used_priority > FREERTOS_MAX_PRIORITIES) {
		free(running_tcbs);
		free(running_coreids);
		LOG_WARNING("FreeRTOS top used priority is unreasonably big (%" PRIu64 "), using synthetic Current Execution",
					top_used_priority);
		return freertos_set_synthetic_current_execution(rtos);
	}

	unsigned int config_max_priorities = top_used_priority + 1;
	LOG_DEBUG("FreeRTOS: top_used_priority=%" PRIu64 ", config_max_priorities=%u",
		top_used_priority, config_max_priorities);
	symbol_address_t *list_of_lists =
		malloc(sizeof(symbol_address_t) * (config_max_priorities + 5));
	if (!list_of_lists) {
		free(running_tcbs);
		free(running_coreids);
		return ERROR_FAIL;
	}

	unsigned int num_lists;
	for (num_lists = 0; num_lists < config_max_priorities; num_lists++)
		list_of_lists[num_lists] = rtos->symbols[FREERTOS_VAL_PX_READY_TASKS_LISTS].address +
		num_lists * freertos->list_width;
	LOG_DEBUG("FreeRTOS: pxReadyTasksLists=0x%" PRIx64 ", list_width=%u, config_max_priorities=%u",
		rtos->symbols[FREERTOS_VAL_PX_READY_TASKS_LISTS].address,
		freertos->list_width, config_max_priorities);

	list_of_lists[num_lists++] = rtos->symbols[FREERTOS_VAL_X_DELAYED_TASK_LIST1].address;
	list_of_lists[num_lists++] = rtos->symbols[FREERTOS_VAL_X_DELAYED_TASK_LIST2].address;
	list_of_lists[num_lists++] = rtos->symbols[FREERTOS_VAL_X_PENDING_READY_LIST].address;
	list_of_lists[num_lists++] = rtos->symbols[FREERTOS_VAL_X_SUSPENDED_TASK_LIST].address;
	list_of_lists[num_lists++] = rtos->symbols[FREERTOS_VAL_X_TASKS_WAITING_TERMINATION].address;

	for (unsigned int i = 0; i < num_lists; i++) {
		if (list_of_lists[i] == 0 || tasks_found >= max_tasks)
			continue;
		LOG_DEBUG("FreeRTOS: scanning list[%u] @ 0x%" PRIx64, i, list_of_lists[i]);
		const char *list_state = "Unknown";
		if (i < config_max_priorities)
			list_state = "Ready";
		else if (i == config_max_priorities || i == config_max_priorities + 1)
			list_state = "Blocked";
		else if (i == config_max_priorities + 2)
			list_state = "PendingReady";
		else if (i == config_max_priorities + 3)
			list_state = "Suspended";
		else if (i == config_max_priorities + 4)
			list_state = "Deleted";

		uint64_t list_thread_count = 0;
		retval = freertos_read_struct_value(rtos->target,
			list_of_lists[i],
			freertos->list_uxNumberOfItems_offset,
			freertos->list_uxNumberOfItems_size,
			&list_thread_count);
		if (retval != ERROR_OK) {
			free(list_of_lists);
			free(running_tcbs);
			free(running_coreids);
			return retval;
		}
		LOG_DEBUG("FreeRTOS: list[%u] thread_count=%" PRIu64 " (uxNumberOfItems off=%u size=%u)",
			i, list_thread_count,
			freertos->list_uxNumberOfItems_offset, freertos->list_uxNumberOfItems_size);

		target_addr_t prev_list_elem_ptr = -1;
		target_addr_t list_elem_ptr = 0;
		retval = freertos_read_struct_value(rtos->target,
			list_of_lists[i],
			freertos->list_next_offset,
			freertos->list_next_size,
			&list_elem_ptr);
		if (retval != ERROR_OK) {
			free(list_of_lists);
			free(running_tcbs);
			free(running_coreids);
			return retval;
		}
		LOG_DEBUG("FreeRTOS: list[%u] first_elem=0x%" TARGET_PRIxADDR " (next off=%u size=%u)",
			i, list_elem_ptr, freertos->list_next_offset, freertos->list_next_size);

		unsigned int list_iter = 0;

		while ((list_thread_count > 0) && (list_elem_ptr != 0) &&
			(list_elem_ptr != prev_list_elem_ptr) &&
			(tasks_found < max_tasks)) {
			list_iter++;
			if ((list_iter & 0x3f) == 0) {
				LOG_WARNING("FreeRTOS: list[%u] long walk iter=%u remaining=%" PRIu64
					", curr=0x%" TARGET_PRIxADDR ", prev=0x%" TARGET_PRIxADDR,
					i, list_iter, list_thread_count, list_elem_ptr, prev_list_elem_ptr);
			}
			target_addr_t tcb;
			retval = freertos_read_struct_value(rtos->target,
				list_elem_ptr,
				freertos->list_elem_content_offset,
				freertos->list_elem_content_size,
				&tcb);
			if (retval != ERROR_OK) {
				free(list_of_lists);
				free(running_tcbs);
				free(running_coreids);
				return retval;
			}

			entry = thread_entry_list_get_or_add(freertos, tcb);
			if (!entry) {
				free(list_of_lists);
				free(running_tcbs);
				free(running_coreids);
				return ERROR_FAIL;
			}
			retval = freertos_update_entry_generation(rtos, freertos, entry);
			if (retval != ERROR_OK) {
				free(list_of_lists);
				free(running_tcbs);
				free(running_coreids);
				return retval;
			}

			LOG_DEBUG("FreeRTOS: list[%u] elem tcb=0x%" TARGET_PRIxADDR " -> tid=%" PRId64,
				i, tcb, entry->threadid);
			if (thread_detail_index_by_id(rtos->thread_details, tasks_found, entry->threadid) < 0) {
				char tmp_str[FREERTOS_THREAD_NAME_STR_SIZE];
				retval = target_read_buffer(rtos->target,
					entry->tcb + freertos->thread_name_offset,
					FREERTOS_THREAD_NAME_STR_SIZE,
					(uint8_t *)&tmp_str);
				if (retval != ERROR_OK) {
					free(list_of_lists);
					free(running_tcbs);
					free(running_coreids);
					return retval;
				}
				tmp_str[FREERTOS_THREAD_NAME_STR_SIZE - 1] = '\x00';
				if (tmp_str[0] == '\x00')
					strcpy(tmp_str, "No Name");
				LOG_DEBUG("FreeRTOS: raw name tid=%" PRId64 " tcb=0x%" TARGET_PRIxADDR " first16='%.*s'",
					entry->threadid, entry->tcb, 16, tmp_str);

				rtos->thread_details[tasks_found].threadid = entry->threadid;
				rtos->thread_details[tasks_found].exists = true;
				rtos->thread_details[tasks_found].thread_name_str =
					freertos_sanitize_thread_name(tmp_str, entry->threadid);
				if (!rtos->thread_details[tasks_found].thread_name_str) {
					free(list_of_lists);
					free(running_tcbs);
					free(running_coreids);
					return ERROR_FAIL;
				}

				rtos->thread_details[tasks_found].extra_info_str =
					freertos_build_extra_info(rtos, freertos, entry, list_state);
				if (!rtos->thread_details[tasks_found].extra_info_str) {
					free(list_of_lists);
					free(running_tcbs);
					free(running_coreids);
					return ERROR_FAIL;
				}

				tasks_found++;
			} else {
				LOG_DEBUG("FreeRTOS: skip duplicate listed tid=%" PRId64, entry->threadid);
			}

			list_thread_count--;
			prev_list_elem_ptr = list_elem_ptr;
			list_elem_ptr = 0;
			retval = freertos_read_struct_value(rtos->target,
				prev_list_elem_ptr,
				freertos->list_elem_next_offset,
				freertos->list_elem_next_size,
				&list_elem_ptr);
			if (retval != ERROR_OK) {
				free(list_of_lists);
				free(running_tcbs);
				free(running_coreids);
				return retval;
			}
		}
	}

	free(list_of_lists);
	free(running_tcbs);
	free(running_coreids);

	rtos->thread_count = tasks_found;
	if (rtos->thread_count == 0) {
		rtos->thread_count = 1;
		rtos->thread_details[0].threadid = 1;
		rtos->thread_details[0].exists = true;
		rtos->thread_details[0].thread_name_str = strdup("Current Execution");
		rtos->thread_details[0].extra_info_str = NULL;
		rtos->current_thread = 1;
	} else if (rtos->current_thread == 0) {
		rtos->current_thread = rtos->thread_details[0].threadid;
	}

	LOG_INFO("FreeRTOS: update_threads done, thread_count=%d, current_thread=%" PRId64,
		rtos->thread_count, rtos->current_thread);

	return ERROR_OK;
}

static int freertos_get_stacking_info(struct rtos *rtos, threadid_t thread_id,
									  const struct rtos_register_stacking **stacking_info,
									  target_addr_t *stack_ptr)
{
	if (!rtos->rtos_specific_params) {
		LOG_ERROR("rtos_specific_params is NULL!");
		return ERROR_FAIL;
	}

	freertos_compute_offsets(rtos);

	struct FreeRTOS *freertos = (struct FreeRTOS *) rtos->rtos_specific_params;
	const struct freertos_params *param = freertos->param;

	const struct freertos_thread_entry *entry =
		thread_entry_list_find_by_id(&freertos->thread_entry_list, thread_id);
	if (!entry) {
		LOG_ERROR("Unknown thread id: %" PRId64, thread_id);
		return ERROR_FAIL;
	}

	/* Read the stack pointer */
	int retval = freertos_read_struct_value(rtos->target,
											entry->tcb,
											freertos->thread_stack_offset,
											freertos->thread_stack_size,
											stack_ptr);
	if (retval != ERROR_OK) {
		LOG_ERROR("Error reading stack frame from FreeRTOS thread %" PRIx64, thread_id);
		return retval;
	}
	LOG_DEBUG("[%" PRId64 "] FreeRTOS: Read stack pointer at 0x%" PRIx64 ", value 0x%" PRIx64,
			  thread_id, entry->tcb + freertos->thread_stack_offset, *stack_ptr);

	if (param->stacking(rtos, stacking_info, *stack_ptr) != ERROR_OK) {
		LOG_ERROR("No stacking info found for %s!", param->target_name);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static int freertos_get_thread_reg_list(struct rtos *rtos, threadid_t thread_id,
		struct rtos_reg **reg_list, int *num_regs)
{
	/* Let the caller read registers directly for the current thread. */
	if (thread_id == 0)
		return ERROR_FAIL;

	if (rtos->target->smp && rtos->rtos_specific_params) {
		struct FreeRTOS *freertos = (struct FreeRTOS *)rtos->rtos_specific_params;
		const struct freertos_thread_entry *entry =
			thread_entry_list_find_by_id(&freertos->thread_entry_list, thread_id);

		if (entry && entry->core_id >= 0) {
			struct target *core_target =
				freertos_find_smp_target_by_coreid(rtos->target, entry->core_id);
			if (core_target) {
				int retval = freertos_get_live_reg_list_from_target(core_target, reg_list, num_regs);
				if (retval == ERROR_OK)
					return ERROR_OK;
			}
		}
	}

	const struct rtos_register_stacking *stacking_info;
	target_addr_t stack_ptr;
	if (freertos_get_stacking_info(rtos, thread_id, &stacking_info, &stack_ptr) != ERROR_OK)
		return ERROR_FAIL;

	return rtos_generic_stack_read(rtos->target, stacking_info, stack_ptr, reg_list, num_regs);
}

static int freertos_get_thread_reg_value(struct rtos *rtos, threadid_t thread_id,
		uint32_t reg_num, uint32_t *size, uint8_t **value)
{
	LOG_DEBUG("reg_num=%d", reg_num);
	/* Let the caller read registers directly for the current thread. */
	if (thread_id == 0)
		return ERROR_FAIL;

	if (rtos->target->smp && rtos->rtos_specific_params) {
		struct FreeRTOS *freertos = (struct FreeRTOS *)rtos->rtos_specific_params;
		const struct freertos_thread_entry *entry =
			thread_entry_list_find_by_id(&freertos->thread_entry_list, thread_id);

		if (entry && entry->core_id >= 0) {
			struct target *core_target =
				freertos_find_smp_target_by_coreid(rtos->target, entry->core_id);
			if (core_target) {
				int retval = freertos_get_live_reg_value_from_target(core_target,
					reg_num, size, value);
				if (retval == ERROR_OK)
					return ERROR_OK;
			}
		}
	}

	const struct rtos_register_stacking *stacking_info;
	target_addr_t stack_ptr;
	if (freertos_get_stacking_info(rtos, thread_id, &stacking_info, &stack_ptr) != ERROR_OK)
		return ERROR_FAIL;

	struct rtos_reg reg;
	reg.number = reg_num;
	int result = rtos_generic_stack_read_reg(rtos->target, stacking_info,
						 stack_ptr, reg_num, &reg);
	*size = reg.size;
	*value = malloc(DIV_ROUND_UP(reg.size, 8));
	if (!*value) {
		LOG_ERROR("Failed to allocate memory for %d-bit register.", reg.size);
		return ERROR_FAIL;
	}
	memcpy(*value, reg.value, DIV_ROUND_UP(reg.size, 8));
	return result;
}

static int freertos_set_reg(struct rtos *rtos, uint32_t reg_num, uint8_t *reg_value)
{
	LOG_DEBUG("[%" PRId64 "] reg_num=%" PRId32, rtos->current_threadid, reg_num);

	if (rtos->target->smp && rtos->rtos_specific_params) {
		struct FreeRTOS *freertos = (struct FreeRTOS *)rtos->rtos_specific_params;
		const struct freertos_thread_entry *entry =
			thread_entry_list_find_by_id(&freertos->thread_entry_list, rtos->current_threadid);

		if (entry && entry->core_id >= 0) {
			struct target *core_target =
				freertos_find_smp_target_by_coreid(rtos->target, entry->core_id);
			if (core_target) {
				struct reg **reg_list;
				int reg_count;
				int retval = target_get_gdb_reg_list_noread(core_target, &reg_list, &reg_count,
					REG_CLASS_ALL);
				if (retval != ERROR_OK)
					return retval;

				if ((int)reg_num >= reg_count || !reg_list[reg_num] ||
					reg_list[reg_num]->exist == false || !reg_list[reg_num]->type ||
					!reg_list[reg_num]->type->set) {
					free(reg_list);
					return ERROR_FAIL;
				}

				retval = reg_list[reg_num]->type->set(reg_list[reg_num], reg_value);
				free(reg_list);
				return retval;
			}
		}
	}

	const struct rtos_register_stacking *stacking_info;
	target_addr_t stack_ptr;
	if (freertos_get_stacking_info(rtos, rtos->current_threadid,
									&stacking_info, &stack_ptr) != ERROR_OK)
		return ERROR_FAIL;

	return rtos_generic_stack_write_reg(rtos->target, stacking_info, stack_ptr,
										reg_num, reg_value);
}

static int freertos_get_symbol_list_to_lookup(struct symbol_table_elem *symbol_list[])
{
	unsigned int i;
	*symbol_list = calloc(
			ARRAY_SIZE(freertos_symbol_list), sizeof(struct symbol_table_elem));

	for (i = 0; i < ARRAY_SIZE(freertos_symbol_list); i++) {
		(*symbol_list)[i].symbol_name = freertos_symbol_list[i].name;
		(*symbol_list)[i].optional = freertos_symbol_list[i].optional;
	}

	return 0;
}

static bool freertos_needs_fake_step(struct target *target, threadid_t thread_id)
{
	struct rtos *rtos = rtos_of_target(target);
	if (!rtos || !rtos->rtos_specific_params)
		return true;

	struct FreeRTOS *freertos = (struct FreeRTOS *)rtos->rtos_specific_params;
	struct freertos_thread_entry *entry =
		thread_entry_list_find_by_id(&freertos->thread_entry_list, thread_id);

	if (entry && entry->core_id >= 0)
		return false;

	return true;
}

static int freertos_clean(struct target *target)
{
	struct rtos *rtos = rtos_of_target(target);
	if (!rtos || !rtos->rtos_specific_params)
		return ERROR_OK;

	struct FreeRTOS* freertos = (struct FreeRTOS *)rtos->rtos_specific_params;
	struct freertos_thread_entry *entry, *tmp;
	list_for_each_entry_safe(entry, tmp, &freertos->thread_entry_list, list) {
		list_del(&entry->list);
		free(entry);
	}

	rtos_free_threadlist(rtos);
	free(freertos);
	rtos->rtos_specific_params = NULL;
	return ERROR_OK;
}

#if 0

static int freertos_set_current_thread(struct rtos *rtos, threadid_t thread_id)
{
	return 0;
}

static int freertos_get_thread_ascii_info(struct rtos *rtos, threadid_t thread_id, char **info)
{
	int retval;
	const struct freertos_params *param;

	if (!rtos)
		return -1;

	if (thread_id == 0)
		return -2;

	if (!rtos->rtos_specific_params)
		return -3;

	param = (const struct freertos_params *)rtos->rtos_specific_params;

#define FREERTOS_THREAD_NAME_STR_SIZE (200)
	char tmp_str[FREERTOS_THREAD_NAME_STR_SIZE];

	/* Read the thread name */
	retval = target_read_buffer(rtos->target,
		thread_id + param->thread_name_offset,
		FREERTOS_THREAD_NAME_STR_SIZE,
		(uint8_t *)&tmp_str);
	if (retval != ERROR_OK) {
		LOG_ERROR("Error reading first thread item location in FreeRTOS thread list");
		return retval;
	}
	tmp_str[FREERTOS_THREAD_NAME_STR_SIZE - 1] = '\x00';

	if (tmp_str[0] == '\x00')
		strcpy(tmp_str, "No Name");

	*info = malloc(strlen(tmp_str) + 1);
	strcpy(*info, tmp_str);
	return 0;
}

#endif

static bool freertos_detect_rtos(struct target *target)
{
	if (!target->rtos->symbols)
		return false;

	if (target->rtos->symbols[FREERTOS_VAL_PX_READY_TASKS_LISTS].address == 0)
		return false;

	if (target->rtos->symbols[FREERTOS_VAL_PX_CURRENT_TCB].address == 0 &&
		target->rtos->symbols[FREERTOS_VAL_PX_CURRENT_TCBS].address == 0)
		return false;

	return true;
}

static int freertos_create(struct target *target)
{
	unsigned int i = 0;
	while (i < ARRAY_SIZE(freertos_params_list) &&
			strcmp(freertos_params_list[i].target_name, target->type->name) != 0) {
		i++;
	}
	if (i >= ARRAY_SIZE(freertos_params_list)) {
		LOG_ERROR("Could not find target in FreeRTOS compatibility list");
		return ERROR_FAIL;
	}

	target->rtos->rtos_specific_params = calloc(1, sizeof(struct FreeRTOS));
	if (!target->rtos->rtos_specific_params) {
		LOG_ERROR("calloc failed");
		return ERROR_FAIL;
	}

	struct FreeRTOS *freertos = (struct FreeRTOS *) target->rtos->rtos_specific_params;
	INIT_LIST_HEAD(&freertos->thread_entry_list);
	target->rtos->gdb_target_for_threadid = freertos_target_for_threadid;

	freertos->param = &freertos_params_list[i];

	if (freertos->param->commands) {
		if (register_commands(target->rtos->cmd_ctx, NULL,
							  freertos->param->commands) != ERROR_OK)
			return ERROR_FAIL;
	}

	return ERROR_OK;
}

static int freertos_smp_init(struct target *target)
{
	struct rtos *rtos = rtos_of_target(target);
	if (!rtos)
		return ERROR_FAIL;

	return freertos_update_threads(rtos);
}
