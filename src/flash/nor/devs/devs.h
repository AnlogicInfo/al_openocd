#ifndef FLASH_DEVS_H
#define FLASH_DEVS_H
#include <flash\nor\spi.h>

int sp_s25fl_reset(struct flash_bank *bank);
int sp_s25fl_quad_en(struct flash_bank* bank);
int sp_s25fl_quad_dis(struct flash_bank* bank);

flash_ops_t sp_s25fl_ops = FLASH_OPS(0x6C, 0x34, sp_s25fl_reset, sp_s25fl_quad_en, sp_s25fl_quad_dis);



#endif

