#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*Register offsets*/
#define ONFI_AXI_DATA_WIDTH			4		/* AXI bus width */

#define NAND_COMMAND_PHASE_FLAG	0x00000000		/* Command phase flag */
#define NAND_DATA_PHASE_FLAG	0x00080000		/* Data phase flag */

#define ONFI_CMD_PROGRAM_PAGE1						0x80	/* ONFI Read ID Start command */
#define ONFI_CMD_PROGRAM_PAGE2						0x10	/* ONFI Read ID End command */
#define ONFI_CMD_PROGRAM_PAGE_CYCLES				5		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_PROGRAM_PAGE_END_TIMING 			ONFI_ENDIN_DATA_PHASE/* READ_ID End Cmd Invalid */

/* Provides the status of the ECC block 0 = idle 1 = busy*/
#define SMC_EccStatus_EccStatus_FIELD				(6)

/* Indicates if the ECC value for block <a> is valid */
#define SMC_EccBlock_ISCheakValueValid_FIELD		(30)

#define SMC_REG_MEMC_STATUS         	0x00UL
#define SMC_REG_MEMIF_CFG         		0x04UL
#define SMC_REG_MEM_CFG_SET        		0x08UL
#define SMC_REG_MEM_CFG_CLR       		0x0cUL
#define SMC_REG_DIRCT_CMD           	0x10UL
#define SMC_REG_SET_CYCLES            	0x14UL

#define SMC_REG_SET_OPMODE        		0x18UL
#define SMC_REG_REFRESH_0       		0x20UL
#define SMC_REG_REFRESH_1           	0x24UL

#define SMC_REG_NAND_CYCLES1_0          0x180UL
#define SMC_REG_OPMODE1_0               0x184UL

#define SMC_REG_USER_CONFIG             0x204UL
#define SMC_REG_USER_STATUS             0x200UL

#define SMC_REG_ECC1_STATUS             0x400UL
#define SMC_REG_ECC1_CFG                0X404UL
#define SMC_REG_ECC1_MEMCMD0            0X408UL
#define SMC_REG_ECC1_MEMCMD1            0X40CUL
#define SMC_REG_ECC1_ADDR0              0X410UL
#define SMC_REG_ECC1_ADDR1              0X414UL
#define SMC_REG_ECC1_BLOCK0             0X418UL
#define SMC_REG_ECC1_EXTRA_BLOCK        0X428UL

#define SMC_MemCfgClr_ClrSmcInt1_FIELD	(4)
#define SMC_MemCfgClr_ClrSmcInt1		(1 << SMC_MemCfgClr_ClrSmcInt1_FIELD)

#define ERROR_OK						(0)

#define NAND_BUSY	0
#define NAND_READY	1

#define FAILED_FLAG			1	/* return failed flag */
#define SUCCESS_FLAG		0	/* return success flag */
#define NAND_WRITE_PROTECTED 2	

/* Raw status of the smc_int1 interrupt signal */
#define SMC_MemcStatus_SmcInt1RawStatus_FIELD		(6)

enum {
	/* Standard NAND flash commands */
	NAND_CMD_READ0 = 0x0,
	NAND_CMD_READ1 = 0x1,
	NAND_CMD_RNDOUT = 0x5,
	NAND_CMD_PAGEPROG = 0x10,
	NAND_CMD_READOOB = 0x50,
	NAND_CMD_ERASE1 = 0x60,
	NAND_CMD_STATUS = 0x70,
	NAND_CMD_STATUS_MULTI = 0x71,
	NAND_CMD_SEQIN = 0x80,
	NAND_CMD_RNDIN = 0x85,
	NAND_CMD_READID = 0x90,
	NAND_CMD_ERASE2 = 0xd0,
	NAND_CMD_RESET = 0xff,

	/* Extended commands for large page devices */
	NAND_CMD_READSTART = 0x30,
	NAND_CMD_RNDOUTSTART = 0xE0,
	NAND_CMD_CACHEDPROG = 0x15,
};

/* ONFI Status Register Mask */
#define ONFI_STATUS_FAIL			0x01	/* Status Register : FAIL */
#define ONFI_STATUS_FAILC			0x02	/* Status Register : FAILC */
#define ONFI_STATUS_ARDY			0x20	/* Status Register : ARDY */
#define ONFI_STATUS_RDY				0x40	/* Status Register : RDY */
#define ONFI_STATUS_WP				0x80	

#define ONFI_CMD_READ_STATUS1						0x70	/* ONFI Read Status command Start */
#define ONFI_CMD_READ_STATUS2						ONFI_END_CMD_NONE	/* ONFI Read Status command End */
#define ONFI_CMD_READ_STATUS_CYCLES					0		/* ONFI Read Status command total address cycles*/
#define ONFI_CMD_READ_STATUS_END_TIMING				ONFI_END_CMD_INVALID 

