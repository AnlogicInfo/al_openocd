/*
 * File: pstap.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-12-12
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-12-12
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


#define DBGACC   0b1010

#define READ     0b01
#define WRITE    0b10


struct pstap {
    struct jtag_tap *tap;
};

static int pstap_target_create(struct target *target, Jim_Interp *interp)
{
    struct pstap *pstap = calloc(1, sizeof(struct pstap));

    pstap->tap = target->tap;
    target->arch_info = pstap;
    return ERROR_OK;
}

static int pstap_init_target(struct command_context *cmd_ctx, struct target *target)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int pstap_arch_state(struct target *target)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}


static int pstap_poll(struct target *target)
{
	if ((target->state == TARGET_UNKNOWN) ||
	    (target->state == TARGET_RUNNING) ||
	    (target->state == TARGET_DEBUG_RUNNING))
		target->state = TARGET_HALTED;

	return ERROR_OK;
}


static int pstap_halt(struct target *target)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int pstap_resume(struct target *target, int current, target_addr_t address,
		int handle_breakpoints, int debug_execution)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int pstap_step(struct target *target, int current, target_addr_t address,
				int handle_breakpoints)
{
	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int pstap_assert_reset(struct target *target)
{
	target->state = TARGET_RESET;

	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static int pstap_deassert_reset(struct target *target)
{
	target->state = TARGET_RUNNING;

	LOG_DEBUG("%s", __func__);
	return ERROR_OK;
}

static void pstap_set_instr(struct jtag_tap *tap, uint32_t new_instr)
{

    if(buf_get_u32(tap->cur_instr, 0, tap->ir_length) == new_instr)
    {
        return;
    }

    struct scan_field field;
	field.num_bits = tap->ir_length;
	uint8_t *t = calloc(DIV_ROUND_UP(field.num_bits, 8), 1);
	field.out_value = t;
	buf_set_u32(t, 0, field.num_bits, new_instr);
	field.in_value = NULL;
	jtag_add_ir_scan(tap, &field, TAP_IDLE);
	free(t);
}

static void pstap_memory_cmd(struct jtag_tap *tap, size_t address, const uint8_t* data, uint8_t rnw)
{
	struct scan_field field[3];
    uint8_t instr_buf;
    uint8_t addr_buf[5] = {0};
    uint8_t data_buf[4] = {0};

	pstap_set_instr(tap, DBGACC);

    field[0].num_bits = 2;
    field[0].out_value = &instr_buf;
    buf_set_u32(&instr_buf, 0, 2, rnw);
    field[0].in_value = NULL;
    field[0].check_value = NULL;
	field[0].check_mask = NULL;

    field[1].num_bits = 33;
    field[1].out_value = addr_buf;
    buf_set_u64(addr_buf, 0, 33, address);
    field[1].in_value = NULL;
    field[1].check_value = NULL;
	field[1].check_mask = NULL;


    field[2].num_bits = 32;
    field[2].out_value = (data) ? data : data_buf;
    field[2].in_value = NULL;
    field[2].check_value = NULL;
	field[2].check_mask = NULL;

	jtag_add_dr_scan(tap, 3, field, TAP_IDLE);
}

static void pstap_result_read(struct jtag_tap *tap, size_t address, uint32_t size, uint8_t *value)
{
	struct scan_field field[3];
    uint8_t instr_buf;
    uint8_t addr_buf[5] = {0};
    uint8_t data_buf[4] = {0}; 

	pstap_set_instr(tap, DBGACC);

    field[0].num_bits = 2;
    field[0].out_value = &instr_buf;
    buf_set_u32(&instr_buf, 0, 2, READ);
    field[0].in_value = NULL;
    field[0].check_value = NULL;
	field[0].check_mask = NULL;

    field[1].num_bits = 33;
    field[1].out_value = addr_buf;
    buf_set_u64(addr_buf, 0, 33, address);
    field[1].in_value = NULL;
    field[1].check_value = NULL;
	field[1].check_mask = NULL;


    field[2].num_bits = 32;
    field[2].out_value = data_buf;
    buf_set_u32(data_buf, 0, 32, 0);
    field[2].in_value = value;
    field[2].check_value = NULL;
	field[2].check_mask = NULL;

	jtag_add_dr_scan(tap, 3, field, TAP_IDLE);

}

static int pstap_read_memory(struct target *target, target_addr_t address, 
                            uint32_t size, uint32_t count, uint8_t *buffer)
{
	LOG_DEBUG("Reading memory at physical address 0x%" TARGET_PRIxADDR
		  "; size %" PRIu32 "; count %" PRIu32, address, size, count);

    if(count == 0 || !buffer)
		return ERROR_COMMAND_SYNTAX_ERROR;

    while(count --) {
        pstap_memory_cmd(target->tap, address, NULL, READ);
        pstap_memory_cmd(target->tap, address, NULL, READ);

        pstap_result_read(target->tap, address, size, buffer);
        address += size;
        buffer += size;
    }

    return jtag_execute_queue();

}

static int pstap_write_memory(struct target *target, target_addr_t address,
				uint32_t size, uint32_t count,
				const uint8_t *buffer)
{
	LOG_DEBUG("Writing memory at physical address 0x%" TARGET_PRIxADDR
		  "; size %" PRIu32 "; count %" PRIu32, address, size, count);

	if (count == 0 || !buffer)
		return ERROR_COMMAND_SYNTAX_ERROR;

    while(count --) {
        pstap_memory_cmd(target->tap, address, buffer, WRITE);
        address += size;
        buffer += size;
    }


    return ERROR_OK;
}


struct target_type pstap_target = 
{
    .name = "pstap",
    .target_create = pstap_target_create,
    .init_target = pstap_init_target,

    .poll = pstap_poll,
    .arch_state = pstap_arch_state,

    .halt = pstap_halt,
    .resume = pstap_resume,
    .step = pstap_step,

    .assert_reset = pstap_assert_reset,
    .deassert_reset = pstap_deassert_reset,

    .read_memory = pstap_read_memory,
    .write_memory = pstap_write_memory
};
