#include "onfi.h"
#include "imp.h"
#include "smc35x.h"
#include <target/target.h>

typedef struct{
	uint8_t ecc_read:5;
	uint8_t ecc_can_correct:5;
	uint8_t ecc_fail:5;
	uint8_t ecc_value_valid:5;
	uint8_t ecc_rd_n_wr:1;
	uint8_t ecc_last_status:2;
	uint8_t ecc_status:1;
	uint8_t ecc_int_status:6;
}test;

#define TEST ((test*)(SMC_BASE+SMC_REG_ECC1_STATUS))

uint8_t buf[2048];

typedef struct{
	uint32_t dataBytesPerPage;		/* per page contains data byte numbers*/
	uint16_t spareBytesPerPage;		/* per page contains spare byte numbers*/
	uint32_t pagesPerBlock;			/* per block contains page  numbers*/
	uint32_t blocksPerUnit;			/* per unit contains block numbers*/
	uint8_t totalUnit;				/* total unit numbers*/
	uint8_t eccNum;
	uint8_t device_id[5];
}nand_size_type;

struct smc35x_nand_controller{
	// uint32_t ecc;
	// uint32_t nand_base;
	// uint32_t smc_base;

	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	nand_size_type nand_size;
};

static uint32_t __attribute__((aligned(4))) NandOob64[12] = {52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
static uint32_t __attribute__((aligned(4))) NandOob32[6] = {26, 27, 28, 29, 30, 31};
static uint32_t __attribute__((aligned(4))) NandOob16[3] = {13, 14, 15};		/* data size 512bytes */

NAND_DEVICE_COMMAND_HANDLER(smc35x_nand_device_command)
{
	// unsigned long chip = 0, smc = 0, ecc = 0;
	struct smc35x_nand_controller* smc35x_info;

	// if (CMD_ARGC < 3 || CMD_ARGC > 4) {
	// 	LOG_ERROR("parameters: %s target chip_addr", CMD_ARGV[0]);
	// 	return ERROR_NAND_OPERATION_FAILED;
	// }

	// COMMAND_PARSE_NUMBER(ulong, CMD_ARGV[2], chip);
	// if (chip == 0) {
	// 	LOG_ERROR("invalid NAND chip address: %s", CMD_ARGV[2]);
	// 	return ERROR_NAND_OPERATION_FAILED;
	// }

	smc35x_info = calloc(1, sizeof(struct smc35x_nand_controller));
	if (!smc35x_info) {
		LOG_ERROR("no memory for nand controller");
		return ERROR_NAND_DEVICE_INVALID;
	}

	/* fill in the address fields for the core device */
	nand->controller_priv = smc35x_info;
	smc35x_info->cmd_phase_addr = 0;
	smc35x_info->cmd_phase_data = 0;
	smc35x_info->data_phase_addr = 0;

	return ERROR_OK;
}

int smc35x_command(struct nand_device* nand, uint8_t command)
{
	LOG_INFO("smc send cmd %x", command);

	struct target* target = nand->target;
	struct smc35x_nand_controller* smc35x_info = nand->controller_priv;
	
	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	switch (command)
	{
	case NAND_CMD_RESET:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_RESET1 << 3);
		smc35x_info->cmd_phase_data = ONFI_COLUMN_NOT_VALID;
		break;

	case NAND_CMD_READID:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_READ_ID_CYCLES << 21) | (ONFI_CMD_READ_ID1 << 3);
		smc35x_info->cmd_phase_data = 0x00;
		smc35x_info->data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG;
		break;

	case NAND_CMD_STATUS:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_READ_STATUS1 << 3);
		smc35x_info->cmd_phase_data = ONFI_COLUMN_NOT_VALID;
		target_write_u32(target, smc35x_info->cmd_phase_addr, smc35x_info->cmd_phase_data);
		smc35x_info->data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG;
		break;

	case NAND_CMD_ERASE1:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_ERASE_BLOCK_CYCLES << 21) |
		(ONFI_CMD_ERASE_BLOCK_END_TIMING << 20) | (ONFI_CMD_ERASE_BLOCK2 << 11) | (ONFI_CMD_ERASE_BLOCK1 << 3);
		break;
	
	case NAND_CMD_SEQIN:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_PROGRAM_PAGE_CYCLES << 21) | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (ONFI_CMD_PROGRAM_PAGE1 << 3);
		smc35x_info->data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
		break;

	case NAND_CMD_READ0:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_READ_PAGE_CYCLES << 21) | (ONFI_CMD_READ_PAGE_END_TIMING << 20) | (ONFI_CMD_READ_PAGE2 << 11) | (ONFI_CMD_READ_PAGE1 << 3);
		smc35x_info->data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		break;
	
	case NAND_CMD_READOOB:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_READ_PAGE_CYCLES << 21) | (ONFI_CMD_READ_PAGE_END_TIMING << 20) | (ONFI_CMD_READ_PAGE2 << 11) | (ONFI_CMD_READ_PAGE1 << 3);
		smc35x_info->data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		break;
		
	case NAND_CMD_READSTART:
		smc35x_info->cmd_phase_addr = NAND_BASE;
		smc35x_info->cmd_phase_data = 0x00;
		target_write_u32(target, smc35x_info->cmd_phase_addr, smc35x_info->cmd_phase_data);
		break;
	
	case ONFI_CMD_READ_PARAMETER1:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_READ_PARAMETER_CYCLES << 21) | (ONFI_CMD_READ_PARAMETER1 << 3);
		smc35x_info->cmd_phase_data = 0x00;
		target_write_u32(target, smc35x_info->cmd_phase_addr, smc35x_info->cmd_phase_data);
		smc35x_info->data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG;
		break;

	case ONFI_CMD_GET_FEATURES1:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_GET_FEATURES1 << 3);
		smc35x_info->cmd_phase_data = 0x90;
		target_write_u32(target, smc35x_info->cmd_phase_addr, smc35x_info->cmd_phase_data);
		smc35x_info->data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG;
		break;
	
	case ONFI_CMD_SET_FEATURES1:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_SET_FEATURES1 << 3);
		smc35x_info->cmd_phase_data = 0x90;
		target_write_u32(target, smc35x_info->cmd_phase_addr, smc35x_info->cmd_phase_data);
		smc35x_info->data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG;
		break;

	default: 
		LOG_INFO("no action");
		return ERROR_OK;
	}

	return ERROR_OK;
}

