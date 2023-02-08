#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define SMC_BASE				0xF841A000UL
#define NAND_BASE				0x64000000UL

/*NAND Flash Manufacturer ID Codes*/
enum {
	NAND_MFR_TOSHIBA = 0x98,
	NAND_MFR_SAMSUNG = 0xec,
	NAND_MFR_FUJITSU = 0x04,
	NAND_MFR_NATIONAL = 0x8f,
	NAND_MFR_RENESAS = 0x07,
	NAND_MFR_STMICRO = 0x20,
	NAND_MFR_HYNIX = 0xad,
	NAND_MFR_MICRON = 0x2c,
	NAND_MFR_ESMT = 0xc8,
};

/********* Interface1 ecc register *********/
#define SMC_REG_ECC1_MEMCMD0            0X408UL
#define SMC_REG_ECC1_MEMCMD1            0X40CUL

 /* The NAND<x> command to initiate a write (0x80) */
 #define SMC_EccMemCmd0_InitWriteCmd_FIELD   (0)
 #define SMC_EccMemCmd0_InitWriteCmd         (0x80 << SMC_EccMemCmd0_InitWriteCmd_FIELD)

  /* The NAND<x> command to initiate a read (0x00) */
 #define SMC_EccMemCmd0_InitReadCmd_FIELD    (8)
 #define SMC_EccMemCmd0_InitReadCmd          (0x00 << SMC_EccMemCmd0_InitReadCmd_FIELD)
 #define SMC_EccMemCmd0_InitReadChangeColumnCmd          (0x05 << SMC_EccMemCmd0_InitReadCmd_FIELD)

 /* Use end command */
 #define SMC_EccMemCmd0_UseEndCmd_FIELD      (24)
 #define SMC_EccMemCmd0_UseEndCmd            (1 << SMC_EccMemCmd0_UseEndCmd_FIELD)

 /* The NAND<x> command to indicate the end of a read (0x30) */
 #define SMC_EccMemCmd0_EndReadCmd_FIELD     (16)
 #define SMC_EccMemCmd0_EndReadCmd           (0x30 << SMC_EccMemCmd0_EndReadCmd_FIELD)
 #define SMC_EccMemCmd0_EndReadCacheCmd      (0x31 << SMC_EccMemCmd0_EndReadCmd_FIELD)
 #define SMC_EccMemCmd0_EndReadChangeColumnCmd      (0xE0 << SMC_EccMemCmd0_EndReadCmd_FIELD)

 /* Use end command */
#define SMC_EccMemCmd1_UseEndCmd_FIELD      (24)
#define SMC_EccMemCmd1_UseEndCmd            (1  << SMC_EccMemCmd1_UseEndCmd_FIELD)

/* The NAND command to indicate the end of a column change read (0xE0) */
#define SMC_EccMemCmd1_ColChangeEndCmd_FIELD    (16)
#define SMC_EccMemCmd1_ColChangeEndCmd          (0xE0  << SMC_EccMemCmd1_ColChangeEndCmd_FIELD)

/* The NAND command (0x05) that is either */
#define SMC_EccMemCmd1_ColChangeRDCmd_FIELD     (8)
#define SMC_EccMemCmd1_ColChangeRDCmd           (0x05  << SMC_EccMemCmd1_ColChangeRDCmd_FIELD)

/* The NAND command to initiate a column change write (0x85) */
#define SMC_EccMemCmd1_ColChangeWRCmd_FIELD     (0)
#define SMC_EccMemCmd1_ColChangeWRCmd           (0x85  << SMC_EccMemCmd1_ColChangeWRCmd_FIELD)

/*Register offsets*/
#define ONFI_AXI_DATA_WIDTH			4			/* AXI bus width */
#define NAND_COMMAND_PHASE_FLAG	0x00000000		/* Command phase flag */
#define NAND_DATA_PHASE_FLAG	0x00080000		/* Data phase flag */

#define NO_ECC_LAST			0	/* return failed flag */
#define ECC_LAST			1	/* return success flag */

#define NAND_ECC_BLOCK_SIZE		512		/* ECC block size */
#define NAND_ECC_BYTES			3		/* ECC bytes per ECC block */

