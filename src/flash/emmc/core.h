/*
 * File: core.h
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */

#ifndef OPENOCD_FLASH_EMMC_CORE_H
#define OPENOCD_FLASH_EMMC_CORE_H
#include <flash/common.h>

struct emmc_device 
{
    const char *name;
    struct target *target;
    struct target *trans_target;
    struct emmc_flash_controller *controller;
    void *controller_priv;
    struct emmc_info *device;

    struct emmc_device *next;
};


/*
* Emmc spec defines
*/

// Emmc block defines
// #define EMMC_BLOCK_SIZE                            512

/** 
  * @brief SDIO Commands  Index 
  */
#define SD_CMD_GO_IDLE_STATE                       ((uint8_t)0)
#define SD_CMD_SEND_OP_COND                        ((uint8_t)1)
#define SD_CMD_ALL_SEND_CID                        ((uint8_t)2)
#define SD_CMD_SET_REL_ADDR                        ((uint8_t)3) /*!< SDIO_SEND_REL_ADDR for SD Card */
#define SD_CMD_SET_DSR                             ((uint8_t)4)
#define SD_CMD_SDIO_SEN_OP_COND                    ((uint8_t)5)
#define SD_CMD_HS_SWITCH                           ((uint8_t)6)
#define SD_CMD_SEL_DESEL_CARD                      ((uint8_t)7)
#define SD_CMD_HS_SEND_EXT_CSD                     ((uint8_t)8)
#define SD_CMD_SEND_CSD                            ((uint8_t)9)
#define SD_CMD_SEND_CID                            ((uint8_t)10)
#define SD_CMD_READ_DAT_UNTIL_STOP                 ((uint8_t)11) /*!< SD Card doesn't support it */
#define SD_CMD_STOP_TRANSMISSION                   ((uint8_t)12)
#define SD_CMD_SEND_STATUS                         ((uint8_t)13)
#define SD_CMD_HS_BUSTEST_READ                     ((uint8_t)14)
#define SD_CMD_GO_INACTIVE_STATE                   ((uint8_t)15)
#define SD_CMD_SET_BLOCKLEN                        ((uint8_t)16)
#define SD_CMD_READ_SINGLE_BLOCK                   ((uint8_t)17)
#define SD_CMD_READ_MULT_BLOCK                     ((uint8_t)18)
#define SD_CMD_HS_BUSTEST_WRITE                    ((uint8_t)19)
#define SD_CMD_WRITE_DAT_UNTIL_STOP                ((uint8_t)20) /*!< SD Card doesn't support it */
#define SD_CMD_SET_BLOCK_COUNT                     ((uint8_t)23) /*!< SD Card doesn't support it */
#define SD_CMD_WRITE_SINGLE_BLOCK                  ((uint8_t)24)
#define SD_CMD_WRITE_MULT_BLOCK                    ((uint8_t)25)
#define SD_CMD_PROG_CID                            ((uint8_t)26) /*!< reserved for manufacturers */
#define SD_CMD_PROG_CSD                            ((uint8_t)27)
#define SD_CMD_SET_WRITE_PROT                      ((uint8_t)28)
#define SD_CMD_CLR_WRITE_PROT                      ((uint8_t)29)
#define SD_CMD_SEND_WRITE_PROT                     ((uint8_t)30)
#define SD_CMD_SD_ERASE_GRP_START                  ((uint8_t)32) /*!< To set the address of the first write
                                                                  block to be erased. (For SD card only) */
#define SD_CMD_SD_ERASE_GRP_END                    ((uint8_t)33) /*!< To set the address of the last write block of the
                                                                  continuous range to be erased. (For SD card only) */
#define SD_CMD_ERASE_GRP_START                     ((uint8_t)35) /*!< To set the address of the first write block to be erased.
                                                                  (For MMC card only spec 3.31) */

#define SD_CMD_ERASE_GRP_END                       ((uint8_t)36) /*!< To set the address of the last write block of the
                                                                  continuous range to be erased. (For MMC card only spec 3.31) */

