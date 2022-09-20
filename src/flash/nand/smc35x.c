#include <stdio.h>
#include <stdlib.h>
#include "smc35x.h"

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

typedef struct{
	uint32_t dataBytesPerPage;		/* per page contains data byte numbers*/
	uint16_t spareBytesPerPage;		/* per page contains spare byte numbers*/
	uint32_t pagesPerBlock;			/* per block contains page  numbers*/
	uint32_t blocksPerUnit;			/* per unit contains block numbers*/
	uint8_t totalUnit;				/* total unit numbers*/
}nand_size_type;

struct smc35x_nand_controller{
	int Page;
	int Column;

	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	nand_size_type nand_size;
};

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

int smc35x_commit(struct nand_device* nand)
{
	struct target* target = nand->target;
	struct smc35x_nand_controller* smc35x_info = nand->controller_priv;
	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	uint64_t cmd_addr = 0;
	uint32_t cmd_data = 0;
	cmd_addr  = smc35x_info->cmd_phase_addr;
	cmd_data = smc35x_info->cmd_phase_data;

	LOG_INFO("commit nand command %x data %x\n", cmd_addr, cmd_data);
	target_write_u32(target, cmd_addr, cmd_data);

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
		smc35x_info->data_phase_addr = NAND_BASE | NAND_DATA_PHASE_FLAG;
		break;

	case NAND_CMD_ERASE1:
		smc35x_info->cmd_phase_addr = NAND_BASE | (ONFI_CMD_ERASE_BLOCK_CYCLES << 21) |
		(ONFI_CMD_ERASE_BLOCK_END_TIMING << 20) | (ONFI_CMD_ERASE_BLOCK2 << 11) | (ONFI_CMD_ERASE_BLOCK1 << 3);
		break;
	
	case NAND_CMD_READSTART:
		
		break;

	case NAND_CMD_PAGEPROG:
		
		break;

	case ONFI_CMD_READ_PARAMETER1:

		break;

	default: 
		break;
	}
	smc35x_commit(nand);

	return ERROR_OK;
}

int smc35x_reset(struct nand_device* nand)
{
	// struct target* target = nand->target;

	// if (target->state != TARGET_HALTED) {
	// 	LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
	// 	return ERROR_NAND_OPERATION_FAILED;
	// }

	// target_write_u32(target, (NAND_BASE | (ONFI_CMD_RESET1 << 3)), ONFI_COLUMN_NOT_VALID);

	// return ERROR_OK;

	return smc35x_command(nand, NAND_CMD_RESET);
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

int smc35x_erase(struct nand_device* nand, int bolck)
{
	nand_size_type nand_size;
	struct target* target = nand->target;
	struct smc35x_nand_controller* smc35x_info = nand->controller_priv;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	smc35x_read_parameter(nand, &nand_size);

	smc35x_info->cmd_phase_data = bolck * nand_size.pagesPerBlock;

	smc35x_command(nand, NAND_CMD_ERASE1);

	return ERROR_OK;
}

int smc35x_address(struct nand_device* nand, uint8_t address)
{
	//smc35x_info->cmd_phase_data = address;

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

                                            //    页号
int smc35x_read_page(struct nand_device* nand, uint32_t page, uint8_t* data, uint32_t data_size,
			uint8_t* oob, uint32_t oob_size)
{
	uint32_t index = 0;
	nand_size_type nand_size;
	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}
	
	LOG_INFO("page number %d data size %d oob size %d\n", page, data_size, oob_size);

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

	LOG_INFO("read data from addr %x", data_phase_addr);
	for (index = 0; index < data_size; ++index, ++data)
	{
		target_read_u8(target, data_phase_addr, data);
		LOG_INFO("page %d index %d read data %x", page, index, *data);
	}
	for (index = 0; index < oob_size; ++index, ++oob)
	{
		target_read_u8(target, data_phase_addr, oob);
		LOG_INFO("page %d index %d read oob %x", page, index, *oob);
	}

	return ERROR_OK;
}

int smc35x_write_page(struct nand_device* nand, uint32_t page, uint8_t* data, uint32_t data_size,
			uint8_t* oob, uint32_t oob_size)
{
	uint32_t index, block;
    nand_size_type nand_size;
	uint64_t cmd_phase_addr;
	uint32_t cmd_phase_data;
	uint64_t data_phase_addr;
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	LOG_INFO("please ensure %d page has been erased, otherwise the page will not be modified", page);

    smc35x_read_parameter(nand, &nand_size);
	LOG_INFO("blocksPerUnit:%d pagesPerBlock:%d dataBytesPerPage:%d spareBytesPerPage:%d \r\n",
	nand_size.blocksPerUnit,nand_size.pagesPerBlock,nand_size.dataBytesPerPage,nand_size.spareBytesPerPage);
	
	// block = page / nand_size.pagesPerBlock;
	// smc35x_erase(nand, page / nand_size.pagesPerBlock);
	
	cmd_phase_addr = NAND_BASE | (ONFI_CMD_PROGRAM_PAGE_CYCLES << 21) | (ONFI_CMD_PROGRAM_PAGE2 << 11) | (ONFI_CMD_PROGRAM_PAGE1 << 3);
	cmd_phase_data = 0 | (page << (2*8));
	data_phase_addr = NAND_BASE | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);
	cmd_phase_data = page >> (32 - (2*8));
	target_write_u32(target, cmd_phase_addr, cmd_phase_data);

	LOG_INFO("write data to addr %x", data_phase_addr);
	for (index = 0; index < data_size; ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, data[index]);
		target_write_u8(target, data_phase_addr, data[index]);
	}

	oob = (oob == NULL) ? data : oob;
	oob_size = nand_size.spareBytesPerPage - ONFI_AXI_DATA_WIDTH;
	LOG_INFO("write data to addr %x", data_phase_addr);
	for (index = 0; index < oob_size; ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, oob[index]);
		target_write_u8(target, data_phase_addr, oob[index]);
	}

	oob_size = ONFI_AXI_DATA_WIDTH;
	oob += (nand_size.spareBytesPerPage-ONFI_AXI_DATA_WIDTH);
	data_phase_addr = NAND_BASE | (1 << 21) | (1 << 20) | NAND_DATA_PHASE_FLAG | (ONFI_CMD_PROGRAM_PAGE2 << 11);
	LOG_INFO("write data to addr %x", data_phase_addr);
	for (index = 0; index < oob_size; ++index)
	{
		LOG_INFO("page %d index %d write data %x", page, index, oob[index]);
		target_write_u8(target, data_phase_addr, oob[index]);
	}

	return ERROR_OK;
}

