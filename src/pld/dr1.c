#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "dr1.h"
#include "anlogic_bit.h"
#include "pld.h"
#include <helper/log.h> // 确保包含 LOG_DEBUG 宏的头文件

static int dr1_set_instr(struct jtag_tap *tap, uint32_t new_instr)
{
    if (!tap)
        return ERROR_FAIL;
    
    
    if (buf_get_u32(tap->cur_instr, 0, tap->ir_length) != new_instr) {
        struct scan_field field;

        field.num_bits = tap->ir_length;
        void *t = calloc(DIV_ROUND_UP(field.num_bits, 8), 1);
        field.out_value = t;
        buf_set_u32(t, 0, field.num_bits, new_instr);
        field.in_value = NULL;

        jtag_add_ir_scan(tap, &field, TAP_IDLE);

        free(t);
    }
    return ERROR_OK;
}


static int dr1_send_32(struct pld_device *pld_device,
    int num_words, uint32_t *words, uint32_t *in_value)
{
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    struct scan_field scan_field;
    uint8_t *values;
    int i;

    values = malloc(num_words * 4);
    scan_field.in_value = (uint8_t *)in_value; // 修复类型不匹配

    scan_field.num_bits = num_words * 32;
    scan_field.out_value = values;

    for (i = 0; i < num_words; i++)
        buf_set_u32(values + 4 * i, 0, 32, flip_u32(*words++, 32));

    jtag_add_dr_scan(dr1_info->tap, 1, &scan_field, TAP_IDLE);

    free(values);

    return ERROR_OK;
}

static inline void dr1_flip32(jtag_callback_data_t arg)
{
    uint32_t *words = (uint32_t *)arg;
    *words = flip_u32(*words, 32);
}

static int dr1_read_id(struct pld_device *pld_device, uint32_t *idcode)
{
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    uint32_t outvalue = 0;
    jtag_add_tlr();

    dr1_set_instr(dr1_info->tap, 0xFF);
    dr1_set_instr(dr1_info->tap, 0xFF);
    dr1_set_instr(dr1_info->tap, 0x06);
    dr1_send_32(pld_device, 1, &outvalue, idcode);

    jtag_execute_queue();
    LOG_INFO("idcode: 0x%8.8" PRIx32 "", *idcode);
    return ERROR_OK;

}


static int dr1_load(struct pld_device *pld_device, const char *filename)
{
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    struct anlogic_bit_file bit_file;
    uint32_t idcode=0;
    int retval;
    long i;

    retval = anlogic_read_bit_file(&bit_file, filename); 
    if (retval != ERROR_OK) {
        LOG_ERROR("Failed to read bit file: %s", filename);
        return retval;
    }

    dr1_read_id(pld_device, &idcode);
    if (1) 
    {
        jtag_add_sleep(10);

        dr1_set_instr(dr1_info->tap, 0x1);
        dr1_set_instr(dr1_info->tap, 0xFF);
        dr1_set_instr(dr1_info->tap, 0x39);
        jtag_execute_queue();
        jtag_add_sleep(100);

        dr1_set_instr(dr1_info->tap, 0x30);
        jtag_execute_queue();
        jtag_add_sleep(100);

        dr1_set_instr(dr1_info->tap, 0x3b);
        jtag_execute_queue();
        jtag_add_sleep(100);

        for (i = 0; i < bit_file.data_len; i++) 
            bit_file.data[i] = flip_u32(bit_file.data[i], 8);


        // 分批次进行dr scan
        // const size_t chunk_bytes = 1024; // 16KB
        size_t total_bytes = (size_t)bit_file.data_len;
        // size_t offset = 0;
        // tap_state_t end_state = TAP_DRSHIFT;
        LOG_INFO("Total bytes to scan: %zu", total_bytes);
        jtag_add_statemove(TAP_DRSHIFT);
        jtag_add_tdi_seq(3, 0, NULL, TAP_DRSHIFT); // 发送 3 个 BYPASS 占位
        jtag_add_tdi_seq(total_bytes*8, bit_file.data, NULL, TAP_IDLE); // 发送数据
        // while (offset < total_bytes) {
        //     struct scan_field field;
        //     size_t this_chunk = 0;
            
        //     if (total_bytes - offset > chunk_bytes) {
        //         this_chunk = chunk_bytes;
        //         end_state = TAP_DRSHIFT; // 保持在 DRSHIFT 状态
        //     } else {
        //         this_chunk = total_bytes - offset;
        //         end_state = TAP_IDLE; // 最后一块数据后进入 IDLE 状态
        //     }
        //     field.num_bits = (int)(this_chunk * 8);
        //     field.out_value = bit_file.data + offset;
            
        //     jtag_add_tdi_seq(field.num_bits, field.out_value, NULL, end_state);            
        //     offset += this_chunk;
        // }

        jtag_execute_queue();
        
        LOG_INFO("JTAG queue executed, now setting instructions");

        dr1_set_instr(dr1_info->tap, 0x32);
        dr1_set_instr(dr1_info->tap, 0xFF);
        jtag_execute_queue();
        jtag_add_sleep(1000);

        dr1_set_instr(dr1_info->tap, 0x31);
        jtag_execute_queue();
        jtag_add_sleep(1000);

        dr1_set_instr(dr1_info->tap, 0x32);
        jtag_execute_queue();
        jtag_add_sleep(1000);
        dr1_set_instr(dr1_info->tap, 0xFF);
        jtag_execute_queue();
        jtag_add_sleep(1000);

    }

    return ERROR_OK;
}

// 添加 jtag_set_instr 的声明
extern int jtag_set_instr(struct jtag_tap *tap, uint32_t new_instr);

PLD_DEVICE_COMMAND_HANDLER(dr1_fpga_device_command)
{
    struct jtag_tap *tap;
    struct dr1_fpga_device *dr1_info;

    if (CMD_ARGC < 2)
        return ERROR_COMMAND_SYNTAX_ERROR;
    
    tap = jtag_tap_by_string(CMD_ARGV[1]);
    if (!tap) {
        command_print(CMD, "Tap: %s does not exist", CMD_ARGV[1]);
        return ERROR_OK;
    }
    dr1_info = malloc(sizeof(struct dr1_fpga_device));
    dr1_info->tap = tap;
    
    pld->driver_priv = dr1_info;

    return ERROR_OK;

}

static const struct command_registration dr1_command_handler[] = {
    {
        .name = "dr1",
        .mode = COMMAND_ANY,
        .help = "DR1 specific commands",
        .usage = "",
        .chain = NULL, // 修复拼写错误，移除未定义的 dr1_exec_command_handlers
    },

    COMMAND_REGISTRATION_DONE
};


struct pld_driver dr1_fpga = {
    .name = "dr1",
    .commands = dr1_command_handler,
    .pld_device_command = &dr1_fpga_device_command,
    .load = &dr1_load,
}; // 添加缺失的分号