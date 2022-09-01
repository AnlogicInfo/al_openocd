#ifndef ONFI_H
#define ONFI_H

#include "smc35x.h"

#define SMC_MAX_TARGETS			1		/* Max number of targets supported */
#define SMC_MAX_BLOCKS			32768	/* Max number of Blocks */
#define SMC_MAX_PAGE_SIZE		16384	/* Max page size of NAND flash */
#define SMC_MAX_SPARE_SIZE		512		/* Max spare bytes of a NAND flash page */

#define NAND_ECC_BLOCK_SIZE		512		/* ECC block size */
#define NAND_ECC_BYTES			3		/* ECC bytes per ECC block */

#define NAND_PAGE_SIZE_512		512		/* Page size 512 */
#define NAND_PAGE_SIZE_1024		1024	/* Page size 1024 */
#define NAND_PAGE_SIZE_2048		2048	/* Page size 2048 */
#define NAND_PAGE_SIZE_4096		4096	/* Page size 4096 */
#define NAND_PAGE_SIZE_8192		8192	/* Page size 8192 */

#define NAND_SPARE_SIZE_8		8		/* Spare bytes size 8 */
#define NAND_SPARE_SIZE_16		16		/* Spare bytes size 16 */
#define NAND_SPARE_SIZE_32		32		/* Spare bytes size 32 */
#define NAND_SPARE_SIZE_64		64		/* Spare bytes size 64 */
#define NAND_SPARE_SIZE_128		128		/* Spare bytes size 128 */
#define NAND_SPARE_SIZE_256		256		/* Spare bytes size 256 */

#define NAND_FLASH_WIDTH_8		8		/* NAND Flash width 8-bit */
#define NAND_FLASH_WIDTH_16		16		/* NAND Flash width 16-bit */

/*
enum OnfiCmdArrayOffset{
	READ 				= 0,
	COPY_BACKREAD 		= 1,
	CHANGE_READ_COLUMN 	= 2,
	READ_CACHE_ENHANCED = 3,
	READ_CACHE 			= 4,
	READ_CACHEEND 		= 5,
	BLOCK_ERASE 		= 6,
	INTERLEAVED 		= 7,
	READ_STATUS 		= 8,
	READ_STATUS_ENHANCED= 9,
	PAGE_PROGRAM 		= 10,
	INTERLEAVED 		= 11,
	PAGE_CACHE_PROGRAM 	= 12,
	COPY_BACK_PROGRAM 	= 13,
	INTERLEAVED 		= 14,
	CHANGE_WRITE_COLUMN = 15,
	READ_ID 			= 16,
	READ_PARAMETER_PAGE = 17,
	READ_UNIQUE_ID 		= 18,
	GET_FEATURES		= 19,
	SET_FEATURES 		= 20,
	RESET 				= 21
};
*/

/* Vendor Specific Commands */
#define ONFI_AXI_DATA_WIDTH			4		/* AXI bus width */

/* Onfi Commands */
#define ONFI_END_CMD_NONE			0		/* No End command */

#define ONFI_END_CMD_INVALID		0		/* End command invalid */
#define ONFI_ENDIN_CMD_PHASE				1		/* End command in command phase */
#define ONFI_ENDIN_DATA_PHASE				2		/* End command in data phase */

#define ONFI_PAGE_NOT_VALID		-1		/* Page is not valid in command phase */
#define ONFI_COLUMN_NOT_VALID	-1		/* Column is not valid in command phase */


/*  command  */
#define ONFI_CMD_COPY_BACKREAD1						0x00	/* ONFI Read command Start */
#define ONFI_CMD_COPY_BACKREAD2						0x35	/* ONFI Read command End */
#define ONFI_CMD_COPY_BACKREAD_CYCLES				5		/* ONFI Read command total address cycles*/
#define ONFI_CMD_COPY_BACKREAD_END_TIMING			ONFI_ENDIN_CMD_PHASE /* COPY_BACKREAD End Cmd in command phase */



#if 0

#define ONFI_CMD_CHANGE_READ_COLUMN1				0x05	/* ONFI Change Read Column command Start */
#define ONFI_CMD_CHANGE_READ_COLUMN2				0xE0	/* ONFI Change Read Column command End */
#define ONFI_CMD_CHANGE_READ_CYCLES					2		/* ONFI Change Raed command total address cycles*/
#define ONFI_CMD_COPY_BACKREAD_END_TIMING			ONFI_ENDIN_CMD_PHASE /* COPY_BACKREAD End Cmd in command phase */

