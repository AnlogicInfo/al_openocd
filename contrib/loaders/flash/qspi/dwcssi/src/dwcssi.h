
#ifndef LOADER_DWCSSI_H
#define LOADER_DWCSSI_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// void dwcssi_config_eeprom(volatile uint32_t *ctrl_base, uint32_t len);
// void dwcssi_config_tx(volatile uint32_t *ctrl_base, uint8_t frf, uint32_t tx_total_len, uint32_t tx_start_lv);
// int dwcssi_txwm_wait(volatile uint32_t *ctrl_base);
// int dwcssi_wait_flash_idle(volatile uint32_t *ctrl_base);
uint32_t wait_fifo(uint32_t *work_area_start);
int dwcssi_txwm_wait(volatile uint32_t *ctrl_base);
void dwcssi_disable(volatile uint32_t *ctrl_base);
void dwcssi_enable(volatile uint32_t *ctrl_base);
int dwcssi_flash_wr_en(volatile uint32_t *ctrl_base, uint8_t frf);
int dwcssi_wait_flash_idle(volatile uint32_t *ctrl_base);
int dwcssi_tx_buf_no_wait(volatile uint32_t *ctrl_base, const uint8_t* in_buf, uint32_t in_cnt);

int dwcssi_write_buffer(volatile uint32_t *ctrl_base, const uint8_t *buffer, uint32_t offest, uint32_t len, uint32_t flash_info, uint32_t spictrl);
int dwcssi_write_buffer_x1(volatile uint32_t *ctrl_base, const uint8_t *buffer, uint32_t offset, uint32_t len, uint32_t flash_info);
int dwcssi_read_page(volatile uint32_t *ctrl_base, uint8_t *buffer, uint32_t offset, uint32_t len, uint32_t qread_cmd);
#endif