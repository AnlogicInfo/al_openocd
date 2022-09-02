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

#define TEST ((test *)(SMC_BASE+SMC_REG_ECC1_STATUS))

volatile int count=0;

struct smc35x_nand_controller {
    uint32_t smc_base;

    uint32_t pin_mux_base;
	uint32_t cmd;
	uint32_t data;
};

NAND_DEVICE_COMMAND_HANDLER(smc35x_nand_device_command)
{
	struct smc35x_nand_controller *smc35x_info;

	smc35x_info = calloc(1, sizeof(struct smc35x_nand_controller));
	if (!smc35x_info) {
		LOG_ERROR("no memory for nand controller");
		return ERROR_NAND_DEVICE_INVALID;
	}

	/* fill in the address fields for the core device */
	nand->controller_priv = smc35x_info;
    smc35x_info->smc_base = SMC_BASE;
    smc35x_info->pin_mux_base = PIN_MUX_BASE;

	return ERROR_OK;
}

int smc35x_commit(struct nand_device* nand)
{
	struct target *target = nand->target;
	struct smc35x_nand_controller  *smc35x_info = nand->controller_priv;
	uint32_t cmd_phase = 0, data_phase = 0;

	cmd_phase  = smc35x_info->cmd;
	data_phase = smc35x_info->data;

	if (target->state != TARGET_HALTED)
		return ERROR_NAND_OPERATION_FAILED;

	LOG_INFO("commit nand command %x data %x", cmd_phase, data_phase);
	target_write_u32(target, cmd_phase, data_phase);

	return ERROR_OK;

}

int smc35x_command(struct nand_device *nand, uint8_t command)
{
	LOG_INFO("smc send cmd %x", command);
	// struct smc35x_nand_controller  *smc35x_info = nand->controller_priv;
	// struct target *target = nand->target;

	// create cmdphase 
	// if(command == ) // start cmd?
	// {


	// }

	// else{    // end cmd


	// }


	return ERROR_OK;
}




int smc35x_command_re(struct nand_device *nand, uint8_t startCmd, uint8_t endCmd, uint8_t addrCycles, uint8_t endCmdPhase, int Page, int Column)
{
	// struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	struct target *target = nand->target;
	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	uint32_t endCmdReq = 0, cmdPhaseData  = 0;
	volatile uint64_t cmdPhaseAddr  = 0;
	if(endCmdPhase == ONFI_ENDIN_CMD_PHASE)
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
			//SMC_WriteReg((volatile void *)cmdPhaseAddr, cmdPhaseData);
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

	//SMC_WriteReg((volatile void *)cmdPhaseAddr, cmdPhaseData);
	target_write_u32(target, cmdPhaseAddr, cmdPhaseData);
	
	//或者
	//target_write_u8(target, hw->cmd, command);

	return ERROR_OK;
}

int smc35x_reset(struct nand_device *nand)
{
	return ERROR_OK;
}
int smc35x_reset_re(struct nand_device *nand)
{
	/*
	struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	target_write_u32(target, smc35x_info->cmd, 0xff);

	return ERROR_OK;
	*/
    //或者
	//return smc35x_command(nand, NAND_CMD_RESET);
	smc35x_command_re(nand, ONFI_CMD_RESET1, ONFI_CMD_RESET2, ONFI_CMD_RESET_CYCLES,
			ONFI_CMD_RESET_END_TIMING, ONFI_PAGE_NOT_VALID, ONFI_COLUMN_NOT_VALID);

	return 0;
}