#define ONFI_CMD_READ_CACHE_ENHANCED1				0x00	/* ONFI Read Cache Enhanced command Start */
#define ONFI_CMD_READ_CACHE_ENHANCED2				0x31	/* ONFI Read Cache Enhanced command End */
#define ONFI_CMD_READ_CACHE_ENHANCED_CYCLES			5		/* ONFI Read Cache Enhanced command total address cycles*/
#define ONFI_CMD_READ_CACHE_ENHANCED_END_TIMING		ONFI_ENDIN_CMD_PHASE /* READ_CACHE_ENHANCED End Cmd in command phase */

#endif


#define ONFI_CMD_CHANGE_READ_COLUMN1				0x05	/* ONFI Change Read Column command Start */
#define ONFI_CMD_CHANGE_READ_COLUMN2				0xE0	/* ONFI Change Read Column command End */
#define ONFI_CMD_CHANGE_READ_CYCLES					2		/* ONFI Change Raed command total address cycles*/
#define ONFI_CMD_COPY_BACKREAD_END_TIMING			ONFI_ENDIN_CMD_PHASE /* COPY_BACKREAD End Cmd in command phase */

#define ONFI_CMD_READ_CACHE_ENHANCED1				0x00	/* ONFI Read Cache Enhanced command Start */
#define ONFI_CMD_READ_CACHE_ENHANCED2				0x31	/* ONFI Read Cache Enhanced command End */
#define ONFI_CMD_READ_CACHE_ENHANCED_CYCLES			5		/* ONFI Read Cache Enhanced command total address cycles*/
#define ONFI_CMD_READ_CACHE_ENHANCED_END_TIMING		ONFI_ENDIN_CMD_PHASE /* READ_CACHE_ENHANCED End Cmd in command phase */

#define ONFI_CMD_PROGRAM_CACHE1						0X80	/* ONFI Read Cache command Start */
#define ONFI_CMD_PROGRAM_CACHE2						0X15	/* ONFI Read Cache command End */
#define ONFI_CMD_PROGRAM_CACHE_CYCLES				5		/* ONFI Read Cache command total address cycles*/
#define ONFI_CMD_PROGRAM_CACHE_END_TIMING			ONFI_ENDIN_DATA_PHASE /* READ_CACHE End Cmd Invalid */

#define ONFI_CMD_READ_CACHE_END1					0x3F	/* ONFI Read Cache command Start */
#define ONFI_CMD_READ_CACHE_END2					ONFI_END_CMD_NONE	/* ONFI Read Cache command End */
#define ONFI_CMD_READ_CACHE_END_CYCLES				0		/* ONFI Read Cache command total address cycles*/
#define ONFI_CMD_READ_CACHE_END_TIMING				ONFI_END_CMD_INVALID /* READ_CACHE End Cmd Invalid */


#define ONFI_CMD_READ_PARAMETER1					0xEC	/* ONFI Read ID command Start */
#define ONFI_CMD_READ_PARAMETER2					ONFI_END_CMD_NONE	/* ONFI Read ID command End */
#define ONFI_CMD_READ_PARAMETER_CYCLES				1		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_READ_PARAMETER_END_TIMING			ONFI_END_CMD_INVALID /* READ_ID End Cmd Invalid */

#define ONFI_CMD_SET_FEATURES1						0xEF	/* ONFI Read ID command Start */
#define ONFI_CMD_SET_FEATURES2						ONFI_END_CMD_NONE	/* ONFI Read ID command End */
#define ONFI_CMD_SET_FEATURES_CYCLES				0		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_SET_FEATURES_END_TIMING			ONFI_END_CMD_INVALID /* READ_ID End Cmd Invalid */

#define ONFI_CMD_GET_FEATURES1						0xEE	/* ONFI Read ID command Start */
#define ONFI_CMD_GET_FEATURES2						ONFI_END_CMD_NONE	/* ONFI Read ID command End */
#define ONFI_CMD_GET_FEATURES_CYCLES				0		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_GET_FEATURES_END_TIMING			ONFI_END_CMD_INVALID /* READ_ID End Cmd Invalid */

