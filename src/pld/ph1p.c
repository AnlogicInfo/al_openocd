#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "ph1p.h"
#include "anlogic_bit.h"
#include "pld.h"
#include <helper/log.h> // 确保包含 LOG_DEBUG 宏的头文件

static int ph1p_set_ir(struct jtag_tap *tap, uint32_t new_instr)
{
    if (!tap)
        return ERROR_FAIL;

    struct scan_field field;
    field.num_bits = tap->ir_length;
    void *t = calloc(DIV_ROUND_UP(field.num_bits, 8), 1);
    field.out_value = t;
    buf_set_u32(t, 0, field.num_bits, new_instr);
    field.in_value = NULL;
    jtag_add_ir_scan(tap, &field, TAP_IDLE);
    free(t);
    return ERROR_OK;
}

static int ph1p_set_dr(struct jtag_tap *tap, uint32_t num_bits, uint8_t *out_val, uint8_t *in_val)
{

    if (!tap)
        return ERROR_FAIL;
    struct scan_field field;
    field.num_bits = num_bits;
    field.out_value = out_val;
    field.in_value = in_val;

    jtag_add_dr_scan(tap, 1, &field, TAP_IDLE);
    return ERROR_OK;
}

// 实现 Disable Dual boot loop
static int ph1p_disable_dual_boot(struct jtag_tap *tap)
{
    // SIR 8 TDI (80)
    ph1p_set_ir(tap, 0x80);

    // SDR 64 TDI (00800000000000a5)
    uint8_t dr_val[8] = {0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00}; // LSB first
    uint8_t in_val[8] = {0};
    ph1p_set_dr(tap, 64, dr_val, in_val);

    // RUNTEST 200 TCK
    jtag_add_runtest(200, TAP_IDLE);

    return ERROR_OK;
}

static int ph1p_read_id(struct pld_device *pld_device, uint32_t *idcode)
{
    struct ph1p_fpga_device *ph1p_info = pld_device->driver_priv;
    uint32_t outvalue = 0;
    uint8_t invalue[4] = {0};

    ph1p_set_ir(ph1p_info->tap, 0xFF);
    ph1p_set_ir(ph1p_info->tap, 0xFF);
    ph1p_set_ir(ph1p_info->tap, 0x06);
    jtag_add_runtest(15, TAP_IDLE);
    ph1p_set_dr(ph1p_info->tap, 32, (uint8_t *)&outvalue, invalue);

    // 将 invalue 字节数组转换为 uint32_t
    *idcode = invalue[0] | (invalue[1] << 8) | (invalue[2] << 16) | (invalue[3] << 24);

    LOG_DEBUG("read idcode: 0x%08x", *idcode);

    return ERROR_OK;

}

static int ph1p_load(struct pld_device *pld_device, const char *filename)
{
    struct ph1p_fpga_device *ph1p_info = pld_device->driver_priv;
    struct anlogic_bit_file bit_file;
    uint32_t idcode=0;
    uint8_t dr_val[8] = {0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // LSB first
    uint8_t in_val[8] = {0};
    int retval;
    long i;

    retval = anlogic_read_bit_file(&bit_file, filename);
    if (retval != ERROR_OK) {
        LOG_ERROR("Failed to read bit file: %s", filename);
        return retval;
    }
    ph1p_disable_dual_boot(ph1p_info->tap);
    ph1p_read_id(pld_device, &idcode);
    ph1p_set_ir(ph1p_info->tap, 0x1);
    ph1p_set_ir(ph1p_info->tap, 0xFF);
    ph1p_set_ir(ph1p_info->tap, 0x39);
    jtag_add_runtest(120000, TAP_IDLE);
    jtag_execute_queue();
    
    ph1p_set_ir(ph1p_info->tap, 0x30);
    jtag_add_runtest(1000, TAP_IDLE);
    
    ph1p_set_ir(ph1p_info->tap, 0x3b);
    jtag_add_runtest(1000, TAP_IDLE);
    for (i = 0; i < bit_file.data_len; i++)
    {
        bit_file.data[i] = flip_u32(bit_file.data[i], 8); 
    }

    // 一次性写入全部数据
    struct scan_field field;
    field.in_value = NULL;
    field.num_bits = bit_file.data_len * 8;
    field.out_value = (uint8_t*)bit_file.data;
    // LOG_INFO("Loading bit file: %s, size: %ld bytes", filename, bit_file.data_len);
    jtag_add_dr_scan(ph1p_info->tap, 1, &field, TAP_IDLE);

    jtag_add_runtest(1000, TAP_IDLE);
    jtag_execute_queue();
    ph1p_set_ir(ph1p_info->tap, 0x32);
    jtag_add_runtest(1000, TAP_IDLE);
    
    ph1p_set_ir(ph1p_info->tap, 0xFF);
    jtag_add_runtest(1000, TAP_IDLE);
    
    ph1p_set_ir(ph1p_info->tap, 0x31);
    jtag_add_runtest(1000, TAP_IDLE);
    
    ph1p_set_ir(ph1p_info->tap, 0x32);
    jtag_add_runtest(15, TAP_IDLE);
    
    ph1p_set_ir(ph1p_info->tap, 0xFF);
    jtag_add_runtest(15, TAP_IDLE);

    // SDR 64 TDI (00000000000000a5)
    ph1p_set_ir(ph1p_info->tap, 0x80);
    ph1p_set_dr(ph1p_info->tap, 64, dr_val, in_val);
    jtag_add_runtest(200, TAP_IDLE);
    jtag_execute_queue();

    return ERROR_OK;
}

PLD_DEVICE_COMMAND_HANDLER(ph1p_fpga_device_command)
{
    struct jtag_tap *tap;
    struct ph1p_fpga_device *ph1p_info;

    if (CMD_ARGC < 2)
        return ERROR_COMMAND_SYNTAX_ERROR;

    tap = jtag_tap_by_string(CMD_ARGV[1]);
    if (!tap) {
        command_print(CMD, "Tap: %s does not exist", CMD_ARGV[1]);
        return ERROR_OK;
    }
    ph1p_info = malloc(sizeof(struct ph1p_fpga_device));
    ph1p_info->tap = tap;

    pld->driver_priv = ph1p_info;

    return ERROR_OK;

}

static const struct command_registration ph1p_command_handler[] = {
    {
        .name = "ph1p",
        .mode = COMMAND_ANY,
        .help = "ph1p specific commands",
        .usage = "",
        .chain = NULL, // 修复拼写错误，移除未定义的 ph1p_exec_command_handlers
    },

    COMMAND_REGISTRATION_DONE
};


struct pld_driver ph1p_fpga = {
    .name = "ph1p",
    .commands = ph1p_command_handler,
    .pld_device_command = &ph1p_fpga_device_command,
    .load = &ph1p_load,
}; // 添加缺失的分号