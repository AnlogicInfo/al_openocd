#ifndef LOADER_DWCMSHC_H
#define LOADER_DWCMSHC_H
#include <stdint.h>

#define BLOCK_SIZE                (512)
#define BLOCK_SIZE_IN_WORD        (BLOCK_SIZE >> 2)

#define ARGUMENT_R                (0x8 >> 2)
#define XFER_CMD_R                (0xC >> 2)
#define BUF_DATA_R                (0x20>> 2)
#define NORMAL_ERROR_INT_R        (0x30>> 2)

#define XFER_DIR_WRITE            (0<<4)
#define XFER_DIR_READ             (1<<4)
#define XFER_BLOCK_COUNT_ENABLE   (1<<1)
#define XFER_RESP_ERR_CHK_ENABLE  (1<<7)

#define CMD_RESP_TYPE_SEL_48      (2<<(0+16)) 
#define CMD_DATA_PRESENT          (1<<(5+16))
#define CMD_CRC_CHECK_ENABLE      (1<<(3+16))
#define CMD_IDX_CHECK_ENABLE      (1<<(4+16))
#define CMD_INDEX_24              (24<<(8+16))
#define CMD_INDEX_17              (17<<(8+16))

#define INT_CMD_COMPLETE_OFFSET   (0)
#define INT_XFER_COMPLETE_OFFSET  (1)
#define INT_BUF_WR_READY          (4)
#define INT_BUF_RD_READY          (5)


#define WR_SINGLE_BLK             (XFER_DIR_WRITE | XFER_BLOCK_COUNT_ENABLE | XFER_RESP_ERR_CHK_ENABLE | \
                                  CMD_RESP_TYPE_SEL_48 | CMD_DATA_PRESENT | CMD_CRC_CHECK_ENABLE | CMD_IDX_CHECK_ENABLE | \
                                  CMD_INDEX_24)

#define RD_SINGLE_BLK             (XFER_DIR_READ | XFER_BLOCK_COUNT_ENABLE | XFER_RESP_ERR_CHK_ENABLE | \
                                  CMD_RESP_TYPE_SEL_48 | CMD_DATA_PRESENT | CMD_CRC_CHECK_ENABLE | CMD_IDX_CHECK_ENABLE | \
                                  CMD_INDEX_17)

#define reg_write(addr, value) \
do { \
    *(volatile uint32_t *)(addr) = (value); \
}while(0)

#define reg_read(addr) (*(volatile uint32_t *)(addr))

uint32_t  emmc_wait_fifo(uint32_t *work_area_start);
void emmc_write_block(volatile uint32_t *ctrl_base, uint32_t offset, const uint32_t *buffer);
void emmc_read_block(volatile uint32_t *ctrl_base, uint32_t *buffer, uint32_t offset, uint32_t count);

#endif