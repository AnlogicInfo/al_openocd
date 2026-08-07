#ifndef DWCSSI_FLASH_H
#define DWCSSI_FLASH_H

struct dwcssi_flash_params {
    uint32_t flash_addr;           // Offset: 0x00
    uint8_t  _pad0[60];            // Padding to 0x40
    uint32_t image_size;           // Offset: 0x40
    uint8_t  _pad1[60];            // Padding to 0x80
    uint32_t bank_id;              // Offset: 0x80
    uint8_t  _pad2[60];            // Padding to 0xC0
    uint32_t clk_param;             // Offset: 0xC0
    uint8_t  flash_name[20];
    uint32_t flash_id;
    uint32_t sector_size;
    uint32_t sector_num;
    uint32_t page_size;
    uint8_t  _pad3[24];            // Padding to 0x100
};

struct dwcssi_flash_bank {
    bool probed;
    uint8_t sample_delay;
    target_addr_t ctrl_base;
    uint8_t       addr_len;
    const struct flash_device *dev;
    struct flash_loader loader;
    struct dwcssi_flash_params params;
    char *elf_dir;
};

// #define   FLASH_STATUS_ERR(x)                  ((x >> 5) & 0x3)

// flash model
int flash_id_parse(struct dwcssi_flash_bank *dwcssi_info,  uint32_t *id);
int flash_sector_init(struct flash_bank *bank, struct dwcssi_flash_bank *dwcssi_info);
int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t *id);
int flash_sector_check(struct flash_bank *bank, uint32_t offset, uint32_t count);
uint32_t flash_write_boundary_check(struct flash_bank *bank, uint32_t offset, uint32_t count);
int flash_addr_mode_check(uint32_t offset, uint32_t count, uint8_t addr_size);
// int flash_status_err(uint8_t status);

#endif