int smc35x_read_data(struct nand_device *nand, void *data)
{
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

    target_read_u8(target, smc35x_info->data_phase_addr, data);

	return ERROR_OK;
}

int smc35x_write_data(struct nand_device *nand, uint16_t data)
{
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

    target_write_u8(target, smc35x_info->data_phase_addr, data);

	return ERROR_OK;
}

int smc35x_address(struct nand_device *nand, uint8_t address)
{
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use S3C24XX NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	target_write_u8(target, smc35x_info->cmd_phase_addr, address);

	return ERROR_OK;
}

int smc35x_reset(struct nand_device *nand)
{
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use S3C24XX NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	target_write_u32(target, smc35x_info->cmd_phase_addr, -1);

	return ERROR_OK;
}

int smc35x_read_parameter(struct nand_device *nand, nand_size_type *nand_size)
{
	uint32_t index;
	uint8_t temp[256];
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	smc35x_command(nand, NAND_CMD_RESET);
	smc35x_reset(nand);

	smc35x_command(nand, ONFI_CMD_READ_PARAMETER1);
	target_write_u32(target, NAND_BASE, 0x00);

	for (index = 0; index < 256; index++)
	{
		target_read_u8(target, smc35x_info->data_phase_addr, &temp[index]);
	}
	nand_size->dataBytesPerPage = 	*((uint32_t *)(&temp[DATA_PER_PAGE_POS]));
	nand_size->spareBytesPerPage = 	*((uint16_t *)(&temp[SPARE_PER_PAGE_POS]));
	nand_size->pagesPerBlock = 		*((uint32_t *)(&temp[PAGE_PER_BLOCK_POS]));
	nand_size->blocksPerUnit =		*((uint32_t *)(&temp[BLOCKS_PER_UINT_POS]));
	nand_size->totalUnit =			temp[TOTAL_UINT_POS];
	nand_size->eccNum =				temp[ECC_NUM_POS];

	return ERROR_OK;
}


