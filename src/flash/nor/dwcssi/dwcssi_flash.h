#ifndef DWCSSI_FLASH_H
#define DWCSSI_FLASH_H

struct dwcssi_flash_bank {
    bool probed;
    target_addr_t ctrl_base;
    const struct flash_device *dev;
    struct flash_loader loader;
};

// flash model 
int flash_bank_init(struct flash_bank *bank,  struct dwcssi_flash_bank *dwcssi_info, uint32_t id);
int flash_sector_check(struct flash_bank *bank, uint32_t offset, uint32_t count);
uint32_t flash_write_boundary_check(struct flash_bank *bank, uint32_t offset, uint32_t count);

#endif
