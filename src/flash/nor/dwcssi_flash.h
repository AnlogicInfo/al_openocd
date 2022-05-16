#ifndef DWCSSI_FLASH_H
#define DWCSSI_FALSH_H
// s25f1256s flash defines
#define   FLASH_RD_CONFIG_REG_CMD              0x35
#define   FLASH_WR_CONFIG_REG_CMD              0x01
typedef union sp_flash_sr1_t
{
    uint8_t reg_val;
    struct
    {
        uint8_t WIP   : 1;
        uint8_t WEL   : 1;
        uint8_t BP0   : 1;
        uint8_t BP1   : 1;
        uint8_t BP2   : 1;
        uint8_t E_ERR : 1;
        uint8_t P_ERR : 1;
        uint8_t SRWD  : 1;
    } reg_fields;
} sp_flash_sr1_t;

typedef union sp_flash_cr1_t
{
    uint8_t reg_val;
    struct {
        uint8_t FREEZE : 1;
        uint8_t QUAD   : 1;
        uint8_t TBPARM : 1;
        uint8_t BPNV   : 1;
        uint8_t RFU    : 1;
        uint8_t TBPROT : 1;
        uint8_t LC0    : 1;
        uint8_t LC1    : 1;        
    } reg_fields;

} sp_flash_cr1_t;

// typedef struct flash_input_t
// {
//     uint32_t bytes_to_send;
//     uint8_t  buf[512];

// } flash_input_t;

// typedef struct flash_output_t
// {
//     uint32_t bytes_to_read;
//     uint8_t  buf[512];
// } flash_output_t;

// typedef struct flash_intf_t
// {
//     uint8_t        flash_status;
//     flash_input_t  input;
//     flash_output_t output;
// } flash_intf_t;



#define   FLASH_STATUS_ERR(x)                  ((x >> 5) & 0x3)
#define   FLASH_STATUS_WP(x)                   ((x >> 2) & 0x7)
#define   FLASH_CONFIG_QUAD(x)                 ((x >> 1) & 0x1)


// flash support
int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t id);
int flash_sector_check(struct flash_bank *bank, uint32_t offset, uint32_t count);
int flash_check_status(uint8_t status);
uint8_t flash_check_wp(uint8_t status);
uint8_t flash_quad_mode(uint8_t config_reg);
#endif