int smc35x_ecc1_init(struct nand_device *nand, nand_size_type *nand_size)
{
	uint32_t ecc1Config, status;
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	switch(nand_size->dataBytesPerPage)
	{
		case(512):
			ecc1Config = SMC_EccCfg_One_PageSize;
			break;
		case(1024):
			ecc1Config = SMC_EccCfg_Two_PageSize;
			break;
		case(2048):
			ecc1Config = SMC_EccCfg_Four_PageSize;
			break;
		default:
			/* Page size 256 bytes & 4096 bytes not supported by ECC block */
			return ERROR_OK;
	}

	/* Restore the ECC mem command1 and ECC mem command2 register if the previous command is read page cache */
	/* Set SMC_REG_ECC1_MEMCMD0 Reg*/
	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_MEMCMD0),
			(SMC_EccMemCmd0_InitWriteCmd | SMC_EccMemCmd0_InitReadCmd | SMC_EccMemCmd0_EndReadCmd | SMC_EccMemCmd0_UseEndCmd));
	/* Set SMC_REG_ECC1_MEMCMD1 Reg*/
	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_MEMCMD1),
			(SMC_EccMemCmd1_ColChangeWRCmd | SMC_EccMemCmd1_ColChangeRDCmd | SMC_EccMemCmd1_ColChangeEndCmd | SMC_EccMemCmd1_UseEndCmd));
	
	do {
		status = target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_STATUS), &status);
	} while (status & SMC_EccStatus_EccBusy);

	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_CFG),
			(SMC_EccCfg_EccModeAPB| SMC_EccCfg_EccValue_InPageEnd| SMC_EccCfg_EccNoJump| ecc1Config));

	return ERROR_OK;
}

uint32_t smc35x_internalecc_init(struct nand_device *nand)
{
	uint8_t index, temp;
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	nand_size_type *nand_size = &smc35x_info->nand_size;
	uint8_t __attribute__((aligned(4))) ecc_set[4] = {0x08, 0x00, 0x00, 0x00};
	uint8_t __attribute__((aligned(4))) ecc_get[4] = {0};

	if (nand_size->device_id[0] == NAND_MFR_AMD) {
		ecc_set[0] = 0x10;
		temp = 0x10;
	} else {
		temp = 0x08;
	}

	smc35x_command(nand, ONFI_CMD_SET_FEATURES1);
	for (index = 0; index < 4; ++index) {
		target_write_u32(target, smc35x_info->data_phase_addr, ecc_set[index]);
	}

	smc35x_command(nand, ONFI_CMD_GET_FEATURES1);
	target_write_u8(target, NAND_BASE, 0x00);
	for (index = 0; index < 4; ++index) {
		target_read_u8(target, smc35x_info->data_phase_addr, &ecc_get[index]);
	}

	if (ecc_get[0] == temp) {
		return SmcSuccess;
	} else {	
		return SmcWriteEccFeatErr;
	}
}

uint32_t smx35x_ecc1_disable(struct nand_device *nand)
{
	uint32_t status, config;
	struct target *target = nand->target;

	do {
		status = target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_STATUS), &status);
	} while (status & SMC_EccStatus_EccBusy);

	config = target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_CFG), &config);
	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_CFG), config & (~(3 << SMC_EccCfg_EccMode_FIELD)));
	
	return ERROR_OK;
}