/*int smc35x_read_id(struct nand_device* nand)
{
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
	}

	target_write_u32(target, (NAND_BASE | (ONFI_CMD_READ_ID_CYCLES << 21) | (ONFI_CMD_READ_ID1 << 3)), ONFI_COLUMN_NOT_VALID);

	return ERROR_OK;
}*/
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
			LOG_INFO("smc send cmdPhase %llx dataPhase %x\n", cmdPhaseAddr, cmdPhaseData);
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

	LOG_INFO("smc send cmdPhase %llx dataPhase %x\n", cmdPhaseAddr, cmdPhaseData);

	target_write_u32(target, cmdPhaseAddr, cmdPhaseData);

	return ERROR_OK;
}
int smc35x_reset_re(struct nand_device* nand)
{
	smc35x_command_re(nand, ONFI_CMD_RESET1, ONFI_CMD_RESET2, ONFI_CMD_RESET_CYCLES,
			ONFI_CMD_RESET_END_TIMING, ONFI_PAGE_NOT_VALID, ONFI_COLUMN_NOT_VALID);

	return 0;
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

	LOG_INFO("smc35x_read_id function start");
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

    smc35x_reset_re(nand);
    
	smc35x_command_re(nand, ONFI_CMD_READ_ID1, ONFI_CMD_READ_ID2, ONFI_CMD_READ_ID_CYCLES,ONFI_CMD_READ_ID_END_TIMING, ONFI_PAGE_NOT_VALID, 0x00);
	SmcReadData_re(nand, ONFI_CMD_READ_ID2, ONFI_CMD_READ_ID_END_TIMING, &device_id[0], 5);

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
COMMAND_HANDLER(handle_smc35x_erase_command)
{
    unsigned num, block;

	if (CMD_ARGC < 1 || CMD_ARGC > 2){
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], num);
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[1], block);
	struct nand_device* nand = get_nand_device_by_num(num);
	if (!nand) {
		command_print(CMD, "nand device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	smc35x_erase(nand, block);

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
	{
		.name = "erase",
		.handler = handle_smc35x_erase_command,
		.mode = COMMAND_ANY,
		.help = "select nand id",
		.usage = "nand_id [block number]",
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

/*
static int smc35x_write_data(struct nand_device* nand, uint16_t data)
{
    struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

    volatile uint32_t i=0,j=0;
    Nand_Size_TypeDef nandSize;

    *((volatile uint32_t* )(SMC_BASE+SMC_REG_SET_CYCLES)) =
                    ((0 <<	SMC_SetCycles_Trr_FIELD		)|		//busy to data out
                    (0 <<	SMC_SetCycles_Tar_FIELD   	)|		//ale to data out
                    (0 << 	SMC_SetCycles_Tclr_FIELD 	)|		//cle to data out
                    (1 << 	SMC_SetCycles_Twp_FIELD    	)|	 //we width
                    (1 << 	SMC_SetCycles_Trea_FIELD    )|	//RE before output data
                    (2 <<	SMC_SetCycles_Twc_FIELD     )|	//WE (cmd/addr) width
                    (2 << 	SMC_SetCycles_Trc_FIELD		));	//RE width
    *((volatile uint32_t* )(SMC_BASE+SMC_REG_DIRCT_CMD)) = SMC_DirectCmd_SelChip_Interface1Chip1 | SMC_DirectCmd_CmdType_UpdateRegsAndAXI;

    Onfi_CmdReset();
    Onfi_CmdReadParameter(&nandSize);
    Onfi_CmdBlockErase(0);

    Nand_ProgramPage(0, 0, &data, &nandSize);

	return ERROR_OK;
}

static int smc35x_write_page(struct nand_device* nand,
		uint8_t* data, int data_size)
{
    struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	volatile uint32_t i=0,j=0;
    Nand_Size_TypeDef nandSize;

    *((volatile uint32_t* )(SMC_BASE+SMC_REG_SET_CYCLES)) =
                    ((0 <<	SMC_SetCycles_Trr_FIELD		)|		//busy to data out
                    (0 <<	SMC_SetCycles_Tar_FIELD   	)|		//ale to data out
                    (0 << 	SMC_SetCycles_Tclr_FIELD 	)|		//cle to data out
                    (1 << 	SMC_SetCycles_Twp_FIELD    	)|	 //we width
                    (1 << 	SMC_SetCycles_Trea_FIELD    )|	//RE before output data
                    (2 <<	SMC_SetCycles_Twc_FIELD     )|	//WE (cmd/addr) width
                    (2 << 	SMC_SetCycles_Trc_FIELD		));	//RE width
    *((volatile uint32_t* )(SMC_BASE+SMC_REG_DIRCT_CMD)) = SMC_DirectCmd_SelChip_Interface1Chip1 | SMC_DirectCmd_CmdType_UpdateRegsAndAXI;

    Onfi_CmdReset();
    Onfi_CmdReadParameter(&nandSize);
    Onfi_CmdBlockErase(0);

    Nand_ProgramPage(0, 0, &data, &nandSize);

	return ERROR_OK;
}

static int smc35x_read_page(struct nand_device* nand,
		uint8_t* data, int data_size)
{
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

    volatile uint32_t i=0,j=0;
    Nand_Size_TypeDef nandSize;
	Onfi_CmdReset();
	Onfi_CmdReadParameter(&nandSize);
    
	for (j=0;j<2;j++)
    {
        Nand_ReadPage(j, 0, data, &nandSize);
        printf("read data result:\r\n");
        for (i=0; i<1024*4+224; i++)
        {
            printf("%d ",data[i]);
        }
        printf("\r\n\r\n");
    }

	return ERROR_OK;
}

static int smc35x_nand_ready(struct nand_device* nand, int timeout)
{
	struct target* target = nand->target;
	uint8_t status;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	do {
		target_read_u8(target, SMC35X_NFSTAT, &status);

		if (status & SMC35X_NFSTAT_BUSY)
			return 1;

		alive_sleep(1);
	} while (timeout-- > 0);

	return 0;
}*/

static int smc35x_init(struct nand_device* nand)
{
	struct target* target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	LOG_INFO("smc init");

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

struct nand_flash_controller smc35x_nand_controller = {
	.name = "smc35x",
	.commands = smc35x_command_handler,
	.nand_device_command = smc35x_nand_device_command,
	.init = smc35x_init,
	.erase = smc35x_erase,
	.reset = smc35x_reset,
	.command = smc35x_command,
	.commit = smc35x_commit,
	.address = smc35x_address,
	.write_data = NULL,
	.read_data = smc35x_read_data,
	.write_page = smc35x_write_page,
	.read_page = smc35x_read_page,
	.nand_ready = NULL,
};