#define ONFI_END_CMD_INVALID				0		/* End command invalid */
#define ONFI_ENDIN_CMD_PHASE				1		/* End command in command phase */
#define ONFI_ENDIN_DATA_PHASE				2		/* End command in data phase */

#define ONFI_CMD_PROGRAM_PAGE1						0x80	/* ONFI Read ID Start command */
#define ONFI_CMD_PROGRAM_PAGE2						0x10	/* ONFI Read ID End command */
#define ONFI_CMD_PROGRAM_PAGE_CYCLES				5		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_PROGRAM_PAGE_END_TIMING 			ONFI_ENDIN_DATA_PHASE/* READ_ID End Cmd Invalid */

#define ONFI_CMD_READ_PAGE1							0x00	/* ONFI Read ID command Start */
#define ONFI_CMD_READ_PAGE2							0x30	/* ONFI Read ID command End */
#define ONFI_CMD_READ_PAGE_CYCLES					5		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_READ_PAGE_END_TIMING				ONFI_ENDIN_CMD_PHASE /* READ_ID End Cmd Invalid */

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
#define SMC_REG_ECC1_BLOCK0             0X418UL
/* Provides the status of the ECC block 0 = idle 1 = busy*/
#define SMC_EccStatus_EccStatus_FIELD				(6)
/* Indicates if the ECC value for block <a> is valid */
#define SMC_EccBlock_ISCheakValueValid_FIELD		(30)

#define SMC_MemCfgClr_ClrSmcInt1_FIELD	(4)
#define SMC_MemCfgClr_ClrSmcInt1		(1 << SMC_MemCfgClr_ClrSmcInt1_FIELD)

#define ERROR_OK						(0)
#define FAILED_FLAG			1	/* return failed flag */
#define SUCCESS_FLAG		0	/* return success flag */

#define ONFI_PAGE_NOT_VALID		-1		/* Page is not valid in command phase */
#define ONFI_COLUMN_NOT_VALID	-1		/* Column is not valid in command phase */

/* Raw status of the smc_int1 interrupt signal */
#define SMC_MemcStatus_SmcInt1RawStatus_FIELD		(6)

/* ONFI Status Register Mask */
#define ONFI_STATUS_FAIL			0x01	/* Status Register : FAIL */
#define ONFI_STATUS_WP				0x80	/* Status Register : WR */

#define ONFI_CMD_READ_STATUS1		0x70	/* ONFI Read Status command Start */

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

#define SMC_WriteReg(reg,value)	\
do{		\
	*(volatile uint32_t *)(reg) = (value);	\
}while(0)
#define SMC_ReadReg(reg)			(*(volatile uint32_t *)(reg))
#define SMC_Read8BitReg(reg)	 	(*(volatile uint8_t *)(reg))
#define SMC_Write8BitReg(reg, value) \
do {  \
	*(volatile uint8_t *)(reg) = (value); \
} while(0)
#define OneHot(Value)	(!((Value) & (Value - 1)))