uint32_t smc35x_nand_init(struct nand_device *nand)
{
	uint32_t status, extid;
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	nand_size_type *nand_size = &smc35x_info->nand_size;
	
	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	smc35x_command(nand, NAND_CMD_RESET);
	smc35x_reset(nand);
	smc35x_command(nand, NAND_CMD_READID);
	smc35x_address(nand, 0x00);

	smc35x_read_data(nand, &nand_size->device_id[0]);
	smc35x_read_data(nand, &nand_size->device_id[1]);
	smc35x_read_data(nand, &nand_size->device_id[2]);
	smc35x_read_data(nand, &nand_size->device_id[3]);
	smc35x_read_data(nand, &nand_size->device_id[4]);

	LOG_INFO("id: %x %x %x %x %x",
		nand_size->device_id[0],nand_size->device_id[1],nand_size->device_id[2],nand_size->device_id[3],nand_size->device_id[4]);

	if(nand_size->device_id[0] != NAND_MFR_SAMSUNG)
	{
		status = smc35x_read_parameter(nand, nand_size);

		if(status != SmcSuccess)
		{
			return status;
		}
	}
	else
	{
		/* from kernel */
		extid = nand_size->device_id[3];

		nand_size->dataBytesPerPage = 1024 << (extid & 0x03);
		extid >>= 2;
		// oob size
		nand_size->spareBytesPerPage = (8 << (extid & 0x01)) * (nand_size->dataBytesPerPage >> 9);

		extid >>= 2;
		nand_size->pagesPerBlock = ((64 * 1024) << (extid & 0x03)) /nand_size->dataBytesPerPage;

		/* myself */
		extid = nand_size->device_id[4] >> 4;

		nand_size->blocksPerUnit = ((8 * 1024 * 1024) << (extid & 0x07))/(nand_size->pagesPerBlock * nand_size->dataBytesPerPage);

		nand_size->totalUnit = 1;

		if((nand_size->dataBytesPerPage > SMC_MAX_PAGE_SIZE) ||
				(nand_size->spareBytesPerPage > SMC_MAX_SPARE_SIZE)||
				(nand_size->pagesPerBlock > SMC_MAX_PAGES_PER_BLOCK))
		{
			return SmcSamsungParamOver;
		}

		/* form myself */
		nand_size->eccNum = 4;
	}

	if((nand_size->device_id[0] == NAND_MFR_AMD) && (nand_size->eccNum == 0))
	{
		/* Because SkyHigh ecc nums is zero */
		nand_size->eccNum = 4;
	}

	if(nand_size->eccNum == 1)
	{
		/* Enable SMC IP Hardware */
		status = smc35x_ecc1_init(nand, nand_size);

		if(status != SmcSuccess)
		{
			return status;
		}
	}
	else
	{
		/* Close SMC IP ECC */
		status = smx35x_ecc1_disable(nand);

		if(status != SmcSuccess)
		{
			return status;
		}

		if((nand_size->device_id[0] == NAND_MFR_AMD)||
			(nand_size->device_id[0] == NAND_MFR_MICRON))
		{
			/* Enable On Die ECC */
			status = smc35x_internalecc_init(nand);

			if(status != SmcSuccess)
			{
				return status;
			}
		}
	}

	return SmcSuccess;
}

uint8_t *smc35x_oob_init(struct nand_device *nand, uint8_t *oob, uint32_t *size, uint32_t spare_size)
{
	if (!oob || *size == 0) {
		*size = spare_size;

		oob = malloc(*size);
		if (!oob) {
			LOG_ERROR("Unable to allocate space for OOB");
			return NULL;
		}

		memset(oob, 0xFF, *size);
	}

	return oob;
}

int smc35x_ecc_calculate(struct nand_device *nand, uint8_t *ecc_data, uint8_t ecc_data_nums)
{
	uint8_t count = 0, status = 0;
	volatile uint8_t ecc_reg = 0;
	uint32_t ecc_value = 0;
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	/* Check busy signal if it is busy to poll*/
	do {
		target_read_u8(target, (SMC_BASE + SMC_REG_ECC1_STATUS), &status);
		status &= (1 << SMC_EccStatus_EccStatus_FIELD);
	}
	while (status);
	for (ecc_reg = 0; ecc_reg < ecc_data_nums/3; ++ecc_reg)
	{
		target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_BLOCK0 + ecc_reg * 4), &ecc_value);
		LOG_INFO("ecc value: %x\r\n", ecc_value);
		if((ecc_value) & (1 << SMC_EccBlock_ISCheakValueValid_FIELD))
		{
			for(count=0; count < 3; ++count)
			{
				*ecc_data = ecc_value & 0xFF;
				LOG_INFO("dst: %d\r\n", *ecc_data);
				ecc_value = ecc_value >> 8;
				++ecc_data;
			}
		}
		else
		{
			LOG_INFO("EccInvalidErr\r\n");
			return SmcEccDataInvalidErr;
		}
	}

	return ERROR_OK;
}


int smc35x_read_page_internalecc(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	uint32_t index = 0;
	uint8_t *oob_data = oob, *oob_free;

	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	uint64_t data_phase_addr = smc35x_info->data_phase_addr;

	if(smc35x_info->nand_size.device_id[0] == NAND_MFR_MICRON)
	{
		target_write_u32(target, NAND_BASE, 0x00);
	}

	for (index = 0; index < data_size; ++index, ++data)
	{
		target_read_u8(target, data_phase_addr, data);
		LOG_INFO("page %d index %d read data %x", page, index, *data);
	}

	oob = smc35x_oob_init(nand, oob, &oob_size, smc35x_info->nand_size.spareBytesPerPage);
	oob_free = oob;
	for (index = 0; index < oob_size-ONFI_AXI_DATA_WIDTH; ++index, ++oob)
	{
		target_read_u8(target, data_phase_addr, oob);
		LOG_INFO("page %d index %d read oob %x", page, index, *oob);
	}
	data_phase_addr = NAND_BASE | (1 << 21) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
	for (index = 0; index < ONFI_AXI_DATA_WIDTH; ++index, ++oob)
	{
		target_read_u8(target, data_phase_addr, oob);
		LOG_INFO("page %d index %d read oob %x", page, index, *oob);
	}

	if (!oob_data) {
		free(oob_free);
	}

	return ERROR_OK;
}

