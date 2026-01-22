#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "dr1.h"
#include "anlogic_bit.h"
#include "pld.h"
#include "jtag/interface.h"
#include "jtag/adapter.h"
#include <helper/log.h> // 确保包含 LOG_DEBUG 宏的头文件

extern struct adapter_driver *adapter_driver;
static int dr1_set_ir(struct jtag_tap *tap, uint32_t new_instr)
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


static int dr1_send_32(struct pld_device *pld_device,
    int num_words, uint32_t *words, uint32_t *in_value)
{
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    struct scan_field scan_field;

    scan_field.in_value = (uint8_t *)in_value;

    scan_field.num_bits = num_words * 32;
    scan_field.out_value = (uint8_t *)words;

    jtag_add_dr_scan(dr1_info->tap, 1, &scan_field, TAP_IDLE);


    return ERROR_OK;
}

static int dr1_send_addr(struct pld_device *pld_device,
    uint32_t *addr, uint32_t *in_value)
{
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    struct scan_field scan_field;

    scan_field.in_value = (uint8_t *)in_value;

    scan_field.num_bits = 20;
    scan_field.out_value = (uint8_t *)addr;

    jtag_add_dr_scan(dr1_info->tap, 1, &scan_field, TAP_IDLE);
    return ERROR_OK;
}

static int dr1_read_status(struct pld_device *pld_device, uint32_t *status)
{
    uint32_t outvalue = 0;
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    dr1_set_ir(dr1_info->tap, 0x46);
    dr1_send_32(pld_device, 1, &outvalue, status);
    jtag_execute_queue();
    LOG_INFO("DR1 status register: 0x%08x", *status);
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

    dr1_set_ir(dr1_info->tap, 0xFF);
    dr1_set_ir(dr1_info->tap, 0xFF);
    dr1_set_ir(dr1_info->tap, 0x06);
    jtag_add_runtest(15, TAP_IDLE);
    dr1_send_32(pld_device, 1, &outvalue, idcode);
    
    return ERROR_OK;

}

static int dr1_load(struct pld_device *pld_device, const char *filename)
{
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    struct anlogic_bit_file bit_file;
    int download_khz = 5000;
    int actual_khz;
    uint32_t idcode=0;
    status0_t status0;
    int retval;
    long i;
    actual_khz = adapter_get_speed_khz();
    adapter_config_khz(download_khz);
    // 初始化状态寄存器
    status0.reg_val = 0;

    retval = anlogic_read_bit_file(&bit_file, filename);
    if (retval != ERROR_OK) {
        LOG_ERROR("Failed to read bit file: %s", filename);
        return retval;
    }

    retval = anlogic_check_architecture(&bit_file, pld_device->driver->name);
    if (retval != ERROR_OK)
        return retval;

    dr1_read_id(pld_device, &idcode);
    dr1_set_ir(dr1_info->tap, 0x1);
    dr1_set_ir(dr1_info->tap, 0xFF);
    dr1_set_ir(dr1_info->tap, 0x39);
    jtag_add_runtest(80000, TAP_IDLE);
    jtag_execute_queue();
    
    dr1_set_ir(dr1_info->tap, 0x30);
    jtag_add_runtest(1000, TAP_IDLE);
    
    dr1_set_ir(dr1_info->tap, 0x3b);
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
    jtag_add_dr_scan(dr1_info->tap, 1, &field, TAP_IDLE);

    jtag_add_runtest(1000, TAP_IDLE);
    jtag_execute_queue();
    dr1_set_ir(dr1_info->tap, 0x32);
    jtag_add_clocks(1500);
    
    dr1_set_ir(dr1_info->tap, 0xFF);
    jtag_add_clocks(1000);
    
    dr1_set_ir(dr1_info->tap, 0x31);
    jtag_add_clocks(1000);
    
    dr1_set_ir(dr1_info->tap, 0x32);
    jtag_add_clocks(1000);
    
    dr1_set_ir(dr1_info->tap, 0xFF);
    jtag_add_clocks(1000);
    jtag_execute_queue();

    dr1_read_status(pld_device, &status0.reg_val);

    if (status0.reg_fields.jtag_prgm_done | status0.reg_fields.njtag_prgm_done)
    {
        LOG_INFO("Programming successful, JTAG programming done.");
    }
    else
    {
        LOG_ERROR("Programming failed, JTAG programming not done.");
        return ERROR_FAIL;
    }

    adapter_config_khz(actual_khz);
    return ERROR_OK;
}

static void dr1_send_instr(struct pld_device *pld_device, uint8_t command, uint32_t value)
{
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    uint32_t outvalue = 0, invalue = 0;

    outvalue = value;

    dr1_set_ir(dr1_info->tap, command);
    jtag_add_runtest(5, TAP_IDLE);
    dr1_send_32(pld_device, 1, &outvalue, &invalue);
    jtag_add_runtest(5, TAP_IDLE);
}



