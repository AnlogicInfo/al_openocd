#ifndef OPENOCD_FLASH_EMMC_TARGET_IO_H
#define OPENOCD_FLASH_EMMC_TARGET_IO_H
enum target_emmc_op {
    TARGET_EMMC_NONE,
    TARGET_EMMC_READ,
    TARGET_EMMC_WRITE,
};

struct target_emmc_loader {
    struct target *target;
    struct working_area *copy_area;
    unsigned chunk_size;
    target_addr_t ctrl_base;
    enum target_emmc_op op;
};

int target_emmc_write(struct target_emmc_loader *loader, uint8_t *data, target_addr_t offset, int size);
// int target_emmc_read(struct target_emmc_loader *loader, uint8_t *data, uint32_t size);


#endif