int smc35x_read_page_ecc1(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	uint32_t index = 0;
	uint8_t *oob_data = oob, *oob_free;
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	uint64_t data_phase_addr = smc35x_info->data_phase_addr;

	uint32_t status = 0;
	uint8_t i = 0, eccDataNums = 0, EccOffset = 0;
	uint8_t __attribute__((aligned(4))) eccData[12] = {0};
	uint8_t __attribute__((aligned(4))) ReadEccData[12] = {0};
	uint32_t *dataOffsetPtr = NULL;
	uint8_t *TempBuf = oob;

	if (data == NULL) {
		if (oob_size >= ONFI_AXI_DATA_WIDTH) {
			data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
			for (index = 0; index < oob_size - ONFI_AXI_DATA_WIDTH; ++index, ++oob)
			{
				target_read_u8(target, data_phase_addr, oob);
				LOG_INFO("page %d index %d read oob %x", page, index, *oob);
			}
			data_phase_addr = NAND_BASE | (1 << 21) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
			for (index = 0; index < ONFI_AXI_DATA_WIDTH; ++index, ++oob)
			{
				target_read_u8(target, data_phase_addr, oob);
				LOG_INFO("page %d index %d read oob %x", page, index, *oob);
			}
		}

		return ERROR_OK;
	}

	if (data_size >= ONFI_AXI_DATA_WIDTH) {
		for (index = 0; index < data_size - ONFI_AXI_DATA_WIDTH; ++index, ++data)
		{
			target_read_u8(target, data_phase_addr, data);
			LOG_INFO("page %d index %d read data %x", page, index, *data);
		}
		data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11) | (ECC_LAST << 10);
		for (index = 0; index < ONFI_AXI_DATA_WIDTH; ++index, ++data)
		{
			target_read_u8(target, data_phase_addr, data);
			LOG_INFO("page %d index %d read data %x", page, index, *data);
		}
	}

	switch(smc35x_info->nand_size.spareBytesPerPage)
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
		default:
			/* Page size 256 bytes & 4096 bytes not supported by ECC block */
			return SmcHwReadSizeOver;
	}
	smc35x_ecc_calculate(nand, eccData, eccDataNums);
	for(i = 0; i < eccDataNums; i++)
	{
		LOG_INFO("calculate ecc: %d ",eccData[i]);
	}

	oob = smc35x_oob_init(nand, oob, &oob_size, smc35x_info->nand_size.spareBytesPerPage);
	oob_free = oob;
	if (oob_size >= ONFI_AXI_DATA_WIDTH) {
		data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		for (index = 0; index < oob_size - ONFI_AXI_DATA_WIDTH; ++index, ++oob)
		{
			target_read_u8(target, data_phase_addr, oob);
			LOG_INFO("page %d index %d read oob %x", page, index, *oob);
		}
		data_phase_addr = NAND_BASE | (1 << 21) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		for (index = 0; index < ONFI_AXI_DATA_WIDTH; ++index, ++oob)
		{
			target_read_u8(target, data_phase_addr, oob);
			LOG_INFO("page %d index %d read oob %x", page, index, *oob);
		}
	}

	TempBuf = oob_free;
	for(i = 0; i < eccDataNums; i++)
	{
		ReadEccData[i] = ~TempBuf[dataOffsetPtr[i]];
		LOG_INFO("read ecc: %d", ReadEccData[i]);
	}

	i = smc35x_info->nand_size.dataBytesPerPage/NAND_ECC_BLOCK_SIZE;
	for(; i; i--)
	{
		status = Nand_HwCorrectEcc(&ReadEccData[EccOffset], &eccData[EccOffset], TempBuf);
		if(status != SUCCESS_FLAG){
			return status;
		}

		EccOffset += NAND_ECC_BYTES;
		TempBuf += NAND_ECC_BLOCK_SIZE;
	}

	if (!oob_data) {
		free(oob_free);
	}

	return ERROR_OK;
}