#define SD_CMD_ERASE                               ((uint8_t)38)
#define SD_CMD_FAST_IO                             ((uint8_t)39) /*!< SD Card doesn't support it */
#define SD_CMD_GO_IRQ_STATE                        ((uint8_t)40) /*!< SD Card doesn't support it */
#define SD_CMD_LOCK_UNLOCK                         ((uint8_t)42)
#define SD_CMD_APP_CMD                             ((uint8_t)55)
#define SD_CMD_GEN_CMD                             ((uint8_t)56)
#define SD_CMD_NO_CMD                              ((uint8_t)64)

//CMD PARAMETER
#define EMMC_CMD0_PARA_GO_IDLE_STATE            ((uint32_t)0x0)
#define EMMC_CMD0_PARA_GO_PRE_IDLE_STATE        ((uint32_t)0xF0F0F0F0)
#define EMMC_CMD0_PARA_BOOT_INITIATION          ((uint32_t)0xFFFFFFFA)
#define EMMC_CMD3_PARA_DEFAULT_VAL              ((uint32_t)0x10000) //bit[31:16]    default relative addr
#define EMMC_CMD6_PARA_4_BIT_WIDTH_BUS          ((uint32_t)0x03B70100)
#define EMMC_CMD6_PARA_8_BIT_WIDTH_BUS          ((uint32_t)0x03B70200)
#define EMMC_CMD16_PARA_BLOCK_LEN_512           ((uint32_t)0x200)

// Response defines
#define EMMC_NO_RESP                            0
#define EMMC_RESP_R1                            1
#define EMMC_RESP_R1b                           2
#define EMMC_RESP_R2                            3
#define EMMC_RESP_R3                            4
#define EMMC_RESP_R4                            5
#define EMMC_RESP_R5                            6

//OCR register status
#define EMMC_OCR_HIGH_VOLTAGE                   0x0
#define EMMC_OCR_DUAL_VOLTAGE                   0x1
#define EMMC_OCR_VOLTAGE_2V7_3V6                0x1FF
#define EMMC_OCR_ACCESS_MODE_BYTE_MODE          0x0
#define EMMC_OCR_ACCESS_MODE_SECTOR_MODE        0x2

#define SECTOR_COUNT_OFFSET                     (212 >> 2)

typedef union{
     uint32_t d32;
    struct {
        uint32_t reserved6_0 : 7;
        uint32_t voltage_mode : 1;
        uint32_t reserved14_8 : 7;
        uint32_t voltage2v7_3v6 : 9;
        uint32_t reserved28_24 : 5;
        uint32_t access_mode : 2;
        uint32_t card_power_up_status : 1;
    }bit;
} OCR_R;


enum 
{
    EMMC_MFR_SAMSUNG = 0x15,
    EMMC_MFR_MICRON  = 0x13,
    EMMC_MFR_FORESEE = 0xD6,
    EMMC_MFR_HYNIX = 0x90,
    EMMC_MFR_SANDISK = 0x45,
    EMMC_MFR_XINCUN = 0xAD
};

struct emmc_manufacture {
    int id;
    const char *name;
};

struct emmc_info {
    int mfr_id;
    size_t prd_name;
    int block_size;
    int chip_size;
    const char *name;
};

struct emmc_device *get_emmc_device_by_num(int num);

int emmc_register_commands(struct command_context *cmd_ctx);
COMMAND_HELPER(emmc_command_get_device, unsigned name_index, struct emmc_device **emmc);
COMMAND_HELPER(emmc_command_auto_probe, unsigned name_index, struct emmc_device **emmc);


#define         ERROR_EMMC_DEVICE_INVALID               (-1100)
#define         ERROR_EMMC_OPERATION_FAILED             (-1101)
#define         ERROR_EMMC_OPERATION_TIMEOUT    (-1102)
#define         ERROR_EMMC_OPERATION_NOT_SUPPORTED      (-1103)
#define         ERROR_EMMC_DEVICE_NOT_PROBED    (-1104)
#define         ERROR_EMMC_ERROR_CORRECTION_FAILED      (-1105)
#define         ERROR_EMMC_NO_BUFFER                    (-1106)


#endif
