/*
 * File: jtag2apb.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-12-12
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-12-12
 */

/*
 * JTAG2APB GPIO Bitfield Description:
 * [1:0]   GPIO_INSTR
 *         Instruction: Selects the instruction to be performed on the configuration port.
 *         00 = Reserved. No action on the configuration port
 *         01 = Load configuration register address and execute a register read operation
 *         10 = Load configuration register address and data and execute a register write operation
 *         11 = Execute read a register read/write operation done status
 * [33:2]  GPIO_ADDR
 *         Address: Address for the register to be read or written.
 * [34]    GPIO_JTAG2APB_enable
 *         1 : JTAG REG function, 0 : JTAG2APB function
 * [66:35] GPIO_DATA
 *         INSTR=10, Write Data: Data to be written to a register
 *         INSTR=01, Read Data: Data read from a register.
 *         INSTR=10, Read operation done status: Bit0 is execute read a register read/write operation done status, 1 is done, 0 is busy
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


#define DBGACC   0xE0

#define READ     0b01
#define WRITE    0b10


struct jtag2apb {
    struct jtag_tap *tap;
};

static int jtag2apb_target_create(struct target *target, Jim_Interp *interp)
{
    struct jtag2apb *jtag2apb = calloc(1, sizeof(struct jtag2apb));

    jtag2apb->tap = target->tap;
    target->arch_info = jtag2apb;
    jtag2apb->tap->bypass = 0;
    return ERROR_OK;
}

static int jtag2apb_init_target(struct command_context *cmd_ctx, struct target *target)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int jtag2apb_arch_state(struct target *target)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}


static int jtag2apb_poll(struct target *target)
{
    if ((target->state == TARGET_UNKNOWN) ||
        (target->state == TARGET_RUNNING) ||
        (target->state == TARGET_DEBUG_RUNNING))
        target->state = TARGET_HALTED;

    return ERROR_OK;
}


static int jtag2apb_halt(struct target *target)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int jtag2apb_resume(struct target *target, int current, target_addr_t address,
        int handle_breakpoints, int debug_execution)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int jtag2apb_step(struct target *target, int current, target_addr_t address,
                int handle_breakpoints)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int jtag2apb_assert_reset(struct target *target)
{
    target->state = TARGET_RESET;

    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static int jtag2apb_deassert_reset(struct target *target)
{
    target->state = TARGET_RUNNING;

    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

static void jtag2apb_set_instr(struct jtag_tap *tap, uint32_t new_instr)
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
    jtag_execute_queue();
}

static void jtag2apb_memory_cmd(struct jtag_tap *tap, size_t address, uint8_t jtag2apb_en, const uint8_t* data, uint8_t rnw)
{
    struct scan_field field[4];
    uint8_t instr_buf = 0;
    uint8_t apb_en_buf = 0;
    uint8_t addr_buf[4] = {0};
    uint8_t data_buf[4] = {0};
    
    // LOG_INFO("jtag2apb_memory_cmd: address=0x%" TARGET_PRIxADDR " jtag2apb_en=%d rnw=%d", address, jtag2apb_en, rnw);

    jtag2apb_set_instr(tap, DBGACC);
    
    field[0].num_bits = 2;
    field[0].out_value = &instr_buf;
    buf_set_u32(&instr_buf, 0, 2, rnw);
    field[0].in_value = NULL;
    field[0].check_value = NULL;
    field[0].check_mask = NULL;

    field[1].num_bits = 32;
    field[1].out_value = addr_buf;
    buf_set_u64(addr_buf, 0, 32, address);
    field[1].in_value = NULL;
    field[1].check_value = NULL;
    field[1].check_mask = NULL;

    field[2].num_bits = 1;
    field[2].out_value = &apb_en_buf;
    buf_set_u32(&apb_en_buf, 0, 1, jtag2apb_en);
    field[2].in_value = NULL;
    field[2].check_value = NULL;
    field[2].check_mask = NULL;

    field[3].num_bits = 32;
    field[3].out_value = (data) ? data : data_buf;
    field[3].in_value = NULL;
    field[3].check_value = NULL;
    field[3].check_mask = NULL;

    jtag_add_dr_scan(tap, 4, field, TAP_IDLE);
}

static void jtag2apb_result_read(struct jtag_tap *tap, size_t address, uint8_t jtag2apb_en, uint8_t *value)
{
    struct scan_field field[4];
    uint8_t instr_buf = 0;
    uint8_t apb_en_buf = 0;
    uint8_t addr_buf[4] = {0};
    uint8_t data_buf[4] = {0}; 

    jtag2apb_set_instr(tap, DBGACC);
       
    field[0].num_bits = 2;
    field[0].out_value = &instr_buf;
    buf_set_u32(&instr_buf, 0, 2, READ);
    field[0].in_value = NULL;
    field[0].check_value = NULL;
    field[0].check_mask = NULL;

    field[1].num_bits = 32;
    field[1].out_value = addr_buf;
    buf_set_u64(addr_buf, 0, 32, address);
    field[1].in_value = NULL;
    field[1].check_value = NULL;
    field[1].check_mask = NULL;

    field[2].num_bits = 1;
    field[2].out_value = &apb_en_buf;
    buf_set_u32(&apb_en_buf, 0, 1, jtag2apb_en);
    field[2].in_value = NULL;
    field[2].check_value = NULL;
    field[2].check_mask = NULL;

    field[3].num_bits = 32;
    field[3].out_value = data_buf;
    buf_set_u32(data_buf, 0, 32, 0);
    field[3].in_value = value;
    field[3].check_value = NULL;
    field[3].check_mask = NULL;

    jtag_add_dr_scan(tap, 4, field, TAP_IDLE);

}

static int jtag2apb_read_apb_memory(struct target *target, target_addr_t address, 
                            uint32_t size, uint32_t count, uint8_t *buffer)
{
    // LOG_INFO("Reading memory at apb address 0x%" TARGET_PRIxADDR
    //       "; size %" PRIu32 "; count %" PRIu32, address, size, count);

    if(count == 0 || !buffer)
        return ERROR_COMMAND_SYNTAX_ERROR;

    while(count --) {
        jtag2apb_memory_cmd(target->tap, address, 0, NULL, READ);
        jtag2apb_memory_cmd(target->tap, address, 0, NULL, READ);

        jtag2apb_result_read(target->tap, address, 0, buffer);
        address += size;
        buffer += size;
    }

    return jtag_execute_queue();

}

static int jtag2apb_write_apb_memory(struct target *target, target_addr_t address,
                uint32_t size, uint32_t count,
                const uint8_t *buffer)
{
    LOG_DEBUG("Writing memory at apb address 0x%" TARGET_PRIxADDR
          "; size %" PRIu32 "; count %" PRIu32, address, size, count);

    if (count == 0 || !buffer)
        return ERROR_COMMAND_SYNTAX_ERROR;

    while(count --) {
        jtag2apb_memory_cmd(target->tap, address, 0,buffer, WRITE);
        address += size;
        buffer += size;
    }


    return ERROR_OK;
}


static int jtag2apb_read_reg_memory(struct target *target, target_addr_t address, 
                            uint32_t size, uint32_t count, uint8_t *buffer)
{
    // LOG_INFO("Reading memory at reg address 0x%" TARGET_PRIxADDR
    //       "; size %" PRIu32 "; count %" PRIu32, address, size, count);

    if(count == 0 || !buffer)
        return ERROR_COMMAND_SYNTAX_ERROR;

    while(count --) {
        jtag2apb_memory_cmd(target->tap, address, 1, NULL, READ);
        jtag2apb_memory_cmd(target->tap, address, 1, NULL, READ);

        jtag2apb_result_read(target->tap, address,1, buffer);
        address += size;
        buffer += size;
    }

    return jtag_execute_queue();

}

static int jtag2apb_write_reg_memory(struct target *target, target_addr_t address,
                uint32_t size, uint32_t count,
                const uint8_t *buffer)
{
    LOG_DEBUG("Writing memory at reg address 0x%" TARGET_PRIxADDR
          "; size %" PRIu32 "; count %" PRIu32, address, size, count);

    if (count == 0 || !buffer)
        return ERROR_COMMAND_SYNTAX_ERROR;

    while(count --) {
        jtag2apb_memory_cmd(target->tap, address, 1,buffer, WRITE);
        address += size;
        buffer += size;
    }


    return ERROR_OK;
}

static int jtag2pab_mmu(struct target *target, int *enabled)
{
    LOG_DEBUG("%s", __func__);
    return ERROR_OK;
}

struct target_type jtag2apb_target = 
{
    .name = "jtag2apb",
    .target_create = jtag2apb_target_create,
    .init_target = jtag2apb_init_target,

    .poll = jtag2apb_poll,
    .arch_state = jtag2apb_arch_state,

    .halt = jtag2apb_halt,
    .resume = jtag2apb_resume,
    .step = jtag2apb_step,

    .assert_reset = jtag2apb_assert_reset,
    .deassert_reset = jtag2apb_deassert_reset,

    .read_memory = jtag2apb_read_apb_memory,
    .write_memory = jtag2apb_write_apb_memory,

    .read_phys_memory = jtag2apb_read_reg_memory,
    .write_phys_memory = jtag2apb_write_reg_memory,
    .mmu = jtag2pab_mmu,
};