int smc35x_read_id(struct nand_device *nand)
{
	// struct smc35x_nand_controller *smc35x_info = nand->controller_priv;
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

	LOG_INFO("smc35x_read_id function start");
	/*SMC_WriteReg(PS_MIO0, 0x02);
	//SMC_WriteReg(PS_MIO1, 0x02);
	SMC_WriteReg(PS_MIO2, 0x02);
	SMC_WriteReg(PS_MIO3, 0x02);
	SMC_WriteReg(PS_MIO4, 0x02);
	SMC_WriteReg(PS_MIO5, 0x02);
	SMC_WriteReg(PS_MIO6, 0x02);
	SMC_WriteReg(PS_MIO7, 0x02);
	SMC_WriteReg(PS_MIO8, 0x02);
	SMC_WriteReg(PS_MIO9, 0x02);
	SMC_WriteReg(PS_MIO10, 0x02);
	SMC_WriteReg(PS_MIO11, 0x02);
	SMC_WriteReg(PS_MIO12, 0x02);
	SMC_WriteReg(PS_MIO13, 0x02);
	SMC_WriteReg(PS_MIO14, 0x02);*/

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

    /*((volatile uint32_t *)(SMC_BASE+SMC_REG_SET_CYCLES)) =
            ((2 <<	SMC_SetCycles_Trr_FIELD		)|		//busy to data out
            (1 <<	SMC_SetCycles_Tar_FIELD   	)|		//ale to data out
            (1 << 	SMC_SetCycles_Tclr_FIELD 	)|		//cle to data out
            (2 << 	SMC_SetCycles_Twp_FIELD    	)|	 //we width
            (1 << 	SMC_SetCycles_Trea_FIELD    )|	//RE before output data
            (3 <<	SMC_SetCycles_Twc_FIELD     )|	//WE (cmd/addr) width
            (3 << 	SMC_SetCycles_Trc_FIELD		));	//RE width*/
	target_write_u32(target, (SMC_BASE+SMC_REG_SET_CYCLES),
	        (2 <<	SMC_SetCycles_Trr_FIELD		)|		//busy to data out
            (1 <<	SMC_SetCycles_Tar_FIELD   	)|		//ale to data out
            (1 << 	SMC_SetCycles_Tclr_FIELD 	)|		//cle to data out
            (2 << 	SMC_SetCycles_Twp_FIELD    	)|	 //we width
            (1 << 	SMC_SetCycles_Trea_FIELD    )|	//RE before output data
            (3 <<	SMC_SetCycles_Twc_FIELD     )|	//WE (cmd/addr) width
            (3 << 	SMC_SetCycles_Trc_FIELD		));

	//Onfi_CmdReset();
    smc35x_reset_re(nand);
    
    //Onfi_CmdReadId(0x00, &device_id[0], 5);
	smc35x_command_re(nand, ONFI_CMD_READ_ID1, ONFI_CMD_READ_ID2, ONFI_CMD_READ_ID_CYCLES,ONFI_CMD_READ_ID_END_TIMING, ONFI_PAGE_NOT_VALID, 0x00);
	SmcReadData_re(nand, ONFI_CMD_READ_ID2, ONFI_CMD_READ_ID_END_TIMING, &device_id[0], 5);

    printf("%x %x %x %x %x\r\n",device_id[0],device_id[1],device_id[2],device_id[3],device_id[4]);
	LOG_ERROR("%x %x %x %x %x\r\n",device_id[0],device_id[1],device_id[2],device_id[3],device_id[4]);

    return ERROR_OK;
}

