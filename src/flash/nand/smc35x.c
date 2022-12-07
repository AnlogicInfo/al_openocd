#include "onfi.h"
#include "imp.h"
#include "smc35x.h"
#include <target/target.h>
#include <target/algorithm.h>
#include <target/riscv/riscv.h>
#include <target/aarch64.h>
#include <helper/time_support.h>

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
	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	nand_size_type nand_size;
};

static const uint8_t riscv32_bin[] = {
#include "../../../contrib/loaders/flash/smc35x/riscv32_smc35x.inc"
};
static const uint8_t riscv64_bin[] = {
#include "../../../contrib/loaders/flash/smc35x/riscv64_smc35x.inc"
};
static const uint8_t aarch64_bin[] = {
#include "../../../contrib/loaders/flash/smc35x/aarch64_smc35x.inc"
};

static uint32_t __attribute__((aligned(4))) NandOob64[12] = {52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
static uint32_t __attribute__((aligned(4))) NandOob32[6] = {26, 27, 28, 29, 30, 31};
static uint32_t __attribute__((aligned(4))) NandOob16[3] = {13, 14, 15};		/* data size 512bytes */

NAND_DEVICE_COMMAND_HANDLER(smc35x_nand_device_command)
{
	struct smc35x_nand_controller* smc35x_info;

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


void smc35x_read_buf(struct nand_device *nand, uint8_t *buf, uint32_t length, uint64_t addr)
{
	struct target *target = nand->target;

	uint32_t index = 0;
	uint32_t *temp_buff = (uint32_t *)buf;
	uint32_t temp_length = length >> 2;

	for (index = 0; index < temp_length; index++) {
		target_read_u32(target, addr, &temp_buff[index]);
	}
}
void smc35x_write_buf(struct nand_device *nand, uint8_t *buf, uint32_t length, uint64_t addr)
{
	struct target *target = nand->target;

	uint32_t index = 0;
	uint32_t *temp_buff = (uint32_t *)buf;
	uint32_t temp_length = length >> 2;

	for (index = 0; index < temp_length; index++) {
		target_write_u32(target, addr, temp_buff[index]);
	}
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
int smc35x_address_u32(struct nand_device *nand, uint32_t address)
{
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use S3C24XX NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	target_write_u32(target, smc35x_info->cmd_phase_addr, address);

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
	uint8_t __attribute__((aligned(4))) temp[256] = {0};
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

	for (index = 0; index < 256; index++) {
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


uint32_t smc35x_ecc1_init(struct nand_device *nand, nand_size_type *nand_size)
{
	uint32_t ecc1Config, status;
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	/* Restore the ECC mem command1 and ECC mem command2 register if the previous command is read page cache */
	/* Set SMC_REG_ECC1_MEMCMD0 Reg*/
	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_MEMCMD0),
			(SMC_EccMemCmd0_InitWriteCmd | SMC_EccMemCmd0_InitReadCmd | SMC_EccMemCmd0_EndReadCmd | SMC_EccMemCmd0_UseEndCmd));
	/* Set SMC_REG_ECC1_MEMCMD1 Reg*/
	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_MEMCMD1),
			(SMC_EccMemCmd1_ColChangeWRCmd | SMC_EccMemCmd1_ColChangeRDCmd | SMC_EccMemCmd1_ColChangeEndCmd | SMC_EccMemCmd1_UseEndCmd));

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
			return SmcHwInitSizeErr;
	}

	do {
		target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_STATUS), &status);
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
		target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_STATUS), &status);
	} while (status & SMC_EccStatus_EccBusy);

	target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_CFG), &config);
	target_write_u32(target, (SMC_BASE + SMC_REG_ECC1_CFG), config & (~(3 << SMC_EccCfg_EccMode_FIELD)));
	
	return ERROR_OK;
}
uint32_t smc35x_nand_init(struct nand_device *nand)
{
	uint32_t status = 0, extid = 0;
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

	if ((nand_size->device_id[0] != NAND_MFR_SAMSUNG) && (nand_size->device_id[0] != NAND_MFR_ESMT))
	{
		status = smc35x_read_parameter(nand, nand_size);

		if (status != SmcSuccess)
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

		if ((nand_size->dataBytesPerPage > SMC_MAX_PAGE_SIZE) ||
			(nand_size->spareBytesPerPage > SMC_MAX_SPARE_SIZE)||
			(nand_size->pagesPerBlock > SMC_MAX_PAGES_PER_BLOCK))
		{
			return SmcSamsungParamOver;
		}

		/* form myself */
		nand_size->eccNum = 1;
	}

	if ((nand_size->device_id[0] == NAND_MFR_AMD) && (nand_size->eccNum == 0))
	{
		/* Because SkyHigh ecc nums is zero */
		nand_size->eccNum = 4;
	}

	if (nand_size->eccNum == 1)
	{
		/* Enable SMC IP Hardware */
		status = smc35x_ecc1_init(nand, nand_size);

		if (status != SmcSuccess)
		{
			return status;
		}
	}
	else
	{
		/* Close SMC IP ECC */
		status = smx35x_ecc1_disable(nand);

		if (status != SmcSuccess)
		{
			return status;
		}

		if ((nand_size->device_id[0] == NAND_MFR_AMD)||
			(nand_size->device_id[0] == NAND_MFR_MICRON))
		{
			/* Enable On Die ECC */
			status = smc35x_internalecc_init(nand);

			if (status != SmcSuccess)
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
	} while (status);

	for (ecc_reg = 0; ecc_reg < ecc_data_nums/3; ++ecc_reg)
	{
		target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_BLOCK0 + ecc_reg * 4), &ecc_value);
		LOG_INFO("ecc value: %x", ecc_value);
		if ((ecc_value) & (1 << SMC_EccBlock_ISCheakValueValid_FIELD))
		{
			for (count=0; count < 3; ++count)
			{
				*ecc_data = ecc_value & 0xFF;
				LOG_INFO("dst: %d", *ecc_data);
				ecc_value = ecc_value >> 8;
				++ecc_data;
			}
		}
		else
		{
			LOG_INFO("EccInvalidErr");
			return SmcEccDataInvalidErr;
		}
	}

	return ERROR_OK;
}
uint8_t smc35x_ecc_correct(uint8_t *eccCode, uint8_t *eccCalc, uint8_t *buf)
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
	{
		LOG_INFO("0 bit error");
		return SmcSuccess;
	}

	/* One bit Error */
	if (eccOdd == (~eccEven & 0xfff))
	{
		bytePos = (eccOdd >> 3) & 0x1ff;
		bitPos = eccOdd & 0x07;
		/* Toggling error bit */
		buf[bytePos] ^= (1 << bitPos);
		LOG_INFO("1 bit error");
		LOG_INFO("bytepos:%u bitpos:%u", bytePos, bitPos);
		return SmcSuccess;
	}

	/* Two bits Error */
	if (OneHot((eccOdd | eccEven)) == SmcSuccess)
	{
		LOG_INFO("2 bit error, return");
		return SmcTwoBitsErr;
	}

	LOG_INFO("multiple bit error");
	/* Multiple bits error */
	return SmcMultipleBitsErr;
}
int smc35x_read_page_internalecc(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	uint8_t *oob_data = oob, *oob_free;
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	uint64_t data_phase_addr = smc35x_info->data_phase_addr;

	if (smc35x_info->nand_size.device_id[0] == NAND_MFR_MICRON) {
		target_write_u32(target, NAND_BASE, 0x00);
	}

	smc35x_read_buf(nand, data, data_size, data_phase_addr);

	oob = smc35x_oob_init(nand, oob, &oob_size, smc35x_info->nand_size.spareBytesPerPage);
	oob_free = oob;

	smc35x_read_buf(nand, oob, oob_size - ONFI_AXI_DATA_WIDTH, data_phase_addr);
	oob += (oob_size - ONFI_AXI_DATA_WIDTH);

	data_phase_addr = NAND_BASE | (1 << 21) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
	smc35x_read_buf(nand, oob, ONFI_AXI_DATA_WIDTH, data_phase_addr);

	if (!oob_data) {
		free(oob_free);
	}

	return ERROR_OK;
}
int smc35x_read_page_ecc1(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	uint64_t data_phase_addr = smc35x_info->data_phase_addr;

	uint32_t status = 0, index = 0;
	uint8_t *oob_data = oob, *page_data = data, *oob_free;

	uint8_t eccDataNums = 0, EccOffset = 0;
	uint8_t __attribute__((aligned(4))) eccData[12] = {0};
	uint8_t __attribute__((aligned(4))) ReadEccData[12] = {0};
	uint32_t *dataOffsetPtr = NULL;
	uint8_t *TempBuf = oob;

	if (data == NULL) {
		LOG_INFO("read only oob data");
		data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
		
		for (index = 0; index < oob_size; ++index, ++oob) {
			target_read_u8(target, data_phase_addr, oob);
		}

		return ERROR_OK;
	}

	smc35x_read_buf(nand, data, data_size - ONFI_AXI_DATA_WIDTH, data_phase_addr);
	data += (data_size - ONFI_AXI_DATA_WIDTH);

	data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11) | (ECC_LAST << 10);
	smc35x_read_buf(nand, data, ONFI_AXI_DATA_WIDTH, data_phase_addr);

	target_read_u32(target, (SMC_BASE + SMC_REG_ECC1_STATUS), &status);
	LOG_INFO("ecc status register: %x", status);

	switch (smc35x_info->nand_size.spareBytesPerPage)
	{
		case(64):
			eccDataNums = 12;
			dataOffsetPtr = NandOob64;
			break;
		case(32):
			eccDataNums = 6;
			dataOffsetPtr = NandOob32;
			break;
		case(16):
			eccDataNums = 3;
			dataOffsetPtr = NandOob16;
			break;
		default:
			/* Page size 256 bytes & 4096 bytes not supported by ECC block */
			return SmcHwReadSizeOver;
	}
	smc35x_ecc_calculate(nand, eccData, eccDataNums);
	for (index = 0; index < eccDataNums; ++index) {
		LOG_INFO("calculate ecc: %d ", eccData[index]);
	}

	oob = smc35x_oob_init(nand, oob, &oob_size, smc35x_info->nand_size.spareBytesPerPage);
	oob_free = oob;
	
	data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
	smc35x_read_buf(nand, oob, oob_size - ONFI_AXI_DATA_WIDTH, data_phase_addr);
	oob += (oob_size - ONFI_AXI_DATA_WIDTH);
	
	data_phase_addr = NAND_BASE | (1 << 21) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);
	smc35x_read_buf(nand, oob, ONFI_AXI_DATA_WIDTH, data_phase_addr);
	
	TempBuf = oob_free;
	for (index = 0; index < eccDataNums; ++index)
	{
		ReadEccData[index] = ~TempBuf[dataOffsetPtr[index]];
		LOG_INFO("read ecc: %d", ReadEccData[index]);
	}

	index = smc35x_info->nand_size.dataBytesPerPage / NAND_ECC_BLOCK_SIZE;
	for (; index; --index)
	{
		LOG_INFO("ecc correct %d", index);
		status = smc35x_ecc_correct(&ReadEccData[EccOffset], &eccData[EccOffset], page_data);
		if (status != SUCCESS_FLAG) {
			if (!oob_data) {
				free(oob_free);
			}

			return status;
		}

		EccOffset += NAND_ECC_BYTES;
		page_data += NAND_ECC_BLOCK_SIZE;
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
	LOG_INFO("start to read page %d data", page);

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
	target_read_u32(target, (SMC_BASE + SMC_REG_MEM_CFG_CLR), &status);
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


uint8_t nand_busy(struct nand_device *nand)
{
	uint32_t status = 0;
	struct target *target = nand->target;

	target_read_u32(target, (SMC_BASE + SMC_REG_MEMC_STATUS), &status);
	LOG_INFO("status reg %x", status);
	status &= (1 << SMC_MemcStatus_SmcInt1RawStatus_FIELD);

	LOG_INFO("int1 status %d", status);
	
	if (status)
		return NAND_READY;
	else
		return NAND_BUSY;
}
int slow_smc35x_write_page(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	uint32_t index, status;
	uint32_t eccDataNums = 0, *dataOffsetPtr = NULL;
	uint8_t eccData[12] = {0}, state;
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
	LOG_INFO("make sure page %d is cleared before writing", page);

	smc35x_command(nand, NAND_CMD_STATUS);
	target_read_u8(target, (smc35x_info->data_phase_addr), &state);
	if (!(state & ONFI_STATUS_WP)) {
		return FAILED_FLAG;
	}

	cmd_phase_addr = NAND_BASE | (ONFI_CMD_PROGRAM_PAGE_CYCLES << 21) | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (ONFI_CMD_PROGRAM_PAGE1 << 3);
	cmd_phase_data = 0 | (page << (2*8));
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);
	cmd_phase_data = page >> (32 - (2*8));
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);

	data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	smc35x_write_buf(nand, data, data_size - ONFI_AXI_DATA_WIDTH, data_phase_addr);

	data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (1 << 10);
	data += (data_size - ONFI_AXI_DATA_WIDTH);
	smc35x_write_buf(nand, data, ONFI_AXI_DATA_WIDTH, data_phase_addr);
	
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
			break;
	}
	oob_data = smc35x_oob_init(nand, oob, &oob_size, nand_size->spareBytesPerPage);
	oob_free = oob_data;
	if (nand_size->eccNum == 1 && dataOffsetPtr != NULL) {
		smc35x_ecc_calculate(nand, eccData, eccDataNums);
		for (index = 0; index < eccDataNums; index++)
		{
			oob_data[dataOffsetPtr[index]] = (~eccData[index]);
		}
	}

	data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	smc35x_write_buf(nand, oob_data, oob_size - ONFI_AXI_DATA_WIDTH, data_phase_addr);

	data_phase_addr = NAND_BASE | (1 << 21) | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	oob_data += (oob_size - ONFI_AXI_DATA_WIDTH);
	smc35x_write_buf(nand, oob_data, ONFI_AXI_DATA_WIDTH, data_phase_addr);
	
	while (nand_busy(nand) == NAND_BUSY);

	target_read_u32(target, (SMC_BASE + SMC_REG_MEM_CFG_CLR), &status);
	target_write_u32(target, (SMC_BASE + SMC_REG_MEM_CFG_CLR), status | SMC_MemCfgClr_ClrSmcInt1);

	smc35x_command(nand, NAND_CMD_STATUS);
	target_read_u8(target, (smc35x_info->data_phase_addr), &state);
	if (status & ONFI_STATUS_FAIL) {
		return FAILED_FLAG;
	}

	if (!oob) {
		free(oob_free);
	}

	return ERROR_OK;
}
int smc35x_write_page_re(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	int retval = ERROR_OK;
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	nand_size_type *nand_size = &smc35x_info->nand_size;
	uint32_t count = nand_size->dataBytesPerPage;

	// 设置工作区
	uint32_t xlen = 0;
	struct reg_param reg_params[7];
	struct working_area *algorithm_wa = NULL;
	struct working_area *data_wa = NULL;
	const uint8_t *bin;
	size_t bin_size;
	uint8_t loader_target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}
	LOG_INFO("make sure page is cleared before writing");

	uint32_t index, first_block, last_block;
	first_block = page / nand_size->pagesPerBlock;
	last_block = (page + (data_size / count) - 1) / nand_size->pagesPerBlock;
	for (index = first_block; index <= last_block; ++index)
	{
		if (nand->blocks[index].is_erased == 1)
			nand->blocks[index].is_erased = 0;
	}

	if (strncmp(target_name(target), "riscv", 4) == 0) {
		loader_target = RISCV;
		xlen = riscv_xlen(target);
		if (xlen == 32) {
			LOG_INFO("target arch: riscv32");
			bin = riscv32_bin;
			bin_size = sizeof(riscv32_bin);
		} else {
			LOG_INFO("target arch: riscv64");
			bin = riscv64_bin;
			bin_size = sizeof(riscv64_bin);
		}
	} else {
		LOG_INFO("target arch: aarch64");
		loader_target = ARM;
		xlen = 64;
		bin = aarch64_bin;
		bin_size = sizeof(aarch64_bin);
	}

	if (target_alloc_working_area(target, bin_size, &algorithm_wa) == ERROR_OK) {
		if (target_write_buffer(target, algorithm_wa->address, bin_size, bin) != ERROR_OK) {
			LOG_ERROR("Failed to write code to " TARGET_ADDR_FMT ": %d",
					algorithm_wa->address, retval);
			target_free_working_area(target, algorithm_wa);
			algorithm_wa = NULL;
		} else {
			if (target_alloc_working_area(target, count, &data_wa) != ERROR_OK) {
				LOG_WARNING("Couldn't allocate data working area.");
				target_free_working_area(target, algorithm_wa);
				algorithm_wa = NULL;
			}
		}
	} else {
		LOG_WARNING("Couldn't allocate %zd-byte working area.", bin_size);
		algorithm_wa = NULL;
	}
	uint64_t data_wa_address = data_wa->address;

	LOG_INFO("work area algorithm address: %llx, data length: %d", algorithm_wa->address, algorithm_wa->size);
	LOG_INFO("work area data address: %llx, data length: %d", data_wa->address, data_wa->size);
	if (algorithm_wa)
    {
        if (loader_target == RISCV)
        {
    	    init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
		    init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[4], "a4", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[5], "a5", xlen, PARAM_OUT);
			init_reg_param(&reg_params[6], "a6", xlen, PARAM_OUT);
        }
        else if (loader_target == ARM)
        {
    	    init_reg_param(&reg_params[0], "x0", xlen, PARAM_IN_OUT);
		    init_reg_param(&reg_params[1], "x1", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[2], "x2", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[3], "x3", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[4], "x4", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[5], "x5", xlen, PARAM_OUT);
			init_reg_param(&reg_params[6], "x6", xlen, PARAM_OUT);
        }

		while (data_size > 0)
        {
			LOG_INFO("write page %d buffer adder %llx", page, data_wa_address);
			
			retval = target_write_buffer(target, data_wa->address, count, data);
			if (retval != ERROR_OK) {
				LOG_DEBUG("Failed to write %d bytes to " TARGET_ADDR_FMT ": %d",
						nand_size->dataBytesPerPage, data_wa->address, retval);
				goto err;
			}

			buf_set_u64(reg_params[0].value, 0, xlen, SMC_BASE);
			buf_set_u64(reg_params[1].value, 0, xlen, nand_size->dataBytesPerPage);
			buf_set_u64(reg_params[2].value, 0, xlen, data_wa_address);
			buf_set_u64(reg_params[3].value, 0, xlen, page++);
			buf_set_u64(reg_params[4].value, 0, xlen, nand_size->spareBytesPerPage);
			buf_set_u64(reg_params[5].value, 0, xlen, NAND_BASE);
			buf_set_u64(reg_params[6].value, 0, xlen, nand_size->eccNum);
			
			retval = target_run_algorithm(target, 0, NULL,
					ARRAY_SIZE(reg_params), reg_params,
					algorithm_wa->address, 0, 20000, NULL);
			if (retval != ERROR_OK) {
				LOG_ERROR("Failed to execute algorithm at " TARGET_ADDR_FMT ": %d",
						algorithm_wa->address, retval);
				goto err;
			}

			uint64_t algorithm_result = buf_get_u64(reg_params[0].value, 0, xlen);
			if (algorithm_result != 0) {
			    LOG_DEBUG("Algorithm returned error %llx", algorithm_result);
				retval = ERROR_FAIL;
				goto err;
			}
			
			data += count;
			data_size -= count;
        }

    	target_free_working_area(target, data_wa);
		target_free_working_area(target, algorithm_wa);
    }
    else
    {
		LOG_ERROR("write_image conmmand fail, try to use command write");
		// retval = ERROR_FAIL;

		while (data_size > 0) {
			retval = slow_smc35x_write_page(nand, page++, data, count, NULL, 0);
			data += count;
			data_size -= count;
		}
    }

