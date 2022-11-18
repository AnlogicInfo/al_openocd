#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*Register offsets*/
#define ONFI_AXI_DATA_WIDTH			4			/* AXI bus width */
#define NAND_COMMAND_PHASE_FLAG	0x00000000		/* Command phase flag */
#define NAND_DATA_PHASE_FLAG	0x00080000		/* Data phase flag */

#define ONFI_CMD_PROGRAM_PAGE1						0x80	/* ONFI Read ID Start command */
#define ONFI_CMD_PROGRAM_PAGE2						0x10	/* ONFI Read ID End command */
#define ONFI_CMD_PROGRAM_PAGE_CYCLES				5		/* ONFI Read ID command total address cycles*/
#define ONFI_CMD_PROGRAM_PAGE_END_TIMING 			ONFI_ENDIN_DATA_PHASE/* READ_ID End Cmd Invalid */

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

static uint32_t __attribute__((aligned(4))) NandOob64[12] = {52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
static uint32_t __attribute__((aligned(4))) NandOob32[6] = {26, 27, 28, 29, 30, 31};
static uint32_t __attribute__((aligned(4))) NandOob16[3] = {13, 14, 15};		/* data size 512bytes */
static uint8_t oob_data[512] = {0xff, 0xff};
static uint8_t ecc_data[12] = {0};

int flash_smc35x(uint32_t ctrl_base, uint32_t page_size, uint8_t *buffer, uint32_t offset, uint32_t count, uint32_t nand_base, uint32_t ecc_num)
{
	int retval = ERROR_OK;
	uint8_t state, nums = 0;
    uint32_t index, status;
	
    uint32_t oob_size = count - page_size;
	uint32_t *temp_buffer, temp_length = 0;

	uint32_t eccDataNums = 0, *dataOffsetPtr = NULL;
	uint8_t *pecc_data = ecc_data, *poob_data = oob_data;
	volatile uint8_t ecc_reg = 0;
	uint32_t ecc_value = 0;

	volatile unsigned long status_addr = 0;
	volatile unsigned long cmd_phase_addr = 0;
	volatile unsigned long data_phase_addr = 0;
	uint32_t cmd_phase_data;


	/* Check Nand Status */
	cmd_phase_addr = (nand_base | (ONFI_CMD_READ_STATUS1 << 3));
	cmd_phase_data = ONFI_COLUMN_NOT_VALID;
	SMC_WriteReg(cmd_phase_addr, cmd_phase_data);

	data_phase_addr = (nand_base | NAND_DATA_PHASE_FLAG);
	state = SMC_Read8BitReg(data_phase_addr);
	if (!(state & ONFI_STATUS_WP)) {
		return FAILED_FLAG;
	}


	/* Write Page Data */
	cmd_phase_addr = (nand_base | (ONFI_CMD_PROGRAM_PAGE_CYCLES << 21) | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (ONFI_CMD_PROGRAM_PAGE1 << 3));
	cmd_phase_data = 0 | (offset << (2*8));
	SMC_WriteReg(cmd_phase_addr, cmd_phase_data);
	cmd_phase_data = offset >> (32 - (2*8));
	SMC_WriteReg(cmd_phase_addr, cmd_phase_data);

	data_phase_addr = (nand_base | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11));
	temp_buffer = (uint32_t *)buffer;
	temp_length = (page_size - ONFI_AXI_DATA_WIDTH) >> 2;
	for (index = 0; index < temp_length; ++index)
	{
		SMC_WriteReg(data_phase_addr, temp_buffer[index]);
	}

	data_phase_addr = (nand_base | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (1 << 10));
	buffer += (page_size - ONFI_AXI_DATA_WIDTH);
	temp_buffer = (uint32_t *)buffer;
	temp_length = ONFI_AXI_DATA_WIDTH >> 2;
	for (index = 0; index < temp_length; ++index)
	{
		SMC_WriteReg(data_phase_addr, temp_buffer[index]);
	}


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
		case(224):
			eccDataNums = 12;
			nums = 4;
			dataOffsetPtr = NandOob64;
			break;
		default:
			/* Page size 256 bytes & 4096 bytes not supported by ECC block */
			break;
	}

	if (ecc_num == 1 && dataOffsetPtr != NULL) {
		/* Check busy signal if it is busy to poll*/
		while (SMC_Read8BitReg(ctrl_base + SMC_REG_ECC1_STATUS) & (1 << SMC_EccStatus_EccStatus_FIELD));

		for (ecc_reg = 0; ecc_reg < nums; ++ecc_reg)
		{
			ecc_value = SMC_ReadReg(ctrl_base + SMC_REG_ECC1_BLOCK0 + ecc_reg*4);
			
			if ((ecc_value) & (1 << SMC_EccBlock_ISCheakValueValid_FIELD))
			{
				for (index = 0; index < 3; ++index)
				{
					*pecc_data = ecc_value & 0xFF;
					ecc_value = ecc_value >> 8;
					++pecc_data;
				}
			}
			else {
				retval = 16;
				break;
			}
		}

		for (index = 0; index < eccDataNums; index++)
		{
			oob_data[dataOffsetPtr[index]] = (~ecc_data[index]);
		}
	}


	/* Write Oob Data */
	data_phase_addr = (nand_base | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11));
	temp_buffer = (uint32_t *)poob_data;
	temp_length = (oob_size - ONFI_AXI_DATA_WIDTH) >> 2;
	for (index = 0; index < temp_length; ++index)
	{
		SMC_WriteReg(data_phase_addr, temp_buffer[index]);
	}

	data_phase_addr =(nand_base | (1 << 21) | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11));
	poob_data += oob_size - ONFI_AXI_DATA_WIDTH;
	temp_buffer = (uint32_t *)poob_data;
	temp_length = ONFI_AXI_DATA_WIDTH >> 2;
	for (index = 0; index < temp_length; ++index)
	{
		SMC_WriteReg(data_phase_addr, temp_buffer[index]);
	}


	/* Check Controller Status */
	while (!(SMC_ReadReg(ctrl_base + SMC_REG_MEMC_STATUS) & (1 << SMC_MemcStatus_SmcInt1RawStatus_FIELD)));


	/* Clear SMC Interrupt 1, as an alternative to an AXI read */
    status_addr = (ctrl_base + SMC_REG_MEM_CFG_CLR);
	status = SMC_ReadReg(ctrl_base + SMC_REG_MEM_CFG_CLR);
	SMC_WriteReg(status_addr, (status | SMC_MemCfgClr_ClrSmcInt1));


	/* Check Nand Status */
	cmd_phase_addr = (nand_base | (ONFI_CMD_READ_STATUS1 << 3));
	cmd_phase_data = ONFI_COLUMN_NOT_VALID;
	SMC_WriteReg(cmd_phase_addr, cmd_phase_data);

	data_phase_addr = (nand_base | NAND_DATA_PHASE_FLAG);
	state = SMC_Read8BitReg(data_phase_addr);
	if (state & ONFI_STATUS_FAIL) {
		return FAILED_FLAG;
	}

	return retval;
}