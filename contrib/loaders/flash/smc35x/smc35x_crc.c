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

static const unsigned int crc32_table[] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
	0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
	0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
	0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
	0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
	0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
	0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
	0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
	0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
	0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
	0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
	0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
	0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
	0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
	0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
	0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
	0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
	0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
	0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
	0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
	0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
	0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
	0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
	0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
	0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
	0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
	0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
	0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
	0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
	0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
	0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
	0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
	0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
	0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
	0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
	0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
	0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
	0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
	0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
	0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
	0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
	0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
	0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

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
		return SmcTwoBitsErr;

	/* Multiple bits error */
	return SmcMultipleBitsErr;
}

static int read_sync(bool raw_oob, uint32_t page_size, uint8_t *buffer, uint32_t offset, uint32_t oob_size, uint32_t device_id, uint32_t ecc_num)
{
	uint8_t state, eccDataNums = 0, nums = 0, eccOffset = 0;
    uint32_t status;
	volatile uint32_t index = 0;

	volatile uint32_t *temp_buffer;
	uint32_t temp_length = 0;
	uint32_t *dataOffsetPtr = NULL;
	uint8_t *page_data = buffer, *poob_data = oob_data;
	
	volatile uint8_t *ecc_data = buffer + page_size;
	if (raw_oob) {
		ecc_data = buffer + (page_size + oob_size);
		poob_data = buffer + page_size;
	}
	volatile uint8_t *read_ecc_data = ecc_data + 12;

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
		if (!raw_oob) {
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

			state = smc35x_ecc_calculate(ecc_data, nums);
			if (state != SmcSuccess)
				return state;
		}

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
		
		if (!raw_oob) {
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

uint32_t flash_smc35x(bool raw_oob, uint32_t block_size, uint32_t count, uint8_t *buffer, uint32_t offset, uint32_t oob_size, uint32_t device_id, uint32_t ecc_num)
{
	uint32_t crc = 0xffffffff;
    int index = 0, cur_count = 0, crc_count = 0;
    uint32_t page_size = (raw_oob) ? (block_size - oob_size) : block_size;
	volatile uint8_t *data_buffer = buffer;

    while(count > 0)
    {
		if(count < block_size)
			cur_count = count;
        else
			cur_count = block_size;

        crc_count = cur_count;
		index = 0;

        read_sync(raw_oob, page_size, buffer, offset, oob_size, device_id, ecc_num);

        while(crc_count--) {
            crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ data_buffer[index]) & 255];
            ++index;
        }
		
        ++offset;
        count -= cur_count;
    }

    return crc;
}