#define ONFI_CMD_READ_PAGE1							0x00	/* ONFI Read ID command Start */
#define ONFI_CMD_READ_PAGE2							0x30	/* ONFI Read ID command End */
#define ONFI_CMD_READ_PAGE_CYCLES					5		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_READ_PAGE_END_TIMING				ONFI_ENDIN_CMD_PHASE /* READ_ID End Cmd Invalid */

#define ONFI_CMD_PROGRAM_PAGE1						0x80	/* ONFI Read ID Start command */
#define ONFI_CMD_PROGRAM_PAGE2						0x10	/* ONFI Read ID End command */
#define ONFI_CMD_PROGRAM_PAGE_CYCLES				5		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_PROGRAM_PAGE_END_TIMING 			ONFI_ENDIN_DATA_PHASE/* READ_ID End Cmd Invalid */

#define ONFI_CMD_ERASE_BLOCK1						0x60	/* ONFI Read ID command Start */
#define ONFI_CMD_ERASE_BLOCK2						0xD0	/* ONFI Read ID command End */
#define ONFI_CMD_ERASE_BLOCK_CYCLES					3		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_ERASE_BLOCK_END_TIMING				ONFI_ENDIN_CMD_PHASE /* READ_ID End Cmd Invalid */

#define ONFI_CMD_RANDOM_DATA_READ1					0x05	/* ONFI Read ID command Start */
#define ONFI_CMD_RANDOM_DATA_READ2					0xE0	/* ONFI Read ID command End */
#define ONFI_CMD_RANDOM_DATA_READ_CYCLES			2		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_RANDOM_DATA_READ_END_TIMING		ONFI_ENDIN_CMD_PHASE /* READ_ID End Cmd Invalid */

#define ONFI_CMD_RANDOM_DATA_INPUT1					0x85	/* ONFI Read ID command Start */
#define ONFI_CMD_RANDOM_DATA_INPUT2					ONFI_END_CMD_NONE	/* ONFI Read ID command End */
#define ONFI_CMD_RANDOM_DATA_INPUT_CYCLES			2		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_RANDOM_DATA_INPUT_END_TIMING		ONFI_END_CMD_INVALID /* READ_ID End Cmd Invalid */

#define ONFI_CMD_RESET1								0xFF	/* ONFI Read ID command Start */
#define ONFI_CMD_RESET2								ONFI_END_CMD_NONE	/* ONFI Read ID command End */
#define ONFI_CMD_RESET_CYCLES						0		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_RESET_END_TIMING					ONFI_END_CMD_INVALID /* READ_ID End Cmd Invalid */

#define ONFI_CMD_READ_ID1							0x90	/* ONFI Read ID command Start */
#define ONFI_CMD_READ_ID2							ONFI_END_CMD_NONE	/* ONFI Read ID command End */
#define ONFI_CMD_READ_ID_CYCLES						1		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_READ_ID_END_TIMING					ONFI_END_CMD_INVALID /* READ_ID End Cmd Invalid */

#define ONFI_CMD_READ_STATUS1						0x70	/* ONFI Read Status command Start */
#define ONFI_CMD_READ_STATUS2						ONFI_END_CMD_NONE	/* ONFI Read Status command End */
#define ONFI_CMD_READ_STATUS_CYCLES					0		/* ONFI Read Status command total address cycles*/
#define ONFI_CMD_READ_STATUS_END_TIMING				ONFI_END_CMD_INVALID /* READ_STATUS End Cmd Invalid */


/* ONFI Status Register Mask */
#define ONFI_STATUS_FAIL			0x01	/* Status Register : FAIL */
#define ONFI_STATUS_FAILC			0x02	/* Status Register : FAILC */
#define ONFI_STATUS_ARDY			0x20	/* Status Register : ARDY */
#define ONFI_STATUS_RDY				0x40	/* Status Register : RDY */
#define ONFI_STATUS_WP				0x80	/* Status Register : WR */



#define NAND_COMMAND_PHASE_FLAG	0x00000000		/* Command phase flag */
#define NAND_DATA_PHASE_FLAG	0x00080000		/* Data phase flag */

#define NAND_PAGE_NOT_VALID		-1	/* Page is not valid in command phase */
#define NAND_COLUMN_NOT_VALID	-1	/* Column is not valid in command phase */