enum SMC_ERROR_CODE{
	SmcSuccess = 0,				/* 返回成功 */
	SmcResetErr,				/* 复位失败错误 */ 
	SmcParaBusyStatusErr,		/* Read parameter data happen status reg error */
	SmcCrcErr,					/* Parameter page三次crc都失败出现的错误 */
	SmcParameterOver,			/* 读Parameter Page发现读取出来的数据，表示的page datasize大小远远超出了正常大小 */
	SmcSamsungParamOver,
	SmcHwInitSizeErr,			/* Smc ecc最多能支持的page size的大小为2048.当使用Smc的ecc功能并且nandflash大小超过时出现此错误。*/
	SmcHwInitEccBusyErr,
	SmcHwDisEccBusyErr,
	SmcFeatBusyErr,
	SmcWriteEccFeatErr,			/* 使能On Die Ecc但是使能失败产生的错误 */
	SmcSpareStatusErr,			/* Read spare data happen status reg error */
	SmcBadBlock,				/* 检测到了坏块 */
	SmcReadCmdTimeOutErr,
	SmcHwReadSizeOver,
	SmcCalEccBusyErr,
	SmcEccDataInvalidErr,			/* 读取Smc ecc数据，但是ecc数据被标记为无效 */
	SmcTwoBitsErr,
	SmcMultipleBitsErr,

#if 0
	SmcEccBusyErr0,				/* Nand_EccHwDisable，SMC Ecc busy时间超过5ms产生的错误 */
	SmcEccBusyErr1,				/* Nand_EccHwInit，SMC Ecc busy时间超过5ms产生的错误 */
	SmcFeatBusyErr,				/* GetFeature/SetFeature 5ms超时，发生busy错误 */

	SmcSizeOverErr,
	SmcReadAlignErr1,			/* Nand_ReadPage_HwEcc传递的参数出现错误。未4byte对齐，或者Length=0. */
	SmcBusyTimeOutErr,			/* 调用ReadPage Cmd中判断nandflash为busy并且持续5ms时间。*/
	SmcOnfiStatusErr,			/* ReadPage Cmd之后发送读取状态寄存器的命令 */
	SmcEccBusyErr2,				/* 在数据传输读取结束之后，读取Smc ecc数据，但是ecc busy持续了5ms */

	SmcTwoBitsErr,				/* ecc出现了2bit错误 */
	SmcMultipleBitsErr,			/* Ecc出现了多bit错误 */
	SmcReadAlignErr2,			/* Nand_ReadPage传递的参数出现错误。未4byte对齐，或者Length=0. */
	SmcDataEccErr,				/* On die ecc情况下出现了超出On die ecc纠错位数能力的错误 */
	SmcLongEccSectorErr,		/* samsung flash出现了超出On die ecc纠错位数能力的错误 */
	SmcBadBlock,
	SmcOffsetErr
#endif
};

