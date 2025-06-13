#ifndef OPENOCD_PLD_ANLOGIC_BIT_H
#define OPENOCD_PLD_ANLOGIC_BIT_H

#include "helper/types.h"

struct anlogic_bit_file {
    uint8_t *start;
    uint8_t *version;
    uint8_t *design_name;
    uint8_t *architecture;
    uint8_t *package;
    uint8_t *date;
    uint8_t *golbal_crc;
    uint8_t *transfer_crc;
    uint8_t *file_format;
    uint8_t *spi_feature; /* 修复拼写错误 */
    uint8_t *user_code;
    uint8_t *data;
    long    data_len;
};

int anlogic_read_bit_file(struct anlogic_bit_file *bit_file, const char *filename);
int anlogic_check_architecture(struct anlogic_bit_file* bit_file, const char *drv_name);
#endif /* OPENOCD_PLD_ANLOGIC_BIT_H */