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

#define TEST ((test* )(SMC_BASE+SMC_REG_ECC1_STATUS))

uint32_t buf[256];

typedef struct{
	uint32_t dataBytesPerPage;		/* per page contains data byte numbers*/
	uint16_t spareBytesPerPage;		/* per page contains spare byte numbers*/
	uint32_t pagesPerBlock;			/* per block contains page  numbers*/
	uint32_t blocksPerUnit;			/* per unit contains block numbers*/
	uint8_t totalUnit;				/* total unit numbers*/
}nand_size_type;

struct smc35x_nand_controller{
	uint32_t ecc;
	uint32_t nand_base;
	uint32_t smc_base;

	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	nand_size_type nand_size;
};

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

	struct smc35x_nand_controller* smc35x_info = nand->controller_priv;
	struct target* target = nand->target;
	
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

	default: 
		LOG_INFO("no action");
		return ERROR_OK;
	}

	return ERROR_OK;
}

int smc35x_read_data(struct nand_device* nand, void* data)
{
	struct smc35x_nand_controller* smc35x_info = nand->controller_priv;
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

    target_read_u8(target, smc35x_info->data_phase_addr, data);

	return ERROR_OK;
}

int smc35x_address(struct nand_device* nand, uint8_t address)
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

int smc35x_reset(struct nand_device* nand)
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

int smc35x_read_parameter(struct nand_device* nand, nand_size_type* nand_size)
{
	uint32_t temp = 0, index;
	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	smc35x_command(nand, NAND_CMD_RESET);
	smc35x_reset(nand);

	cmd_phase_addr = NAND_BASE | (ONFI_CMD_READ_PARAMETER_CYCLES << 21) | (ONFI_CMD_READ_PARAMETER1 << 3);
	cmd_phase_data = 0x00;
	data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG;

	target_write_u32(target, cmd_phase_addr, cmd_phase_data);
	cmd_phase_addr = NAND_BASE;
	target_write_u32(target, cmd_phase_addr, 0x00);

	for (index = 0; index < 20; index++)
	{
		target_read_u32(target, data_phase_addr, &temp);
	}
	target_read_u32(target, data_phase_addr, &nand_size->dataBytesPerPage);
	target_read_u16(target, data_phase_addr, &nand_size->spareBytesPerPage);

	for (index = 0; index < 6; index++)
	{
		target_read_u8(target, data_phase_addr, (uint8_t* )&temp);
	}
	target_read_u32(target, data_phase_addr, &nand_size->pagesPerBlock);
	target_read_u32(target, data_phase_addr, &nand_size->blocksPerUnit);

	for (index = 0; index < 155; index++)
	{
		target_read_u8(target, data_phase_addr, (uint8_t* )&temp);
	}

	return ERROR_OK;
}

                                            //actual page address(Block address concatenated with page address)
int smc35x_read_page(struct nand_device* nand, uint32_t page, uint8_t* data_p, uint32_t data_size,
			uint8_t* oob_p, uint32_t oob_size)
{
	uint32_t index = 0;
	nand_size_type nand_size;
	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	data_size >>= 2, oob_size >>= 2;
	uint32_t *data = (uint32_t *)data_p;
	uint32_t *oob = (uint32_t *)oob_p;
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	LOG_INFO("page number %d data size %d word oob size %d word", page, data_size, oob_size);

	smc35x_read_parameter(nand, &nand_size);
    LOG_INFO("blocksPerUnit:%d pagesPerBlock:%d dataBytesPerPage:%d spareBytesPerPage:%d \r\n",
	nand_size.blocksPerUnit,nand_size.pagesPerBlock,nand_size.dataBytesPerPage,nand_size.spareBytesPerPage);
	
	cmd_phase_addr = NAND_BASE | (ONFI_CMD_READ_PAGE_CYCLES << 21) | (ONFI_CMD_READ_PAGE_END_TIMING << 20) | (ONFI_CMD_READ_PAGE2 << 11) | (ONFI_CMD_READ_PAGE1 << 3);
	cmd_phase_data = 0 | (page << (2*8));
	data_phase_addr = NAND_BASE  | NAND_DATA_PHASE_FLAG | (ONFI_CMD_READ_PAGE2 << 11);

	target_write_u32(target, cmd_phase_addr, cmd_phase_data);
	cmd_phase_data = page >> (32 - (2*8));
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);
	cmd_phase_addr = NAND_BASE;
	target_write_u32(target, cmd_phase_addr, 0x00);

	LOG_INFO("read data from addr %llx", data_phase_addr);
	for (index = 0; index < data_size; ++index, ++data)
	{
		target_read_u32(target, data_phase_addr, data);
		LOG_INFO("page %d index %d read data %x", page, index, *data);
	}
	for (index = 0; index < oob_size; ++index, ++oob)
	{
		target_read_u32(target, data_phase_addr, oob);
		LOG_INFO("page %d index %d read oob %x", page, index, *oob);
	}

	return ERROR_OK;
}

