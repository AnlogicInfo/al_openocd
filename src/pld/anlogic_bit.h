#ifndef OPENOCD_PLD_ANLOGIC_BIT_H
#define OPENOCD_PLD_ANLOGIC_BIT_H

#include "helper/types.h"
typedef union status0 {
    uint32_t reg_val;
    struct {
        uint32_t dna_lock          : 1; // bit 0
        uint32_t decrypt_key_lock  : 1; // bit 1
        uint32_t gwe_high          : 1; // bit 2
        uint32_t gsrn_high         : 1; // bit 3
        uint32_t goe_high          : 1; // bit 4
        uint32_t spi_ab            : 1; // bit 5
        uint32_t spi_single_mode   : 1; // bit 6
        uint32_t spi_dual_mode     : 1; // bit 7
        uint32_t spi_quad_mode     : 1; // bit 8
        uint32_t msel_latch        : 3; // bits 9-11
        uint32_t internal_done     : 1; // bit 12
        uint32_t jtag_burst        : 1; // bit 13
        uint32_t njtag_cfg_mode    : 1; // bit 14
        uint32_t clear_memory      : 1; // bit 15
        uint32_t preamble_aes      : 1; // bit 16
        uint32_t preamble_am4      : 1; // bit 17
        uint32_t preamble          : 1; // bit 18
        uint32_t resync_preamble   : 1; // bit 19
        uint32_t pll_locked        : 2; // bit 20-21
        uint32_t init_ok           : 1; // bit 22
        uint32_t initn_o           : 1; // bit 23
        uint32_t pcm_x8_mode       : 1; // bit 24
        uint32_t pcm_x16_mode      : 1; // bit 25
        uint32_t pcm_x32_mode      : 1; // bit 26
        uint32_t isc_not_edit      : 1; // bit 27
        uint32_t jtag_prgm_done    : 1; // bit 28
        uint32_t njtag_prgm_done   : 1; // bit 29
        uint32_t icap_enable       : 1; // bit 30
        uint32_t wakeup_mux_dly20  : 1; // bit 31
    } reg_fields;
} status0_t;


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