err:
	target_free_working_area(target, data_wa);
	target_free_working_area(target, algorithm_wa);

    return retval;
}
int smc35x_write_page(struct nand_device *nand, uint32_t page, uint8_t *data, uint32_t data_size,
			uint8_t *oob, uint32_t oob_size)
{
	int retval = ERROR_OK;
	struct target *target = nand->target;
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	nand_size_type *nand_size = &smc35x_info->nand_size;
	uint32_t buffer_size = 16384 + 8;	// 16KB + 8B
	uint32_t count = nand_size->dataBytesPerPage;

	// 设置工作区
	struct reg_param reg_params[7];
	uint32_t xlen = 0, loader_target;
	struct working_area *write_algorithm;
	struct working_area *write_algorithm_sp;
	struct working_area *source;
	const uint8_t *bin;
	size_t bin_size;

	// struct aarch64_algorithm aarch64_info;
	struct riscv_algorithm *riscv_info = calloc(1, sizeof(struct riscv_algorithm));
	assert(riscv_info);
	// void *arch_info = NULL;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}
	LOG_INFO("make sure page is cleared before writing");

	uint32_t index, first_block, last_block;
	first_block = page / nand_size->pagesPerBlock;
	last_block = (page + (data_size / count) - 1) / nand_size->pagesPerBlock;
	for (index = first_block; index <= last_block; ++index)
	{
		if (nand->blocks[index].is_erased == 1)
			nand->blocks[index].is_erased = 0;
	}

	if (strncmp(target_name(target), "riscv", 4) == 0) {
		loader_target = RISCV;
		xlen = riscv_xlen(target);
		if (xlen == 32) {
			LOG_INFO("target arch: riscv32");
			bin = riscv32_bin;
			bin_size = sizeof(riscv32_bin);
		} else {
			LOG_INFO("target arch: riscv64");
			bin = riscv64_bin;
			bin_size = sizeof(riscv64_bin);
		}
		// arch_info = &riscv_info;
	} else {
		LOG_INFO("target arch: aarch64");
		loader_target = ARM;
		xlen = 64;
		bin = aarch64_bin;
		bin_size = sizeof(aarch64_bin);
		// arch_info = &aarch64_info;
	}

	if (target_alloc_working_area(target, bin_size, &write_algorithm) == ERROR_OK) {
		if (target_write_buffer(target, write_algorithm->address, bin_size, bin) != ERROR_OK) {
			LOG_ERROR("failed to write code to " TARGET_ADDR_FMT ": %d", write_algorithm->address, retval);
			target_free_working_area(target, write_algorithm);
			write_algorithm = NULL;
		} else {
			if (target_alloc_working_area(target, buffer_size, &source) != ERROR_OK) {
				LOG_WARNING("no large enough working area available");
				target_free_working_area(target, write_algorithm);
				write_algorithm = NULL;
			}
		}
	} else {
		LOG_WARNING("couldn't allocate %zd-byte working area.", bin_size);
		write_algorithm = NULL;
	}
	if (target_alloc_working_area(target, 128, &write_algorithm_sp) != ERROR_OK) {
		LOG_DEBUG("no working area for write code stack pointer");
		target_free_working_area(target, write_algorithm);
		target_free_working_area(target, source);
		write_algorithm = NULL;
	}

	LOG_INFO("work area algorithm address: %llx, data length: %d", write_algorithm->address, write_algorithm->size);
	LOG_INFO("work area data address: %llx, data length: %d", source->address, source->size);
	if (write_algorithm)
    {
        if (loader_target == RISCV)
        {
    	    init_reg_param(&reg_params[0], "a0", xlen, PARAM_IN_OUT);
		    init_reg_param(&reg_params[1], "a1", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[2], "a2", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[3], "a3", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[4], "a4", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[5], "a5", xlen, PARAM_OUT);
			init_reg_param(&reg_params[6], "a6", xlen, PARAM_OUT);
        }
        else if (loader_target == ARM)
        {
    	    init_reg_param(&reg_params[0], "x0", xlen, PARAM_IN_OUT);
		    init_reg_param(&reg_params[1], "x1", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[2], "x2", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[3], "x3", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[4], "x4", xlen, PARAM_OUT);
		    init_reg_param(&reg_params[5], "x5", xlen, PARAM_OUT);
			init_reg_param(&reg_params[6], "x6", xlen, PARAM_OUT);
        }

		buf_set_u64(reg_params[0].value, 0, xlen, source->address);
		buf_set_u64(reg_params[1].value, 0, xlen, source->address + source->size);
		buf_set_u64(reg_params[2].value, 0, xlen, page);	//data + nand_size->page_size
		buf_set_u64(reg_params[3].value, 0, xlen, data_size);
		buf_set_u64(reg_params[4].value, 0, xlen, nand_size->dataBytesPerPage);
		buf_set_u64(reg_params[5].value, 0, xlen, nand_size->spareBytesPerPage);
		buf_set_u64(reg_params[6].value, 0, xlen, nand_size->eccNum);

		retval = target_run_flash_async_algorithm(target,
						data,
						data_size / nand_size->dataBytesPerPage,
						nand_size->dataBytesPerPage, /* Block size: we write in block of one page to enjoy burstwrite speed */
						0,
						NULL,
						ARRAY_SIZE(reg_params),
						reg_params,
						source->address,
						source->size,
						write_algorithm->address,
						0,
						riscv_info);

		if (retval == ERROR_FLASH_OPERATION_FAILED)
		{
			LOG_ERROR("error executing smc35x flash write algorithm");
			uint64_t algorithm_result = buf_get_u64(reg_params[0].value, 0, xlen);
			if (algorithm_result != ERROR_OK) {
				LOG_ERROR("algorithm returned error %lld", algorithm_result);
			}
		}
		if (retval == ERROR_OK)
		{
			uint32_t rp;
			/* Read back rp and check that is valid */
			retval = target_read_u32(target, source->address + 4, &rp);
			if (retval == ERROR_OK) {
				if ((rp < source->address + 8) || (rp > (source->address + source->size))) {
					LOG_ERROR("flash write failed %d", rp);
					retval = ERROR_FLASH_OPERATION_FAILED;
				}
			}
		}
		target_free_working_area(target, source);
		target_free_working_area(target, write_algorithm);
		target_free_working_area(target, write_algorithm_sp);

		destroy_reg_param(&reg_params[0]);
		destroy_reg_param(&reg_params[1]);
		destroy_reg_param(&reg_params[2]);
		destroy_reg_param(&reg_params[3]);
		destroy_reg_param(&reg_params[4]);
		destroy_reg_param(&reg_params[5]);
		destroy_reg_param(&reg_params[6]);
    }
    else
    {
		LOG_ERROR("write_image conmmand fail, try to use command write");
		// retval = ERROR_FAIL;

		while (data_size > 0) {
			retval = slow_smc35x_write_page(nand, page++, data, count, NULL, 0);
			data += count;
			data_size -= count;
		}
    }

    return retval;
}