static int dr1_load_sector(struct pld_device *pld_device, const char *filename)
{
    struct dr1_fpga_device *dr1_info = pld_device->driver_priv;
    uint32_t idcode = 0, addr = 0x00008C;

    dr1_read_id(pld_device, &idcode);
    dr1_set_ir(dr1_info->tap, 0x1);
    dr1_set_ir(dr1_info->tap, 0xFF);
    dr1_set_ir(dr1_info->tap, 0x39);
    jtag_add_runtest(80000, TAP_IDLE);
    jtag_execute_queue();

    dr1_set_ir(dr1_info->tap, 0x30);
    jtag_add_runtest(1000, TAP_IDLE);
    jtag_execute_queue();

    dr1_send_instr(pld_device, 0x40, 0x820FC000);
    dr1_send_instr(pld_device, 0x41, 0x401001F0);
    dr1_send_instr(pld_device, 0x42, 0x00000000);
    dr1_send_instr(pld_device, 0x6f, 0x00000000);
    dr1_send_instr(pld_device, 0x69, 0x00000690);
    dr1_send_instr(pld_device, 0x6F, 0x0003003F);
    dr1_send_instr(pld_device, 0x6A, 0x14000000);
    dr1_send_instr(pld_device, 0x6F, 0x0000001F);
    dr1_send_instr(pld_device, 0x6B, 0x00000240);
    dr1_send_instr(pld_device, 0x49, 0x00000000);
    dr1_send_instr(pld_device, 0x4A, 0x00000000);
    dr1_send_instr(pld_device, 0x4B, 0x00000000);
    dr1_send_instr(pld_device, 0x65, 0x00344DD0);
    dr1_send_instr(pld_device, 0x66, 0x00000000);

    // dr1_send_instr(pld_device, 0x21, 0x0000008C);
    dr1_set_ir(dr1_info->tap, 0x21);
    dr1_send_addr(pld_device, &addr, NULL);
    jtag_execute_queue();

    dr1_set_ir(dr1_info->tap, 0x22);
    jtag_add_runtest(15, TAP_IDLE);
    jtag_execute_queue();
    // 读取二进制文件作为 field.out_value
    FILE *fin = fopen(filename, "rb");
    if (!fin) {
        LOG_ERROR("Failed to open file: %s", filename);
        return ERROR_FAIL;
    }
    fseek(fin, 0, SEEK_END);
    long file_len = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    if (file_len <= 0) {
        fclose(fin);
        LOG_ERROR("File is empty: %s", filename);
        return ERROR_FAIL;
    }
    uint8_t *buf = malloc(file_len);
    if (!buf) {
        fclose(fin);
        LOG_ERROR("Failed to allocate buffer for file: %s", filename);
        return ERROR_FAIL;
    }
    if (fread(buf, 1, file_len, fin) != (size_t)file_len) {
        fclose(fin);
        free(buf);
        LOG_ERROR("Failed to read file: %s", filename);
        return ERROR_FAIL;
    }
    fclose(fin);

    struct scan_field field;
    field.in_value = NULL;
    field.num_bits = file_len * 8;
    field.out_value = buf;

    jtag_add_dr_scan(dr1_info->tap, 1, &field, TAP_IDLE);
    jtag_add_runtest(100, TAP_IDLE);
    jtag_execute_queue();

    dr1_set_ir(dr1_info->tap, 0x21);
    dr1_send_addr(pld_device, &addr, NULL);    
    jtag_add_runtest(100, TAP_IDLE);
    dr1_set_ir(dr1_info->tap, 0x23);
    jtag_add_runtest(30, TAP_IDLE);
    jtag_execute_queue();



    uint8_t *verify_out_buf, *verify_in_buf;
    size_t verify_len = 324; // 2592 bits / 8 bits per byte 
    verify_in_buf = malloc(verify_len);
    verify_out_buf = malloc(verify_len);
    memset(verify_in_buf, 0, verify_len);
    memset(verify_out_buf, 0, verify_len);

    field.num_bits = verify_len*8;
    field.out_value = verify_out_buf;
    field.in_value = verify_in_buf;

    jtag_add_dr_scan(dr1_info->tap, 1, &field, TAP_IDLE);
    jtag_add_runtest(15, TAP_IDLE);
    jtag_execute_queue();

    dr1_set_ir(dr1_info->tap, 0x32);
    dr1_set_ir(dr1_info->tap, 0x31);
    dr1_set_ir(dr1_info->tap, 0x32);
    dr1_set_ir(dr1_info->tap, 0x1F);
    jtag_add_runtest(50, TAP_IDLE);
    jtag_execute_queue();

    free(buf);
    free(verify_in_buf);
    free(verify_out_buf);

    return ERROR_OK;
}


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
    .name = "dr1_90",
    .commands = dr1_command_handler,
    .pld_device_command = &dr1_fpga_device_command,
    .load = &dr1_load,
    .load_sector = &dr1_load_sector,
}; // 添加缺失的分号