int smc35x_write_page(struct nand_device* nand, uint32_t page, uint8_t* data_p, uint32_t data_size,
			uint8_t* oob_p, uint32_t oob_size)
{
	uint32_t index;
    nand_size_type nand_size;
	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	uint32_t *data = (uint32_t *)data_p;
	uint32_t *oob = (uint32_t *)oob_p;
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	LOG_INFO("please ensure %d page has been erased, otherwise the page will not be modified", page);

    smc35x_read_parameter(nand, &nand_size);
	LOG_INFO("blocksPerUnit:%d pagesPerBlock:%d dataBytesPerPage:%d spareBytesPerPage:%d \r\n",
	nand_size.blocksPerUnit,nand_size.pagesPerBlock,nand_size.dataBytesPerPage,nand_size.spareBytesPerPage);
	
	cmd_phase_addr = NAND_BASE | (ONFI_CMD_PROGRAM_PAGE_CYCLES << 21) | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (ONFI_CMD_PROGRAM_PAGE1 << 3);
	cmd_phase_data = 0 | (page << (2*8));
	data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);
	cmd_phase_data = page >> (32 - (2*8));
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);

	LOG_INFO("write data to addr %llx", data_phase_addr);
	for (index = 0; index < (data_size >> 2); ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, data[index]);
		target_write_u32(target, data_phase_addr, data[index]);
	}

	memset(buf, 0xFF, sizeof(buf));
	oob = (oob == NULL) ? buf : oob;
	oob_size = nand_size.spareBytesPerPage-ONFI_AXI_DATA_WIDTH;
	LOG_INFO("write data to addr %llx", data_phase_addr);
	for (index = 0; index < (oob_size >> 2); ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, oob[index]);
		target_write_u32(target, data_phase_addr, oob[index]);
	}

	oob_size = ONFI_AXI_DATA_WIDTH;
	oob += (nand_size.spareBytesPerPage-ONFI_AXI_DATA_WIDTH);
	data_phase_addr = NAND_BASE | (1 << 21) | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	LOG_INFO("write data to addr %llx", data_phase_addr);
	for (index = 0; index < (oob_size >> 2); ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, oob[index]);
		target_write_u32(target, data_phase_addr, oob[index]);
	}

	return ERROR_OK;
}

static int smc35x_init(struct nand_device* nand)
{
	struct target* target = nand->target;

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

	return ERROR_OK;
}

int smc35x_command_re(struct nand_device* nand, uint8_t startCmd, uint8_t endCmd, uint8_t addrCycles, uint8_t endCmdPhase, int Page, int Column)
{
	struct target* target = nand->target;
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
int smc35x_read_id_re(struct nand_device* nand)
{
	struct target* target = nand->target;
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
	struct nand_device* nand = get_nand_device_by_num(num);
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
	struct nand_device* nand = get_nand_device_by_num(num);
	if (!nand) {
		command_print(CMD, "nand device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	nand_size_type nand_size;
	smc35x_read_parameter(nand, &nand_size);

	LOG_INFO("blocksPerUnit:%x pagesPerBlock:%x dataBytesPerPage:%x spareBytesPerPage:%x \r\n",
		nand_size.blocksPerUnit,nand_size.pagesPerBlock,nand_size.dataBytesPerPage,nand_size.spareBytesPerPage);
	
	LOG_INFO("blocksPerUnit:%d pagesPerBlock:%d dataBytesPerPage:%d spareBytesPerPage:%d \r\n",
		nand_size.blocksPerUnit,nand_size.pagesPerBlock,nand_size.dataBytesPerPage,nand_size.spareBytesPerPage);

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
	.write_data = NULL,
	.read_data = smc35x_read_data,
	.write_page = smc35x_write_page,
	.read_page = smc35x_read_page,
	.nand_ready = NULL,
};