#define NANDP_BB_FLAG			0xFF	/* Bad block flag */

#define FAILED_FLAG			0	/* return failed flag */
#define SUCCESS_FLAG		1	/* return success flag */
#define NAND_WRITE_PROTECTED 2	/* return BlockErase fail because nand write protected */

#define GOOD_BLOCK 1	/* return BlockErase fail because nand write protected */
#define BAD_BLOCK  2	/* return BlockErase fail because nand write protected */

#define NO_CLEAR_CS			0	/* return failed flag */
#define CLEAR_CS			1	/* return success flag */

#define NO_ECC_LAST			0	/* return failed flag */
#define ECC_LAST			1	/* return success flag */


#if 0
#define SMC_WriteReg	__SW		/* Write 32bit value to address (32 bit. Address First and Value Second */
#define SMC_ReadReg		__LW		/* Read 8bit value from address (32 bit. Address First )*/
#endif



#define SMC_WriteReg(reg,value)	\
do{		\
	*(volatile uint32_t *)reg = value;	\
}while(0)


#define SMC_ReadReg(reg)	 (*(volatile uint32_t *)reg)



 /**
  * OneHot is used to check if one and only one bit is set.
  * This Macro returns 1 if the value passed is OneHot.
  */
 #define OneHot(Value)	(!((Value) & (Value - 1)))



typedef struct{
	volatile uint32_t dataBytesPerPage;		/* per page contains data byte numbers*/
	volatile uint16_t spareBytesPerPage;		/* per page contains spare byte numbers*/
	volatile uint32_t pagesPerBlock;			/* per block contains page  numbers*/
	volatile uint32_t blocksPerUint;			/* per unit contains block numbers*/
	volatile uint8_t totalUint;				/* total unit numbers*/
}Nand_Size_TypeDef;

void SmcSendCommand(uint8_t startCmd, uint8_t endCmd, uint8_t addrCycles, uint8_t endCmdPhase, int Page, int Column);
void SmcReadData(uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length);
void SmcReadData_re(struct nand_device *nand, uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length);
void SmcReadBuf(uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length, uint32_t clearCs, uint32_t eccLast);
void SmcWriteBuf(uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length, uint32_t clearCs, uint32_t eccLast);


void Onfi_CmdReadId(uint8_t Address, uint8_t *Id, uint8_t idSize);
void Onfi_CmdReset(void);
uint8_t Onfi_CmdReadStatus(void);
uint8_t Onfi_CmdReadParameter(Nand_Size_TypeDef *nandSize);
uint8_t Onfi_CmdReadPage(uint32_t Page, uint32_t Column);
uint8_t Onfi_CmdProgramPage(uint32_t Page, uint32_t Column);
uint8_t Onfi_CmdBlockErase(uint32_t Page);
/*
uint8_t Onfi_CmdProgramCachePage(uint32_t Page, uint32_t Column);
uint8_t Onfi_CmdReadCachePage(uint32_t Page, uint32_t Column);
*/
uint8_t Onfi_CmdProgramCachePage(uint32_t Page, uint32_t Column);
uint8_t Onfi_CmdReadCachePage(uint32_t Page, uint32_t Column);
uint8_t Onfi_CmdReadCachePageEnd(uint32_t Page, uint32_t Column);

void Onfi_CmdSetFeature(uint8_t Address, uint8_t *Value);
void Onfi_CmdGetFeature(uint8_t Address, uint8_t *Value);

uint8_t Nand_ReadSpareBytes(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize);
uint8_t Nand_ReadPage(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize);
uint8_t Nand_ProgramPage(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize);
uint8_t Nand_IsBusy(void);
uint8_t Nand_CheakIsBadBlock(uint32_t Page, uint32_t Column, uint8_t *Buf,Nand_Size_TypeDef *nandSize);


uint8_t Nand_EnableOnDieEcc(void);
uint8_t Nand_EccHwInit(Nand_Size_TypeDef *nandSize);
void Nand_EccHwDisable(void);
uint8_t Nand_HwCalculateEcc(uint8_t *Dst);
uint8_t Nand_HwCorrectEcc(uint8_t *eccCode, uint8_t *eccCalc, uint8_t *buf);

uint8_t Nand_ProgramPage_HwEcc(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize);
uint8_t Nand_ReadPage_HwEcc(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize);


#endif