int smc35x_read_page(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	uint32_t status = 0;
	uint32_t cmd_phase_data;

	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	nand_size_type *nand_size = &smc35x_info->nand_size;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	LOG_INFO("data ptr %p data size %d oob ptr %p oob size %d", data, data_size, oob, oob_size);

	/* Restore the ECC mem command1 and ECC mem command2 register if the previous command is read page cache */
	/* Set SMC_REG_ECC1_MEMCMD0 Reg*/
	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_MEMCMD0),
			(SMC_EccMemCmd0_InitWriteCmd | SMC_EccMemCmd0_InitReadCmd | SMC_EccMemCmd0_EndReadCmd | SMC_EccMemCmd0_UseEndCmd));
	/* Set SMC_REG_ECC1_MEMCMD1 Reg*/
	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_MEMCMD1),
			(SMC_EccMemCmd1_ColChangeWRCmd | SMC_EccMemCmd1_ColChangeRDCmd | SMC_EccMemCmd1_ColChangeEndCmd | SMC_EccMemCmd1_UseEndCmd));

	smc35x_command(nand, NAND_CMD_READ0);
	cmd_phase_data = 0 | (page << (2*8));
	target_write_u32(target, smc35x_info->cmd_phase_addr, cmd_phase_data);
	cmd_phase_data = page >> (32 - (2*8));
	target_write_u32(target, smc35x_info->cmd_phase_addr, cmd_phase_data);

	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	status = target_read_u32(target, (SMC_BASE + SMC_REG_MEM_CFG_CLR), &status);
	target_write_u32(target, (SMC_BASE + SMC_REG_MEM_CFG_CLR), status | SMC_MemCfgClr_ClrSmcInt1);

	if (nand_size->eccNum == 1)
	{
		LOG_INFO("1 bit ecc");
		smc35x_read_page_ecc1(nand, page, data, data_size, oob, oob_size);
	} else {
		LOG_INFO("on die ecc");
		smc35x_read_page_internalecc(nand, page, data, data_size, oob, oob_size);
	}

	return ERROR_OK;
}


int smc35x_write_page(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	uint32_t index, status, eccDataNums, *dataOffsetPtr;
	uint8_t eccData[12] = {0};
	uint8_t *oob_data = oob, *oob_free;

	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	nand_size_type *nand_size = &smc35x_info->nand_size;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}
	LOG_INFO("data ptr %p data size %d oob ptr %p oob size %d", data, data_size, oob, oob_size);

	// nand_page_command(nand, page, NAND_CMD_SEQIN, !data);
	cmd_phase_addr = NAND_BASE | (ONFI_CMD_PROGRAM_PAGE_CYCLES << 21) | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (ONFI_CMD_PROGRAM_PAGE1 << 3);
	cmd_phase_data = 0 | (page << (2*8));
	data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);
	cmd_phase_data = page >> (32 - (2*8));
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);

	for (index = 0; index < data_size-ONFI_AXI_DATA_WIDTH; ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, data[index]);
		target_write_u8(target, data_phase_addr, data[index]);
	}
	data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (1 << 10);
	data += data_size - ONFI_AXI_DATA_WIDTH;
	for (index = 0; index < ONFI_AXI_DATA_WIDTH; ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, data[index]);
		target_write_u8(target, data_phase_addr, data[index]);
	}

	switch(nand_size->spareBytesPerPage)
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
			return FAILED_FLAG;
	}
	oob_data = smc35x_oob_init(nand, oob, &oob_size, nand_size->spareBytesPerPage);
	oob_free = oob_data;
	smc35x_ecc_calculate(nand, eccData, eccDataNums);
	for(index = 0; index < eccDataNums; index++)
	{
		oob_data[dataOffsetPtr[index]] = (~eccData[index]);
	}

	data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	oob_size = nand_size->spareBytesPerPage-ONFI_AXI_DATA_WIDTH;
	for (index = 0; index < oob_size; ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, oob_data[index]);
		target_write_u8(target, data_phase_addr, oob_data[index]);
	}
	data_phase_addr = NAND_BASE | (1 << 21) | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	oob_size = ONFI_AXI_DATA_WIDTH;
	oob_data += (nand_size->spareBytesPerPage-ONFI_AXI_DATA_WIDTH);
	for (index = 0; index < oob_size; ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, oob_data[index]);
		target_write_u8(target, data_phase_addr, oob_data[index]);
	}

	status = target_read_u32(target, (SMC_BASE + SMC_REG_MEM_CFG_CLR), &status);
	target_write_u32(target, (SMC_BASE + SMC_REG_MEM_CFG_CLR), status | SMC_MemCfgClr_ClrSmcInt1);

	if (!oob) {
		free(oob_free);
	}

	return ERROR_OK;
}