static int smc35x_init(struct nand_device *nand)
{
	// uint32_t retval;
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
	// retval = smc35x_nand_init(nand);
	// if (retval != ERROR_OK) {
	//  LOG_INFO(err code: retval);
	// 	return retval;
	// }
	
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
	if (endCmdPhase == ONFI_ENDIN_CMD_PHASE) {
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

	if (endCmdPhase == ONFI_ENDIN_DATA_PHASE) {
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
	for (Index = 0;Index < Length;Index++)
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

	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	nand_size_type *nand_size = &smc35x_info->nand_size;

	if (!nand) {
		command_print(CMD, "nand device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	LOG_INFO("eccNum:%x totalUint:%x blocksPerUnit:%x pagesPerBlock:%x dataBytesPerPage:%x spareBytesPerPage:%x",
		nand_size->eccNum,nand_size->totalUnit,nand_size->blocksPerUnit,nand_size->pagesPerBlock,nand_size->dataBytesPerPage,nand_size->spareBytesPerPage);
	LOG_INFO("eccNum:%d totalUint:%d blocksPerUnit:%d pagesPerBlock:%d dataBytesPerPage:%d spareBytesPerPage:%d \r\n",
		nand_size->eccNum,nand_size->totalUnit,nand_size->blocksPerUnit,nand_size->pagesPerBlock,nand_size->dataBytesPerPage,nand_size->spareBytesPerPage);

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
	.address_u32 = smc35x_address_u32,
	.write_data = smc35x_write_data,
	.read_data = smc35x_read_data,
	.write_page = smc35x_write_page,
	.read_page = smc35x_read_page,
	.nand_ready = NULL,
};