static uint32_t __attribute__((aligned(4))) NandOob64[12] = {52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
static uint32_t __attribute__((aligned(4))) NandOob32[6] = {26, 27, 28, 29, 30, 31};
static uint32_t __attribute__((aligned(4))) NandOob16[3] = {13, 14, 15};		/* data size 512bytes */
static uint8_t oob_data[512] = {0xff, 0xff};

static uint8_t smc35x_ecc_calculate(volatile uint8_t *ecc_data, uint8_t nums)
{
	volatile uint8_t ecc_reg = 0;
	volatile uint32_t ecc_value = 0;
	uint8_t count = 0;

	while (SMC_Read8BitReg(SMC_BASE + SMC_REG_ECC1_STATUS) & (1 << SMC_EccStatus_EccStatus_FIELD));

	for (ecc_reg = 0; ecc_reg < nums; ++ecc_reg) {
		ecc_value = SMC_ReadReg(SMC_BASE + SMC_REG_ECC1_BLOCK0 + ecc_reg*4);
		
		if ((ecc_value) & (1 << SMC_EccBlock_ISCheakValueValid_FIELD)) {
			ecc_data[count++] = ecc_value & 0xFF;
			ecc_value >>= 8;
			ecc_data[count++] = ecc_value & 0xFF;
			ecc_value >>= 8;
			ecc_data[count++] = ecc_value & 0xFF;
		} else {
			return SmcEccDataInvalidErr;
		}
	}

	return SmcSuccess;
}

static uint8_t smc35x_ecc_correct(volatile uint8_t *eccCode, volatile uint8_t *eccCalc, volatile uint8_t *buf)
{
	uint8_t bitPos = 0;
	uint32_t bytePos = 0;
	uint16_t eccOdd = 0, eccEven = 0;
	uint16_t readEccLow = 0, readEccHigh = 0;
	uint16_t calcEccLow = 0, calcEccHigh = 0;

	/* Lower 12 bits of ECC Read */
	readEccLow = (eccCode[0] | (eccCode[1] << 8)) & 0xfff;
	/* Upper 12 bits of ECC Read */
	readEccHigh = ((eccCode[1] >> 4) | (eccCode[2] << 4)) & 0xfff;
	/* Lower 12 bits of ECC calculated */
	calcEccLow = (eccCalc[0] | (eccCalc[1] << 8)) & 0xfff;
	/* Upper 12 bits of ECC Calculated */
	calcEccHigh = ((eccCalc[1] >> 4) | (eccCalc[2] << 4)) & 0xfff;

	eccOdd = readEccLow ^ calcEccLow;
	eccEven = readEccHigh ^ calcEccHigh;

	/* No Error */
	if ((eccOdd == 0) && (eccEven == 0))
		return SmcSuccess;

	/* One bit Error */
	if (eccOdd == (~eccEven & 0xfff)) {
		bytePos = (eccOdd >> 3) & 0x1ff;
		bitPos = eccOdd & 0x07;
		/* Toggling error bit */
		buf[bytePos] ^= (1 << bitPos);
		return SmcSuccess;
	}

	/* Two bits Error */
	if (OneHot((eccOdd | eccEven)) == SmcSuccess)
		return SmcSuccess;

	/* Multiple bits error */
	return SmcMultipleBitsErr;
}

int flash_smc35x(uint32_t page_size, uint8_t *buffer, uint32_t offset, uint32_t oob_size, uint32_t device_id, uint32_t ecc_num)
{
	uint8_t state, eccDataNums = 0, nums = 0, eccOffset = 0;
    uint32_t status;
	volatile uint32_t index = 0;

	volatile uint8_t *ecc_data = buffer + page_size;
	volatile uint8_t *read_ecc_data = ecc_data + 12;

	volatile uint32_t *temp_buffer;
	uint32_t temp_length = 0;
	uint32_t *dataOffsetPtr = NULL;
	uint8_t *page_data = buffer, *poob_data = oob_data;
	
	// volatile unsigned long status_addr = 0;
	volatile unsigned long cmd_phase_addr = 0;
	volatile unsigned long data_phase_addr = 0;
	uint32_t cmd_phase_data;

	/* Send Read Page Command */
	cmd_phase_addr = NAND_BASE | (ONFI_CMD_READ_PAGE_CYCLES << 21) | (ONFI_CMD_READ_PAGE_END_TIMING << 20) | (ONFI_CMD_READ_PAGE2 << 11) | (ONFI_CMD_READ_PAGE1 << 3);
	cmd_phase_data = 0 | (offset << (2*8));
	SMC_WriteReg(cmd_phase_addr, cmd_phase_data);
	cmd_phase_data = offset >> (32 - (2*8));
	SMC_WriteReg(cmd_phase_addr, cmd_phase_data);
	
	/* Check Nand Status */
	do {
		status = (SMC_ReadReg(SMC_BASE + SMC_REG_MEMC_STATUS) & (1 << SMC_MemcStatus_SmcInt1RawStatus_FIELD));
	} while (!status);

	/* Clear SMC Interrupt 1, as an alternative to an AXI read */
	status = SMC_ReadReg((SMC_BASE + SMC_REG_MEM_CFG_CLR));
	SMC_WriteReg((SMC_BASE + SMC_REG_MEM_CFG_CLR), status | SMC_MemCfgClr_ClrSmcInt1);
	
	// asm volatile("hlt #0xB");
	if (ecc_num == 1) {
		/* Read Page Data */
		data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		temp_buffer = (uint32_t *)buffer;
		temp_length = (page_size - ONFI_AXI_DATA_WIDTH) >> 2;
		for (index = 0; index < temp_length; ++index)
			temp_buffer[index] = SMC_ReadReg(data_phase_addr);

		data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11) | (ECC_LAST << 10);
		buffer += (page_size - ONFI_AXI_DATA_WIDTH);
		temp_buffer = (uint32_t *)buffer;
		temp_length = ONFI_AXI_DATA_WIDTH >> 2;
		for (index = 0; index < temp_length; ++index)
			temp_buffer[index] = SMC_ReadReg(data_phase_addr);
		
		/* Calculate ECC */
		switch (oob_size)
		{
			case(16):
				eccDataNums = 3;
				nums = 1;
				dataOffsetPtr = NandOob16;
				break;
			case(32):
				eccDataNums = 6;
				nums = 2;
				dataOffsetPtr = NandOob32;
				break;
			case(64):
				eccDataNums = 12;
				nums = 4;
				dataOffsetPtr = NandOob64;
				break;
			default:
				/* Page size 256 bytes & 4096 bytes not supported by ECC block */
				return SmcHwReadSizeOver;
		}

		state =  smc35x_ecc_calculate(ecc_data, nums);
		if(state != SmcSuccess)
			return state;
		
		/* Read Oob Data */
		data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		temp_buffer = (uint32_t *)poob_data;
		temp_length = (oob_size - ONFI_AXI_DATA_WIDTH) >> 2;
		for (index = 0; index < temp_length; ++index)
			temp_buffer[index] = SMC_ReadReg(data_phase_addr);
		
		data_phase_addr = NAND_BASE | (1 << 21) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		poob_data += oob_size - ONFI_AXI_DATA_WIDTH;
		temp_buffer = (uint32_t *)poob_data;
		temp_length = ONFI_AXI_DATA_WIDTH >> 2;
		for (index = 0; index < temp_length; ++index)
			temp_buffer[index] = SMC_ReadReg(data_phase_addr);
		
		/* Correct ECC */
		for (index = 0; index < eccDataNums; ++index)
			read_ecc_data[index] = ~oob_data[dataOffsetPtr[index]];
		
		index = page_size / NAND_ECC_BLOCK_SIZE;
		for (; index != 0; --index) {
			state = smc35x_ecc_correct(&read_ecc_data[eccOffset], &ecc_data[eccOffset], page_data);
			if (state != SmcSuccess)
				return state;

			eccOffset += NAND_ECC_BYTES;
			page_data += NAND_ECC_BLOCK_SIZE;
		}
	} else {
		if (device_id == NAND_MFR_MICRON)
			SMC_WriteReg(NAND_BASE, 0x00);

		/* Read Page Data */
		data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		temp_buffer = (uint32_t *)buffer;
		temp_length = (page_size) >> 2;
		for (index = 0; index < temp_length; ++index) {
			temp_buffer[index] = SMC_ReadReg(data_phase_addr);
		}

		/* Read Oob Data */
		temp_buffer = (uint32_t *)poob_data;
		temp_length = (oob_size - ONFI_AXI_DATA_WIDTH) >> 2;
		for (index = 0; index < temp_length; ++index) {
			temp_buffer[index] = SMC_ReadReg(data_phase_addr);
		}
		
		data_phase_addr = NAND_BASE | (1 << 21) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		poob_data += oob_size - ONFI_AXI_DATA_WIDTH;
		temp_buffer = (uint32_t *)poob_data;
		temp_length = ONFI_AXI_DATA_WIDTH >> 2;
		for (index = 0; index < temp_length; ++index) {
			temp_buffer[index] = SMC_ReadReg(data_phase_addr);
		}
	}

	return ERROR_OK;
}
