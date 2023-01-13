#ifndef DWCSSI_FLASH_H
#define DWCSSI_FALSH_H

#include <flash/target_io.h>

// qspi flash mio defines
#define   MIO_BASE                             0xF8803000

// s25f1256s flash defines
#define   FLASH_RD_CONFIG_REG_CMD              0x35
#define   FLASH_WR_CONFIG_REG_CMD              0x01

struct dwcssi_flash_bank {
    bool probed;
    target_addr_t ctrl_base;
    const struct flash_device *dev;
    struct target_loader loader;
};

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


#define   FLASH_STATUS_ERR(x)                  ((x >> 5) & 0x3)
#define   FLASH_STATUS_WP(x)                   ((x >> 2) & 0x7)
#define   FLASH_CONFIG_QUAD(x)                 ((x >> 1) & 0x1)


// flash model 
int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t id);
int flash_sector_check(struct flash_bank *bank, uint32_t offset, uint32_t count);
uint32_t flash_write_boundary_check(struct flash_bank *bank, uint32_t offset, uint32_t count);
int flash_check_status(uint8_t status);
uint8_t flash_check_wp(uint8_t status);
uint8_t flash_quad_mode(uint8_t config_reg);

// flash compatibility
int sp_flash_reset(struct flash_bank *bank);
int sp_flash_quad_disable(struct flash_bank* bank);
int sp_flash_quad_enable(struct flash_bank* bank);

#endif

