#ifndef OPENOCD_FLASH_EMMC_DWC_MSHC_MMC_H
#define OPENOCD_FLASH_EMMC_DWC_MSHC_MMC_H

typedef enum{
	//	COMMON STATUS
	MMC_SUCCESS = 0,
	MMC_FAILURE,
	//TIMER OUT ERROR
	//MMC_CMD_TIMEOUT,				//send cmd timeout
	//MMC_XFER_TIMEOUT,				//wait transfer complete timeout
	MMC_WAIT_CLK_STABLE_TIMEOUT,	//wait internal clock stable timeout
	//MMC_BUF_RD_RDY_TIMEOUT,			//wait buffer read ready complete timeout
	//MMC_SD_CHECK_VOLT_TIMEOUT,		//SD check voltage timeout
	//MMC_EMMC_CHECK_VOLT_TIMEOUT,	//eMMC check voltage timeout
	MMC_CHECK_VOLT_TIMEOUT,
	MMC_CHECK_DEV_STATUS_TIMEOUT,
	MMC_WAIT_LINE_INHIBIT_TIMEOUT,
	//Error INT Status Regitster
	MMC_CMD_TOUT_ERR,				//no response is returned within 64 clock cycles from end bit of the cmd
	MMC_CMD_CRC_ERR,				//cmd resp CRC err
									//host controller detects a CMD line conflict
	MMC_CMD_END_BIT_ERR,			//detect the end bit of a cmd resp is 0
	MMC_CMD_IDX_ERR,				//cmd index err in the cmd resp
	MMC_DATA_TOUT_ERR,				//busy timeout for R1b,R5b type
									//busy timeout after write CRC status
									//write CRC status timeout
									//Read Data timeout
	MMC_DATA_CRC_ERR,				//CRC err when transfer rd data use DAT line
									//write status have a value other than 010
									//write CRC status timeout
	MMC_DATA_END_BIT_ERR,			//detect 0 at the end bit pos of read data use DAT line
									//detect 0 at the end bit pos of the CRC status
	MMC_WRONG_FREQ,
	MMC_MODE_ERROR,
	MMC_CMD_0_ERR,
	MMC_CMD_1_ERR,
	MMC_CMD_2_ERR,
	MMC_CMD_3_ERR,
	MMC_CMD_6_ERR,
	MMC_CMD_7_ERR,
	MMC_CMD_8_ERR,
	MMC_CMD_9_ERR,
	MMC_CMD_10_ERR,
	MMC_CMD13_ERR,
	MMC_CMD_16_ERR,
	MMC_CMD_17_ERR,
	MMC_CMD_23_ERR,
	MMC_CMD_24_ERR,
	MMC_CMD_55_ERR,
	MMC_ACMD_41_ERR,
	MMC_CMD_7_XFER_ERR,
	MMC_CMD_8_XFER_ERR,
	MMC_CMD_17_XFER_ERR,
	MMC_CMD_24_XFER_ERR,
	MMC_CMD_XFER_ERR,
	MMC_ERR_MAX
}MMC_ERR_TYPE;

//error code offset
#define MMC_ERROR_CODE_OFFSET		MMC_CMD_TOUT_ERR
//error cmd offset
#define MMC_ERROR_CMD_OFFSET		MMC_CMD_0_ERR
//Error INT status bits	length 13~15 reserved
#define MMC_ERR_INT_STAT_BITS_LEN	7


//IO BANK1 REF
//bit 31-3:reserved
//bit 2:bank1_vccio_det3v3
//bit 1:bank1_vccio_det2v5
//bit 0:bank1_vccio_det1v8
#define IO_BANK1_REF                0xF8803C04ULL
#define MBIU_CTRL_R 0x510   //AHB BUS burst contrl register

//top cfg register
//bit 7:enable reg ctrl card write protection   0:io ctrl   1:reg ctrl
//bit 6:enable reg ctrl card detection          0:io ctrl   1:reg ctrl
//bit 5:reg ctrl write protection               0:disable   1:enable
//bit 4:reg ctrl card detect                    0:enable    1:disable
//bit 3:clk soft rst                            0:disable   1:enable
//bit [2:0]:clk phase select similar to tuning
#define TOP_NS__CFG_CTRL_SDIO0_ADDR 0xF8800150ULL   //sd0   emmc
#define TOP_NS__CFG_CTRL_SDIO1_ADDR 0xF8800150ULL	//0xF8800154ULL   //sd1   sd
#define SDIO_WRAP__SDIO0__BASE_ADDR 0xF8049000ULL
#define SDIO_WRAP__SDIO1__BASE_ADDR 0xF8049000ULL	//0xF804A000UL


#define MMC_CMD_TIMEOUT_VAL					(10000*1000)	//10s
#define MMC_XFER_TIMEOUT_VAL				(15000*1000)	//15s
#define MMC_BUF_RD_RDY_TIMEOUT_VAL			(1000*1000)		//1s
#define MMC_WAIT_CLK_STABLE_TIMEOUT_VAL		(5000*1000)		//5s
#define MMC_CHECK_LINE_INHIBIT_TIMEOUT_VAL	(1000*1000)		//1s
#define MMC_CHECK_DEV_STATUS_TIMEOUT_VAL	(1000*1000)		//1s
#define MMC_DELAY_SCALE						(2)

//capabilities1.base_clk_freq 0 another way 1~255 -> 1~255MHz
#define MMC_GET_INFO_ANOTHER_WAY	0x0
//capabilities2.clk_mul 0 not support 1~255 -> 2~256
#define MMC_CLK_MUL_NOT_SUPPORT	0x0

#define     __IO    volatile
#define DEF_BLOCK_LEN   0x1000	//4KB




#endif