static int smc35x_init(struct nand_device *nand)
{
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	target_write_u32(target, PS_MIO0, 0x02);
	target_write_u32(target, PS_MIO2, 0x02);
	target_write_u32(target, PS_MIO3, 0x02);
	target_write_u32(target, PS_MIO4, 0x02);
	target_write_u32(target, PS_MIO5, 0x02);
	target_write_u32(target, PS_MIO6, 0x02);
	target_write_u32(target, PS_MIO7, 0x02);
	target_write_u32(target, PS_MIO8, 0x02);
	target_write_u32(target, PS_MIO9, 0x02);
	target_write_u32(target, PS_MIO10, 0x02);
	target_write_u32(target, PS_MIO11, 0x02);
	target_write_u32(target, PS_MIO12, 0x02);
	target_write_u32(target, PS_MIO13, 0x02);
	target_write_u32(target, PS_MIO14, 0x02);

	smc35x_nand_init(nand);

	return ERROR_OK;
}

int smc35x_command_re(struct nand_device *nand, uint8_t startCmd, uint8_t endCmd, uint8_t addrCycles, uint8_t endCmdPhase, int Page, int Column)
{
	struct target *target = nand->target;
	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	uint32_t endCmdReq = 0, cmdPhaseData  = 0;
	volatile uint64_t cmdPhaseAddr  = 0;
	if (endCmdPhase == ONFI_ENDIN_CMD_PHASE)
	{
		endCmdReq = 1;
	}

	cmdPhaseAddr  = NAND_BASE 		|
			(addrCycles << 21)		|
			(endCmdReq 	<< 20)		|
			NAND_COMMAND_PHASE_FLAG	|
			(endCmd 	<< 11)		|
			(startCmd 	<< 3);

	if ((Column != NAND_COLUMN_NOT_VALID) && (Page != NAND_PAGE_NOT_VALID))
	{
		cmdPhaseData = Column;

		cmdPhaseData |= Page << (3*8);
		if (addrCycles > 4)
		{
			target_write_u32(target, cmdPhaseAddr, cmdPhaseData);
			
			cmdPhaseData = Page >> (32 - (3*8));
		}
	}
	else if (Page != NAND_PAGE_NOT_VALID)
	{
		cmdPhaseData = Page;
	}
	else
	{
		cmdPhaseData = Column;
	}

	target_write_u32(target, cmdPhaseAddr, cmdPhaseData);

	return ERROR_OK;
}
void smc35x_read_data_re(struct nand_device *nand, uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length)
{
	struct target *target = nand->target;
	uint32_t endCmdReq = 0;
	volatile uint64_t dataPhaseAddr = 0;
	uint32_t eccLast = 0;
	uint32_t clearCs = 0;
	volatile uint32_t Index = 0;

	if(endCmdPhase == ONFI_ENDIN_DATA_PHASE)
	{
		endCmdReq = 1;
	}

	/* data phase address */
	dataPhaseAddr  = NAND_BASE 		|
			(clearCs 	<< 21)		|
			(endCmdReq 	<< 20)		|
			NAND_DATA_PHASE_FLAG	|
			(endCmd 	<< 11)		|
			(eccLast 	<< 10);

	/* Read Data */
	for(Index = 0;Index < Length;Index++)
	{
		//Buf[Index] = *((uint8_t *)dataPhaseAddr);
		target_read_u8(target, dataPhaseAddr, &Buf[Index]);
		LOG_INFO("read id byte %d data %x", Index, Buf[Index]);
	}
}
int smc35x_read_id_re(struct nand_device *nand)
{
	struct target *target = nand->target;
	LOG_INFO("target is %s", target->cmd_name);
	
	if (target == NULL) {
		LOG_INFO("target error");
		return ERROR_NAND_OPERATION_FAILED;
	}

	if (target->state != TARGET_HALTED) {
		LOG_INFO("target must be halted to use NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	target_write_u32(target, PS_MIO0, 0x02);
	target_write_u32(target, PS_MIO2, 0x02);
	target_write_u32(target, PS_MIO3, 0x02);
	target_write_u32(target, PS_MIO4, 0x02);
	target_write_u32(target, PS_MIO5, 0x02);
	target_write_u32(target, PS_MIO6, 0x02);
	target_write_u32(target, PS_MIO7, 0x02);
	target_write_u32(target, PS_MIO8, 0x02);
	target_write_u32(target, PS_MIO9, 0x02);
	target_write_u32(target, PS_MIO10, 0x02);
	target_write_u32(target, PS_MIO11, 0x02);
	target_write_u32(target, PS_MIO12, 0x02);
	target_write_u32(target, PS_MIO13, 0x02);
	target_write_u32(target, PS_MIO14, 0x02);

    uint8_t device_id[5]={0};

	smc35x_command_re(nand, ONFI_CMD_RESET1, ONFI_CMD_RESET2, ONFI_CMD_RESET_CYCLES,
			ONFI_CMD_RESET_END_TIMING, ONFI_PAGE_NOT_VALID, ONFI_COLUMN_NOT_VALID);
    
	smc35x_command_re(nand, ONFI_CMD_READ_ID1, ONFI_CMD_READ_ID2, ONFI_CMD_READ_ID_CYCLES,ONFI_CMD_READ_ID_END_TIMING, ONFI_PAGE_NOT_VALID, 0x00);
	
	smc35x_read_data_re(nand, ONFI_CMD_READ_ID2, ONFI_CMD_READ_ID_END_TIMING, &device_id[0], 5);

    return ERROR_OK;
}
COMMAND_HANDLER(handle_smc35x_id_command)
{
    unsigned num;
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], num);
	struct nand_device *nand = get_nand_device_by_num(num);
	if (!nand) {
		command_print(CMD, "nand device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}
	smc35x_command(nand, NAND_CMD_RESET);
	smc35x_reset(nand);

	smc35x_read_id_re(nand);

	return ERROR_OK;
}
COMMAND_HANDLER(handle_smc35x_size_command)
{
    unsigned num;
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], num);
	struct nand_device *nand = get_nand_device_by_num(num);
	if (!nand) {
		command_print(CMD, "nand device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	nand_size_type nand_size;
	smc35x_read_parameter(nand, &nand_size);

	LOG_INFO("eccNum:%x totalUint:%x blocksPerUnit:%x pagesPerBlock:%x dataBytesPerPage:%x spareBytesPerPage:%x",
		nand_size.eccNum,nand_size.totalUnit,nand_size.blocksPerUnit,nand_size.pagesPerBlock,nand_size.dataBytesPerPage,nand_size.spareBytesPerPage);
	
	LOG_INFO("eccNum:%d totalUint:%d blocksPerUnit:%d pagesPerBlock:%d dataBytesPerPage:%d spareBytesPerPage:%d \r\n",
		nand_size.eccNum,nand_size.totalUnit,nand_size.blocksPerUnit,nand_size.pagesPerBlock,nand_size.dataBytesPerPage,nand_size.spareBytesPerPage);

	return ERROR_OK;
}
static const struct command_registration smc35x_exec_command_handlers[] = {
	{
		.name = "id",
		.handler = handle_smc35x_id_command,
		.mode = COMMAND_ANY,
		.help = "select nand id",
		.usage = "nand_id [  ]",
	},
	{
		.name = "size",
		.handler = handle_smc35x_size_command,
		.mode = COMMAND_ANY,
		.help = "select nand id",
		.usage = "nand_id [  ]",
	},
	COMMAND_REGISTRATION_DONE
};
static const struct command_registration smc35x_command_handler[] = {
	{
		.name = "smc35x",
		.mode = COMMAND_ANY,
		.help = "SMC35X NAND flash controller commands",
		.usage = "",
		.chain = smc35x_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

struct nand_flash_controller smc35x_nand_controller = {
	.name = "smc35x",
	.commands = smc35x_command_handler,
	.nand_device_command = smc35x_nand_device_command,
	.init = smc35x_init,
	.reset = smc35x_reset,
	.command = smc35x_command,
	.address = smc35x_address,
	.write_data = smc35x_write_data,
	.read_data = smc35x_read_data,
	.write_page = smc35x_write_page,
	.read_page = smc35x_read_page,
	.nand_ready = NULL,
};