COMMAND_HANDLER(handle_smc35x_id_command)
{
	// struct smc35x_nand_controller *smc35x_info = NULL;
	
    unsigned num;
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], num);
	struct nand_device *nand = get_nand_device_by_num(num);
	if (!nand) {
		command_print(CMD, "nand device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	// smc35x_info = nand->controller_priv;

	smc35x_read_id(nand);

	return ERROR_OK;
}
static const struct command_registration smc35x_exec_command_handlers[] = {
	{
		.name = "id",
		.handler = handle_smc35x_id_command,
		.mode = COMMAND_ANY,
		.help = "select nand id",
		.usage = "nand_id ['mlc'|'slc' ['bulk'] ]",
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
static int smc35x_write_data(struct nand_device *nand, uint16_t data)
{
    struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

    volatile uint32_t i=0,j=0;
    Nand_Size_TypeDef nandSize;

    *((volatile uint32_t *)(SMC_BASE+SMC_REG_SET_CYCLES)) =
                    ((0 <<	SMC_SetCycles_Trr_FIELD		)|		//busy to data out
                    (0 <<	SMC_SetCycles_Tar_FIELD   	)|		//ale to data out
                    (0 << 	SMC_SetCycles_Tclr_FIELD 	)|		//cle to data out
                    (1 << 	SMC_SetCycles_Twp_FIELD    	)|	 //we width
                    (1 << 	SMC_SetCycles_Trea_FIELD    )|	//RE before output data
                    (2 <<	SMC_SetCycles_Twc_FIELD     )|	//WE (cmd/addr) width
                    (2 << 	SMC_SetCycles_Trc_FIELD		));	//RE width
    *((volatile uint32_t *)(SMC_BASE+SMC_REG_DIRCT_CMD)) = SMC_DirectCmd_SelChip_Interface1Chip1 | SMC_DirectCmd_CmdType_UpdateRegsAndAXI;

    Onfi_CmdReset();
    Onfi_CmdReadParameter(&nandSize);
    Onfi_CmdBlockErase(0);

    Nand_ProgramPage(0, 0, &data, &nandSize);

	return ERROR_OK;
}

static int smc35x_write_page(struct nand_device *nand,
		uint8_t *data, int data_size)
{
    struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

	volatile uint32_t i=0,j=0;
    Nand_Size_TypeDef nandSize;

    *((volatile uint32_t *)(SMC_BASE+SMC_REG_SET_CYCLES)) =
                    ((0 <<	SMC_SetCycles_Trr_FIELD		)|		//busy to data out
                    (0 <<	SMC_SetCycles_Tar_FIELD   	)|		//ale to data out
                    (0 << 	SMC_SetCycles_Tclr_FIELD 	)|		//cle to data out
                    (1 << 	SMC_SetCycles_Twp_FIELD    	)|	 //we width
                    (1 << 	SMC_SetCycles_Trea_FIELD    )|	//RE before output data
                    (2 <<	SMC_SetCycles_Twc_FIELD     )|	//WE (cmd/addr) width
                    (2 << 	SMC_SetCycles_Trc_FIELD		));	//RE width
    *((volatile uint32_t *)(SMC_BASE+SMC_REG_DIRCT_CMD)) = SMC_DirectCmd_SelChip_Interface1Chip1 | SMC_DirectCmd_CmdType_UpdateRegsAndAXI;

    Onfi_CmdReset();
    Onfi_CmdReadParameter(&nandSize);
    Onfi_CmdBlockErase(0);

    Nand_ProgramPage(0, 0, &data, &nandSize);

	return ERROR_OK;
}

static int smc35x_read_data(struct nand_device *nand, void *data)
{
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

    uint8_t buf[5000];
    volatile uint32_t i=0,j=0;
    Nand_Size_TypeDef nandSize;
	Onfi_CmdReset();
	Onfi_CmdReadParameter(&nandSize);
    
	for(j=0;j<2;j++)
    {
        Nand_ReadPage(j, 0, buf, &nandSize);
        *((int*)data) = buf[0];
        printf("read data result:\r\n");
        for(i=0; i<1024*4+224; i++)
        {
            printf("%d ",buf[i]);
        }
        printf("\r\n\r\n");
    }

	return ERROR_OK;
}

static int smc35x_read_page(struct nand_device *nand,
		uint8_t *data, int data_size)
{
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use SMC35X NAND flash controller");
		return ERROR_NAND_OPERATION_FAILED;
	}

    volatile uint32_t i=0,j=0;
    Nand_Size_TypeDef nandSize;
	Onfi_CmdReset();
	Onfi_CmdReadParameter(&nandSize);
    
	for(j=0;j<2;j++)
    {
        Nand_ReadPage(j, 0, data, &nandSize);
        printf("read data result:\r\n");
        for(i=0; i<1024*4+224; i++)
        {
            printf("%d ",data[i]);
        }
        printf("\r\n\r\n");
    }

	return ERROR_OK;
}

static int smc35x_nand_ready(struct nand_device *nand, int timeout)
{
	struct target *target = nand->target;
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

static int smc35x_init(struct nand_device *nand)
{
	struct target *target = nand->target;

	if (target->state != TARGET_HALTED) {
		LOG_ERROR("target must be halted to use NAND flash controller");
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
	.reset = NULL,
	.command = smc35x_command,
	.commit = smc35x_commit,
	.address = NULL,
	.write_data = NULL,
	.read_data = NULL,
	.write_page = NULL,
	.read_page = NULL,
	.nand_ready = NULL,
};

