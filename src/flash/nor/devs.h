#ifndef FLASH_DEVS_H
#define FLASH_DEVS_H
#include "spi.h"

int sp_s25fl_reset(struct flash_bank *bank);
int sp_s25fl_quad_en(struct flash_bank* bank);
int sp_s25fl_quad_dis(struct flash_bank* bank);

const enhance_mode_t sp_s25fl_enhance = FLASH_ENHANCE(0x6C, 0x34, sp_s25fl_reset, sp_s25fl_quad_en, sp_s25fl_quad_dis);



#endif

