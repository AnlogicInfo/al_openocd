/*
 * File: dwcphy.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2023-10-24
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2023-10-24
 */



#include <assert.h>
#include <stdlib.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "target/target.h"
#include "target/target_type.h"

#include <jtag/jtag.h>

#define IDCODE 0x01
#define CRSEL  0x31

#define ADDR   0b00
#define WRITE  0b01
#define DATA   0b10
#define READ   0b11


struct dwcphy {
	struct jtag_tap *tap;
};

static int dwcphy_target_create(struct target *target, Jim_Interp *interp)
{
	struct dwcphy *dwcphy = calloc(1, sizeof(struct dwcphy));

	dwcphy->tap = target->tap;
	target->arch_info = dwcphy;
	return ERROR_OK;
}

static int dwcphy_init_target(struct command_context *cmd_ctx, struct target *target)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int dwcphy_arch_state(struct target *target)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}


static int dwcphy_poll(struct target *target)
{
	if ((target->state == TARGET_UNKNOWN) ||
		(target->state == TARGET_RUNNING) ||
		(target->state == TARGET_DEBUG_RUNNING))
		target->state = TARGET_HALTED;

	return ERROR_OK;
}


static int dwcphy_halt(struct target *target)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int dwcphy_resume(struct target *target, int current, target_addr_t address,
		int handle_breakpoints, int debug_execution)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int dwcphy_step(struct target *target, int current, target_addr_t address,
				int handle_breakpoints)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int dwcphy_assert_reset(struct target *target)
{
	target->state = TARGET_RESET;

	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int dwcphy_deassert_reset(struct target *target)
{
	target->state = TARGET_RUNNING;

	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static void dwcphy_set_instr(struct jtag_tap *tap, uint32_t new_instr)
{

	LOG_DEBUG("set ir %x len %d", new_instr, tap->ir_length);

	struct scan_field field;
	field.num_bits = tap->ir_length;
	uint8_t *t = calloc(DIV_ROUND_UP(field.num_bits, 8), 1);
	field.out_value = t;
	buf_set_u32(t, 0, field.num_bits, new_instr);
	field.in_value = NULL;
	jtag_add_ir_scan(tap, &field, TAP_IDLE);
	free(t);
}

static void dwcphy_memory_cmd_addr(struct jtag_tap *tap, size_t address)
{
	struct scan_field field[2];
	uint8_t instr_buf;
	uint8_t addr_buf[4] = {0};

	dwcphy_set_instr(tap, CRSEL);

	field[0].num_bits = 32;
	field[0].out_value = addr_buf;
	buf_set_u32(addr_buf, 0, 32, address);
	LOG_DEBUG("addr buf %x", (*(uint32_t *)addr_buf));
	field[0].in_value = NULL;
	field[0].check_value = NULL;
	field[0].check_mask = NULL;

	field[1].num_bits = 2;
	field[1].out_value = &instr_buf;
	buf_set_u32(&instr_buf, 0, 2, READ);
	field[1].in_value = NULL;
	field[1].check_value = NULL;
	field[1].check_mask = NULL;

	jtag_add_dr_scan(tap, 2, field, TAP_IDLE);
}

static void dwcphy_memory_cmd_data(struct jtag_tap *tap, const uint8_t* data, uint8_t rnw, uint8_t *value)
{
	struct scan_field field[2];
	uint8_t instr_buf;
	uint8_t data_buf[4] = {0};

	dwcphy_set_instr(tap, CRSEL);

	field[0].num_bits = 32;
	field[0].out_value = (data) ? data : data_buf;
	field[0].in_value = (value) ? value : NULL;
	field[0].check_value = NULL;
	field[0].check_mask = NULL;

	field[1].num_bits = 2;
	field[1].out_value = &instr_buf;
	buf_set_u32(&instr_buf, 0, 2, rnw);
	field[1].in_value = NULL;
	field[1].check_value = NULL;
	field[1].check_mask = NULL;

	jtag_add_dr_scan(tap, 2, field, TAP_IDLE);
}

static int dwcphy_read_memory(struct target *target, target_addr_t address,
							uint32_t size, uint32_t count, uint8_t *buffer)
{
	LOG_DEBUG("Reading memory at physical address 0x%" TARGET_PRIxADDR
		  "; size %" PRIu32 "; count %" PRIu32, address, size, count);

	if (count == 0 || !buffer)
		return ERROR_COMMAND_SYNTAX_ERROR;

	while (count--) {
		dwcphy_memory_cmd_addr(target->tap, address);
		jtag_add_clocks(150);
		dwcphy_memory_cmd_data(target->tap, NULL, READ, buffer);

		dwcphy_memory_cmd_addr(target->tap, address);
		jtag_add_clocks(150);
		dwcphy_memory_cmd_data(target->tap, NULL, READ, buffer);
		address += size;
		buffer += size;
	}

	return jtag_execute_queue();

}

static int dwcphy_write_memory(struct target *target, target_addr_t address,
				uint32_t size, uint32_t count,
				const uint8_t *buffer)
{
	LOG_DEBUG("Writing memory at physical address 0x%" TARGET_PRIxADDR
		  "; size %" PRIu32 "; count %" PRIu32, address, size, count);

	if (count == 0 || !buffer)
		return ERROR_COMMAND_SYNTAX_ERROR;

	while (count--) {
		dwcphy_memory_cmd_addr(target->tap, address);
		dwcphy_memory_cmd_data(target->tap, buffer, WRITE, NULL);
		address += size;
		buffer += size;
	}


	return ERROR_OK;
}


struct target_type dwcphy_target = {
	.name = "dwcphy",
	.target_create = dwcphy_target_create,
	.init_target = dwcphy_init_target,

	.poll = dwcphy_poll,
	.arch_state = dwcphy_arch_state,

	.halt = dwcphy_halt,
	.resume = dwcphy_resume,
	.step = dwcphy_step,

	.assert_reset = dwcphy_assert_reset,
	.deassert_reset = dwcphy_deassert_reset,

	.read_memory = dwcphy_read_memory,
	.write_memory = dwcphy_write_memory
};