static uint32_t __attribute__((aligned(4))) NandOob64[12] = {52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
static uint32_t __attribute__((aligned(4))) NandOob32[6] = {26, 27, 28, 29, 30, 31};
static uint32_t __attribute__((aligned(4))) NandOob16[3] = {13, 14, 15};		/* data size 512bytes */


int smc35x_ecc_calculate(uint32_t ctrl_base, uint8_t *ecc_data, uint32_t ecc_data_nums)
{
	uint8_t count = 0, status = 0;
	volatile uint8_t ecc_reg = 0;
	uint32_t ecc_value = 0;
	void *ecc_addr = NULL, *status_addr = NULL;

	uint8_t nums = 0;
	while (ecc_data_nums >= 3) {
		++nums;
		ecc_data_nums -= 3;
	}

	/* Check busy signal if it is busy to poll*/
	do {
		// target_read_u8(target, (ctrl_base + SMC_REG_ECC1_STATUS), &status);
        status_addr = (void *)(uintptr_t)(ctrl_base + SMC_REG_ECC1_STATUS);
		status = *(uint8_t *)status_addr;
		status &= (1 << SMC_EccStatus_EccStatus_FIELD);
	}
	while (status);

	for (ecc_reg = 0; ecc_reg < nums; ++ecc_reg)
	{
		// target_read_u32(target, (ctrl_base + SMC_REG_ECC1_BLOCK0 + ecc_reg * 4), &ecc_value);
        ecc_addr = (void *)(uintptr_t)(ctrl_base + SMC_REG_ECC1_BLOCK0);
		ecc_value = *(uint32_t *)ecc_addr;
		// printf("ecc value: %lx", ecc_value);
		if((ecc_value) & (1 << SMC_EccBlock_ISCheakValueValid_FIELD))
		{
			for(count=0; count < 3; ++count)
			{
				*ecc_data = ecc_value & 0xFF;
				// printf("dst: %d", *ecc_data);
				ecc_value = ecc_value >> 8;
				++ecc_data;
			}
		}
		else
		{
			// printf("EccInvalidErr");
			return SmcEccDataInvalidErr;
		}
	}

	return ERROR_OK;
}

uint8_t nand_busy(uint32_t ctrl_base)
{
	uint32_t status = *(volatile uint32_t *)(ctrl_base + SMC_REG_MEMC_STATUS);
	status &= (1 << SMC_MemcStatus_SmcInt1RawStatus_FIELD);

	

	if(status)
		return NAND_READY;
	else
		return NAND_BUSY;
}

int flash_smc35x(uint32_t ctrl_base, uint32_t page_size, void *pbuffer, uint32_t offset, uint32_t count, uint32_t nand_base, uint32_t ecc_num)
{
	uint32_t flag = 0, retvel = 0;
    uint32_t index, status;
    uint32_t oob_size = count - page_size;
	uint32_t eccDataNums = 0, *dataOffsetPtr = NULL;
	uint8_t eccData[12] = {0}, *buffer = pbuffer;
	void *status_addr = NULL;

	void *cmd_phase_addr = (void *)(uintptr_t)(nand_base | (ONFI_CMD_READ_STATUS1 << 3));
	uint32_t cmd_phase_data = -1;
	*(uint32_t *)cmd_phase_addr = cmd_phase_data;

	void *data_phase_addr = (void *)(uintptr_t)(nand_base | NAND_DATA_PHASE_FLAG);
	status = *(uint8_t *)data_phase_addr;
	if (!(status & ONFI_STATUS_WP)) {
		return FAILED_FLAG;
	}

	cmd_phase_addr = (void *)(uintptr_t)(nand_base | (ONFI_CMD_PROGRAM_PAGE_CYCLES << 21) | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (ONFI_CMD_PROGRAM_PAGE1 << 3));
	cmd_phase_data = 0 | (offset << (2*8));
	*(uint32_t *)cmd_phase_addr = cmd_phase_data;
	cmd_phase_data = offset >> (32 - (2*8));
    *(uint32_t *)cmd_phase_addr = cmd_phase_data;

	data_phase_addr = (void *)(uintptr_t)(nand_base | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11));
	
	for (index = 0; index < page_size - ONFI_AXI_DATA_WIDTH; ++index)
	{
		// target_write_u8(target, data_phase_addr, data[index]);
        *(uint8_t *)data_phase_addr = buffer[index];
		if (flag != 2) {
			retvel = *(uint8_t *)data_phase_addr;
			++flag;
		}
	}
	data_phase_addr = (void *)(uintptr_t)(nand_base | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (1 << 10));
	buffer += page_size - ONFI_AXI_DATA_WIDTH;
	for (index = 0; index < ONFI_AXI_DATA_WIDTH; ++index)
	{
		// target_write_u8(target, data_phase_addr, data[index]);
        *(uint8_t *)data_phase_addr = buffer[index];
	}
    buffer += ONFI_AXI_DATA_WIDTH;

	switch(oob_size)
	{
		case(16):
			eccDataNums = 3;
			dataOffsetPtr = NandOob16;
			break;
		case(32):
			eccDataNums = 6;
			dataOffsetPtr = NandOob32;
			break;
		case(64):
			eccDataNums = 12;
			dataOffsetPtr = NandOob64;
			break;
		case(224):
			eccDataNums = 12;
			dataOffsetPtr = NandOob64;
			break;
		default:
			/* Page size 256 bytes & 4096 bytes not supported by ECC block */
			break;
	}
	if (ecc_num == 1 && dataOffsetPtr != NULL && eccDataNums != 0) {
		smc35x_ecc_calculate(ctrl_base, eccData, eccDataNums);
		for(index = 0; index < eccDataNums; index++)
		{
			buffer[dataOffsetPtr[index]] = (~eccData[index]);
		}
	}

	data_phase_addr = (void *)(uintptr_t)(nand_base | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11));
	for (index = 0; index < oob_size - ONFI_AXI_DATA_WIDTH; ++index)
	{
		// target_write_u8(target, data_phase_addr, oob_data[index]);
        *(uint8_t *)data_phase_addr = buffer[index];
	}
	data_phase_addr = (void *)(uintptr_t)(nand_base | (1 << 21) | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11));
	buffer += oob_size - ONFI_AXI_DATA_WIDTH;
	for (index = 0; index < ONFI_AXI_DATA_WIDTH; ++index)
	{
		// target_write_u8(target, data_phase_addr, oob_data[index]);
        *(uint8_t *)data_phase_addr = buffer[index];
	}


	// while (nand_busy(ctrl_base) == NAND_BUSY);


	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
    status_addr = (void *)(uintptr_t)(ctrl_base + SMC_REG_MEM_CFG_CLR);
	status = *(volatile uint32_t *)(ctrl_base + SMC_REG_MEM_CFG_CLR);
    *(volatile uint32_t *)status_addr = status | SMC_MemCfgClr_ClrSmcInt1;


	cmd_phase_addr = (void *)(uintptr_t)(nand_base | (ONFI_CMD_READ_STATUS1 << 3));
	cmd_phase_data = -1;
	*(uint32_t *)cmd_phase_addr = cmd_phase_data;

	data_phase_addr = (void *)(uintptr_t)(nand_base | NAND_DATA_PHASE_FLAG);
	status = *(uint8_t *)data_phase_addr;
	if (!(status & ONFI_STATUS_FAIL)) {
		return FAILED_FLAG;
	}

	return retvel;
}