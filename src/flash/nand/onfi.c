#include "onfi.h"

//extern Nand_Size_TypeDef nandSize;

uint32_t NandOob64[12] = {52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
uint32_t NandOob32[6] = {26, 27, 28, 29, 30, 31};
uint32_t NandOob16[3] = {13, 14, 15};


void SmcSendCommand(uint8_t startCmd, uint8_t endCmd, uint8_t addrCycles, uint8_t endCmdPhase, int Page, int Column)
{
	uint32_t endCmdReq = 0;
	volatile uint64_t cmdPhaseAddr  = 0;
	uint32_t cmdPhaseData  = 0;

	if(endCmdPhase == ONFI_ENDIN_CMD_PHASE)
	{
		endCmdReq = 1;
	}

	/*
	 * command phase address
	 */
	cmdPhaseAddr  = NAND_BASE 		|
			(addrCycles << 21)		|
			(endCmdReq 	<< 20)		|
			NAND_COMMAND_PHASE_FLAG	|
			(endCmd 	<< 11)			|
			(startCmd 	<< 3);

	/*
	 * command phase data
	 */
	if ((Column != NAND_COLUMN_NOT_VALID) && (Page != NAND_PAGE_NOT_VALID))
	{
		cmdPhaseData = Column;

		cmdPhaseData |= Page << (3*8);
		if (addrCycles > 4)
		{
			/*
			 * Send lower byte of page address
			 */
			SMC_WriteReg((volatile void *)cmdPhaseAddr, cmdPhaseData);
			/*
			 * build upper byte
			 */
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
	/*
 	 * Send command phase
 	 */
	SMC_WriteReg((volatile void *)cmdPhaseAddr, cmdPhaseData);
}

void SmcReadData(uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length)
{
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
		LOG_ERROR("index %d", Index);
		Buf[Index] = *((uint8_t *)dataPhaseAddr);
	}
}

void SmcReadData_re(struct nand_device *nand, uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length)
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

	LOG_INFO("smc read %llx", dataPhaseAddr);
	
	/* Read Data */
	for(Index = 0;Index < Length;Index++)
	{
		//Buf[Index] = *((uint8_t *)dataPhaseAddr);
		target_read_u8(target, dataPhaseAddr, &Buf[Index]);
		LOG_INFO("read %d data %x", Index, Buf[Index]);
	}

}






void SmcWriteBuf(uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length, uint32_t clearCs, uint32_t eccLast)
{
	uint32_t *tempBuff = (uint32_t *)Buf;
	uint32_t tempLength = Length >> 2;

	uint32_t endCmdReq = 0;
	volatile uint64_t dataPhaseAddr = 0;
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
	for(Index = 0;Index < tempLength;Index++)
	{
		SMC_WriteReg((void *)dataPhaseAddr, tempBuff[Index]);
	}
}


void SmcReadBuf(uint8_t endCmd, uint8_t endCmdPhase, uint8_t *Buf, uint32_t Length, uint32_t clearCs, uint32_t eccLast)
{
	uint32_t endCmdReq = 0;
	volatile uint64_t dataPhaseAddr = 0;
	volatile uint32_t Index = 0;

	uint32_t *tempBuff = (uint32_t *)Buf;
	uint32_t tempLength = Length >> 2;

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
	for(Index = 0;Index < tempLength;Index++)
	{
		tempBuff[Index] = *((volatile uint32_t *)dataPhaseAddr);
	}

}


void Onfi_CmdReset(void)
{
	volatile uint8_t Status,i=0;

	SmcSendCommand(ONFI_CMD_RESET1, ONFI_CMD_RESET2, ONFI_CMD_RESET_CYCLES,
			ONFI_CMD_RESET_END_TIMING, ONFI_PAGE_NOT_VALID, ONFI_COLUMN_NOT_VALID);

	/*
	 * Check the Status Register SR[6]
	 * The device will be busy for a maximum of 1ms.
	 */
	do
	{
		i++;
		Status = Onfi_CmdReadStatus();
	}
	while (!(Status & ONFI_STATUS_RDY));
	printf("iiiii:%d  \r\n",i);

}

uint8_t Onfi_CmdReadStatus(void)
{
	uint8_t Status;
	volatile uint64_t dataPhaseAddr = 0;

	SmcSendCommand(ONFI_CMD_READ_STATUS1, ONFI_CMD_READ_STATUS2, ONFI_CMD_READ_STATUS_CYCLES,
			ONFI_CMD_READ_STATUS_END_TIMING, ONFI_PAGE_NOT_VALID, ONFI_COLUMN_NOT_VALID);

	/* data phase address */
	dataPhaseAddr  = NAND_BASE 		|
			(0 	<< 21)				|
			(0 	<< 20)				|
			NAND_DATA_PHASE_FLAG	|
			(0 	<< 11)				|
			(0 	<< 10);
	Status = *((uint8_t *)dataPhaseAddr);

	return Status;
}

uint8_t Onfi_CmdReadParameter(Nand_Size_TypeDef *nandSize)
{
	uint8_t Status;
	uint32_t Temp=0;
	volatile uint32_t i=0;
	volatile uint64_t cmdPhaseAddr = 0;

	SmcSendCommand(ONFI_CMD_READ_PARAMETER1, ONFI_CMD_READ_PARAMETER2, ONFI_CMD_READ_PARAMETER_CYCLES,
			ONFI_CMD_READ_PARAMETER_END_TIMING, ONFI_PAGE_NOT_VALID, 0x00);

	/* Cheak Nand Status */
	do
	{
		Status = Onfi_CmdReadStatus();
	}
	while (!(Status & ONFI_STATUS_RDY));


	/* re-issue a command value of 00h to start reading data */
	cmdPhaseAddr  = NAND_BASE 		|
			(0 	<< 21)				|
			(0 	<< 20)				|
			NAND_COMMAND_PHASE_FLAG	|
			(0 	<< 11)				|
			(0 	<< 10);

	SMC_WriteReg((void *)cmdPhaseAddr, 0x00);

	for(i=0;i<20;i++)
	{
		SmcReadData(ONFI_CMD_READ_PARAMETER2, ONFI_CMD_READ_PARAMETER_END_TIMING, (uint8_t *)&Temp, 4);
	}

	SmcReadData(ONFI_CMD_READ_PARAMETER2, ONFI_CMD_READ_PARAMETER_END_TIMING, (uint8_t *)(&nandSize->dataBytesPerPage), 4);
	SmcReadData(ONFI_CMD_READ_PARAMETER2, ONFI_CMD_READ_PARAMETER_END_TIMING, (uint8_t *)(&nandSize->spareBytesPerPage), 2);

	for(i=0;i<6;i++)
	{
		SmcReadData(ONFI_CMD_READ_PARAMETER2, ONFI_CMD_READ_PARAMETER_END_TIMING, (uint8_t *)&Temp, 1);
	}

	SmcReadData(ONFI_CMD_READ_PARAMETER2, ONFI_CMD_READ_PARAMETER_END_TIMING, (uint8_t *)(&nandSize->pagesPerBlock), 4);
	SmcReadData(ONFI_CMD_READ_PARAMETER2, ONFI_CMD_READ_PARAMETER_END_TIMING, (uint8_t *)(&nandSize->blocksPerUint), 4);

	for(i=0; i<155; i++)
	{
		SmcReadData(ONFI_CMD_READ_PARAMETER2, ONFI_CMD_READ_PARAMETER_END_TIMING, (uint8_t *)&Temp, 1);
	}

	return SUCCESS_FLAG;
}

void Onfi_CmdReadId(uint8_t Address, uint8_t *Id, uint8_t idSize)
{
	SmcSendCommand(ONFI_CMD_READ_ID1, ONFI_CMD_READ_ID2, ONFI_CMD_READ_ID_CYCLES,ONFI_CMD_READ_ID_END_TIMING, ONFI_PAGE_NOT_VALID, Address);
	SmcReadData(ONFI_CMD_READ_ID2, ONFI_CMD_READ_ID_END_TIMING, Id, idSize);
}


uint8_t Onfi_CmdBlockErase(uint32_t Page)
{
	uint8_t Status;

	Status = Onfi_CmdReadStatus();
	if (!(Status & ONFI_STATUS_WP))
	{
		return NAND_WRITE_PROTECTED;
	}

	SmcSendCommand(ONFI_CMD_ERASE_BLOCK1, ONFI_CMD_ERASE_BLOCK2, ONFI_CMD_ERASE_BLOCK_CYCLES,
			ONFI_CMD_ERASE_BLOCK_END_TIMING, Page, ONFI_COLUMN_NOT_VALID);

	while(Nand_IsBusy() == FAILED_FLAG);

	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;

	Status = Onfi_CmdReadStatus();
	if (Status & ONFI_STATUS_FAIL)
	{
		return FAILED_FLAG;
	}

	return SUCCESS_FLAG;
}



uint8_t Onfi_CmdReadPage(uint32_t Page, uint32_t Column)
{
	// uint8_t Status;

	SmcSendCommand(ONFI_CMD_READ_PAGE1, ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_CYCLES,
				ONFI_CMD_READ_PAGE_END_TIMING, Page, Column);
	//printf("fi\r\n");
	while(Nand_IsBusy() == FAILED_FLAG);

	return SUCCESS_FLAG;

}

uint8_t Onfi_CmdProgramPage(uint32_t Page, uint32_t Column)
{
	uint8_t Status;

	/* Cheak Nand Status */
	Status = Onfi_CmdReadStatus();

	if (!(Status & ONFI_STATUS_WP))
	{
		return FAILED_FLAG;
		printf("fff\r\n");
	}

	SmcSendCommand(ONFI_CMD_PROGRAM_PAGE1, ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_CYCLES,
			ONFI_CMD_PROGRAM_PAGE_END_TIMING, Page, Column);

	return SUCCESS_FLAG;
}







uint8_t Onfi_CmdProgramCachePage(uint32_t Page, uint32_t Column)
{
	uint8_t Status;

	/* Cheak Nand Status */
	Status = Onfi_CmdReadStatus();

	if (!(Status & ONFI_STATUS_WP))
	{
		return FAILED_FLAG;
	}

	SmcSendCommand(ONFI_CMD_PROGRAM_CACHE1, ONFI_CMD_PROGRAM_CACHE2, ONFI_CMD_PROGRAM_CACHE_CYCLES,
			ONFI_CMD_PROGRAM_CACHE_END_TIMING, Page, Column);

	return SUCCESS_FLAG;

}

uint8_t Onfi_CmdReadCachePage(uint32_t Page, uint32_t Column)
{
	// uint8_t Status;
#if 0
	/* Restore the ECC mem command1 and ECC mem command2 register if the previous command is read page cache */
	/* Set SMC_REG_ECC1_MEMCMD0 Reg*/
	SMC_WriteReg((void *)(SMC_BASE + SMC_REG_ECC1_MEMCMD0),
			(SMC_EccMemCmd0_InitWriteCmd| SMC_EccMemCmd0_InitReadCmd| SMC_EccMemCmd0_EndReadCacheCmd| SMC_EccMemCmd0_UseEndCmd));

	SmcSendCommand(ONFI_CMD_READ_PAGE1, ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_CYCLES,
					ONFI_CMD_READ_PAGE_END_TIMING, Page, Column);

	while(Nand_IsBusy() == FAILED_FLAG);

	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((volatile uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;

	/* Cheak Nand Status */
	Status = Onfi_CmdReadStatus();

	if (Status & ONFI_STATUS_FAIL) {
		return FAILED_FLAG;
	}
#endif
	SmcSendCommand(ONFI_CMD_READ_CACHE_ENHANCED1, ONFI_CMD_READ_CACHE_ENHANCED2, ONFI_CMD_READ_CACHE_ENHANCED_CYCLES,
					ONFI_CMD_READ_CACHE_ENHANCED_END_TIMING, Page, Column);

	while(Nand_IsBusy() == FAILED_FLAG);

	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((volatile uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;


	return SUCCESS_FLAG;
}


uint8_t Onfi_CmdReadCachePageEnd(uint32_t Page, uint32_t Column)
{
	// uint8_t Status;

	/* Restore the ECC mem command1 and ECC mem command2 register if the previous command is read page cache */
	/* Set SMC_REG_ECC1_MEMCMD0 Reg*/
	SMC_WriteReg((void *)(SMC_BASE + SMC_REG_ECC1_MEMCMD0),
			(SMC_EccMemCmd0_InitWriteCmd| SMC_EccMemCmd0_InitReadChangeColumnCmd
					| SMC_EccMemCmd0_EndReadChangeColumnCmd| SMC_EccMemCmd0_UseEndCmd));

	SmcSendCommand(ONFI_CMD_READ_CACHE_END1, ONFI_CMD_READ_CACHE_END2, ONFI_CMD_READ_CACHE_END_CYCLES,
			ONFI_CMD_READ_CACHE_END_TIMING, ONFI_PAGE_NOT_VALID, ONFI_COLUMN_NOT_VALID);

	while(Nand_IsBusy() == FAILED_FLAG);

	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((volatile uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;

	return SUCCESS_FLAG;
}




uint8_t Nand_IsBusy(void)
{
	uint8_t Status;
	Status = (uint8_t)(  *((volatile uint32_t *)(SMC_BASE+SMC_REG_MEMC_STATUS))  & (1 << SMC_MemcStatus_SmcInt1RawStatus_FIELD));
	//printf("bb\r\n");
	if(Status)
		return SUCCESS_FLAG;
	else
		return FAILED_FLAG;
}







uint8_t Nand_ProgramPage(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize)
{
	uint8_t Status;

	Nand_EccHwDisable();



	//Onfi_CmdProgramPage( Page,  Column);

	printf("2\r\n");
	SmcWriteBuf(ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf,
			nandSize->dataBytesPerPage, NO_CLEAR_CS, NO_ECC_LAST);

	Buf += (nandSize->dataBytesPerPage);

	SmcWriteBuf(ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf,
			nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, NO_ECC_LAST);


	Buf += (nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH);

	SmcWriteBuf( ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf,
			ONFI_AXI_DATA_WIDTH, CLEAR_CS, NO_ECC_LAST);

	while(Nand_IsBusy() == FAILED_FLAG);
	printf("3\r\n");
	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;

	/* Cheak Nand Status */
	Status = Onfi_CmdReadStatus();

	if(Status & ONFI_STATUS_FAIL) {
		return FAILED_FLAG;
		printf("fff\r\n");
	}
	printf("4\r\n");
	return SUCCESS_FLAG;
}


uint8_t Nand_ReadPage(uint32_t Page, uint32_t Column, uint8_t *Buf,Nand_Size_TypeDef *nandSize)
{
	volatile uint64_t cmdPhaseAddr;
	uint8_t Status;

	/* Restore the ECC mem command1 and ECC mem command2 register if the previous command is read page cache */
	/* Set SMC_REG_ECC1_MEMCMD0 Reg*/
	SMC_WriteReg((void *)(SMC_BASE + SMC_REG_ECC1_MEMCMD0),
			(SMC_EccMemCmd0_InitWriteCmd| SMC_EccMemCmd0_InitReadCmd| SMC_EccMemCmd0_EndReadCmd| SMC_EccMemCmd0_UseEndCmd));

	/* Set SMC_REG_ECC1_MEMCMD1 Reg */
	SMC_WriteReg((void *)(SMC_BASE + SMC_REG_ECC1_MEMCMD1),
			(SMC_EccMemCmd1_ColChangeWRCmd| SMC_EccMemCmd1_ColChangeRDCmd| SMC_EccMemCmd1_ColChangeEndCmd| SMC_EccMemCmd1_UseEndCmd));


	Onfi_CmdReadPage(Page, Column);

	while(Nand_IsBusy() == FAILED_FLAG);

	printf("5\r\n");
	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((volatile uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;
	printf("5\r\n");
	/* Cheak Nand Status */
	Status = Onfi_CmdReadStatus();

	if (Status & ONFI_STATUS_FAIL) {
		return FAILED_FLAG;
	}
	printf("5\r\n");
	/* re-issue a command value of 00h to start reading data */
	cmdPhaseAddr  = NAND_BASE 		|
			(0 	<< 21)				|
			(0 	<< 20)				|
			NAND_COMMAND_PHASE_FLAG	|
			(0 	<< 11)				|
			(0 	<< 10);

	SMC_WriteReg((volatile void *)cmdPhaseAddr, 0x00);
	printf("5\r\n");

	/* Read data Page */
	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, nandSize->dataBytesPerPage, NO_CLEAR_CS, NO_ECC_LAST);
	printf("5\r\n");
	Buf += (nandSize->dataBytesPerPage);
	printf("5\r\n");
	/* Read Spare data */
	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, NO_ECC_LAST);
	Buf += (nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH);

	printf("6\r\n");
	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, ONFI_AXI_DATA_WIDTH, CLEAR_CS, NO_ECC_LAST);
	/* keep ONFI_AXI_DATA_WIDTH bytes read to clear cs */
	return SUCCESS_FLAG;
}

uint8_t Nand_ReadSpareBytes(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize)
{
	uint64_t cmdPhaseAddr;
	uint8_t Status;

	/* Disbale SMC control ecc*/
	Nand_EccHwDisable();

	Column |= nandSize->dataBytesPerPage;

	/* Read Current page data bytes */
	Onfi_CmdReadPage(Page,  Column);

	while(Nand_IsBusy() == FAILED_FLAG);

	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((volatile uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;

	/* Cheak Nand Status */
	Status = Onfi_CmdReadStatus();

	if(Status & ONFI_STATUS_FAIL) {
		return FAILED_FLAG;
	}

	/* re-issue a command value of 00h to start reading data */
	cmdPhaseAddr  = NAND_BASE 		|
			(0 	<< 21)				|
			(0 	<< 20)				|
			NAND_COMMAND_PHASE_FLAG	|
			(0 	<< 11)				|
			(0 	<< 10);

	SMC_WriteReg((volatile void *)cmdPhaseAddr, 0x00);

	/* Read Spare data */
	/* keep ONFI_AXI_DATA_WIDTH bytes read to clear cs */
	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, NO_ECC_LAST);

	Buf += (nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH);
	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, ONFI_AXI_DATA_WIDTH, CLEAR_CS, NO_ECC_LAST);

	return SUCCESS_FLAG;
}



uint8_t Nand_CheakIsBadBlock(uint32_t Page, uint32_t Column, uint8_t *Buf,Nand_Size_TypeDef *nandSize)
{
	uint8_t Status;
	// volatile uint32_t i=0;

	Status = Nand_ReadSpareBytes(Page, Column, Buf, nandSize);

	if(Status == FAILED_FLAG)
	{
		return FAILED_FLAG;
	}
	if((Buf[0] == NANDP_BB_FLAG)&&(Buf[1] == NANDP_BB_FLAG))
		return GOOD_BLOCK;
	else
	{
		/*
		for(i=0; i<225; i++)
		{
			printf("%d ",Buf[i]);

		}printf("\r\n");
		*/
		return BAD_BLOCK;
	}
}


uint8_t Nand_HwCalculateEcc(uint8_t *Dst)
{
	uint8_t Status,Count;
	volatile uint8_t eccReg;
	uint32_t eccValue;

	/* Cheak busy signal if it is busy to poll*/
	do{
		Status = (*((volatile uint32_t *)(SMC_BASE+SMC_REG_ECC1_STATUS))) & (1 << SMC_EccStatus_EccStatus_FIELD);
	}
	while(Status);
	printf("cal1\r\n");
	for(eccReg=0; eccReg < 4; eccReg++)
	{
		eccValue = SMC_ReadReg(SMC_BASE+SMC_REG_ECC1_BLOCK0+eccReg*4);
		printf("eccValue:%x\r\n",eccValue);
		printf("cal2\r\n");
		if((eccValue) & (1 << SMC_EccBlock_ISCheakValueValid_FIELD))
		{
			for(Count=0; Count < 3; Count++)
			{
				*Dst = eccValue & 0xFF;
				printf("Dst: %d\r\n",*Dst);
				eccValue = eccValue >> 8;
				Dst++;
			}
		}
		else
		{
			return FAILED_FLAG;
		}
	}
	printf("cal3\r\n");
	//printf("\r\n");
	return SUCCESS_FLAG;
}


/* Variable Definitions */
/* ECC data position in the spare data area  for different page sizes */
	/* Ecc offset for 16 bytes spare area */
	/* Ecc offset for 32 bytes spare area */
/* Ecc offset for 64 bytes spare area */

uint8_t Nand_ProgramPage_HwEcc(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize)
{
	uint8_t Status,i,eccDataNums;
	uint8_t eccData[12]={0};
	uint32_t *dataOffsetPtr;
	uint8_t *TempBuf=Buf;
	/* Cheak Nand Status */
	Status = Onfi_CmdReadStatus();

	if (!(Status & ONFI_STATUS_WP))
	{
		return FAILED_FLAG;
	}

	Onfi_CmdProgramPage( Page,  Column);

	/* Write dataBytesPerPage*/
	SmcWriteBuf(ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf, nandSize->dataBytesPerPage-ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, NO_ECC_LAST);
	Buf += (nandSize->dataBytesPerPage-ONFI_AXI_DATA_WIDTH);

	SmcWriteBuf(ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf, ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, ECC_LAST);
	Buf += ONFI_AXI_DATA_WIDTH;

	/* SmcWriteBuf(ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf, 2048-ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, NO_ECC_LAST);
	Buf += (2048-ONFI_AXI_DATA_WIDTH);

	SmcWriteBuf(ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf, ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, ECC_LAST);
	Buf += ONFI_AXI_DATA_WIDTH;*/
	switch(nandSize->spareBytesPerPage)
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


	printf("cal\r\n");
	/* Calculate Ecc and Fill Spare data */
	Nand_HwCalculateEcc(eccData);

	for(i = 0; i < eccDataNums; i++) {
		TempBuf[dataOffsetPtr[i]+nandSize->dataBytesPerPage] = ~(eccData[i]);
	}

	SmcWriteBuf(ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf, nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, NO_ECC_LAST);
	Buf += (nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH);

	SmcWriteBuf( ONFI_CMD_PROGRAM_PAGE2, ONFI_CMD_PROGRAM_PAGE_END_TIMING, Buf, ONFI_AXI_DATA_WIDTH, CLEAR_CS, NO_ECC_LAST);

	while(Nand_IsBusy() == FAILED_FLAG);

	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((volatile uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;

	/* Cheak Nand Status */
	Status = Onfi_CmdReadStatus();

	if(Status & ONFI_STATUS_FAIL) {
		return FAILED_FLAG;
	}

	return SUCCESS_FLAG;
}




uint8_t Nand_ReadPage_HwEcc(uint32_t Page, uint32_t Column, uint8_t *Buf, Nand_Size_TypeDef *nandSize)
{
	//uint64_t cmdPhaseAddr;
	uint8_t Status = 0, i = 0, eccDataNums = 0, EccOffset = 0;
	uint8_t eccData[12] = {0};
	uint8_t ReadEccData[12] = {0};
	uint32_t *dataOffsetPtr = NULL;
	uint8_t *TempBuf=Buf;


	/* Restore the ECC mem command1 and ECC mem command2 register if the previous command is read page cache */
	/* Set SMC_REG_ECC1_MEMCMD0 Reg*/
	SMC_WriteReg((void *)(SMC_BASE + SMC_REG_ECC1_MEMCMD0),
			(SMC_EccMemCmd0_InitWriteCmd| SMC_EccMemCmd0_InitReadCmd| SMC_EccMemCmd0_EndReadCmd| SMC_EccMemCmd0_UseEndCmd));

	/* Set SMC_REG_ECC1_MEMCMD1 Reg*/
	SMC_WriteReg((void *)(SMC_BASE + SMC_REG_ECC1_MEMCMD1),
			(SMC_EccMemCmd1_ColChangeWRCmd| SMC_EccMemCmd1_ColChangeRDCmd| SMC_EccMemCmd1_ColChangeEndCmd| SMC_EccMemCmd1_UseEndCmd));

	Onfi_CmdReadPage(Page, Column);

	while(Nand_IsBusy() == FAILED_FLAG);

	/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
	*((volatile uint32_t *)(SMC_BASE+SMC_REG_MEM_CFG_CLR)) |= SMC_MemCfgClr_ClrSmcInt1;




	/* Read data Page */
	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, nandSize->dataBytesPerPage-ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, NO_ECC_LAST);
	Buf += (nandSize->dataBytesPerPage-ONFI_AXI_DATA_WIDTH);

	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, ECC_LAST);
	Buf += ONFI_AXI_DATA_WIDTH;

	/* Calculate Ecc */
	Nand_HwCalculateEcc(eccData);

	/* Read Spare data */
	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH, NO_CLEAR_CS, NO_ECC_LAST);
	Buf += (nandSize->spareBytesPerPage-ONFI_AXI_DATA_WIDTH);


	SmcReadBuf(ONFI_CMD_READ_PAGE2, ONFI_CMD_READ_PAGE_END_TIMING, Buf, ONFI_AXI_DATA_WIDTH, CLEAR_CS, NO_ECC_LAST);

	switch(nandSize->spareBytesPerPage)
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
			return FAILED_FLAG;
	}

	for(i = 0; i < eccDataNums; i++)
	{
		ReadEccData[i] = ~TempBuf[dataOffsetPtr[i]+nandSize->dataBytesPerPage];

		/*
		if(TempBuf[dataOffsetPtr[i]+nandSize->dataBytesPerPage] == (~eccData[i]))
		{
			//Nand_HwCorrectEcc();
		}*/
	}

	i = nandSize->dataBytesPerPage/NAND_ECC_BLOCK_SIZE;
	for(;i;i--)
	{
		Status = Nand_HwCorrectEcc(&ReadEccData[EccOffset], &eccData[EccOffset], TempBuf);
		if(Status != SUCCESS_FLAG){
			return Status;
		}

		EccOffset += NAND_ECC_BYTES;
		TempBuf += NAND_ECC_BLOCK_SIZE;
	}

	/* keep ONFI_AXI_DATA_WIDTH bytes read to clear cs */
	return SUCCESS_FLAG;

}



uint8_t Nand_HwCorrectEcc(uint8_t *eccCode, uint8_t *eccCalc, uint8_t *buf)
{
	uint8_t bitPos;
	uint32_t bytePos;
	uint16_t eccOdd, eccEven;
	uint16_t readEccLow, readEccHigh;
	uint16_t calcEccLow, calcEccHigh;

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
		return SUCCESS_FLAG;
	}


	/* One bit Error */
	if (eccOdd == (~eccEven & 0xfff))
	{
		bytePos = (eccOdd >> 3) & 0x1ff;
		bitPos = eccOdd & 0x07;
		/* Toggling error bit */
		buf[bytePos] ^= (1 << bitPos);
		printf("hello\r\n");
		return SUCCESS_FLAG;
	}

	/* Two bits Error */
	if (OneHot((eccOdd | eccEven)) == SUCCESS_FLAG)
	{
		return SUCCESS_FLAG;
	}


	/* Multiple bits error */
	return FAILED_FLAG;

}






uint8_t Nand_EccHwInit(Nand_Size_TypeDef *nandSize)
{
	uint32_t ecc1Config;

	/* Set SMC_REG_ECC1_MEMCMD0 Reg*/
	SMC_WriteReg((void volatile *)(SMC_BASE + SMC_REG_ECC1_MEMCMD0),
			(SMC_EccMemCmd0_InitWriteCmd| SMC_EccMemCmd0_InitReadCmd|
					SMC_EccMemCmd0_EndReadCmd| SMC_EccMemCmd0_UseEndCmd));

	/* Set SMC_REG_ECC1_MEMCMD1 Reg*/
	SMC_WriteReg((void volatile *)(SMC_BASE + SMC_REG_ECC1_MEMCMD1),
			(SMC_EccMemCmd1_ColChangeWRCmd| SMC_EccMemCmd1_ColChangeRDCmd|
				SMC_EccMemCmd1_ColChangeEndCmd| SMC_EccMemCmd1_UseEndCmd));

	/* Cheak Ecc Busy */
	//while(SMC_ReadReg((void volatile *)(SMC_BASE + SMC_REG_ECC1_STATUS)) && SMC_EccStatus_EccBusy);

	switch(nandSize->dataBytesPerPage)
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
		case(4096):
				ecc1Config = SMC_EccCfg_Four_PageSize;
				break;
		default:
			/* Page size 256 bytes & 4096 bytes not supported by ECC block */
			return FAILED_FLAG;
	}

	/* Cheak Ecc Busy */
	while(SMC_ReadReg((volatile void *)(SMC_BASE + SMC_REG_ECC1_STATUS)) & SMC_EccStatus_EccBusy);

	//*((uint32_t *)(SMC_BASE + SMC_REG_ECC1_STATUS)) |=
	SMC_WriteReg((void volatile *)(SMC_BASE + SMC_REG_ECC1_CFG),
			(SMC_EccCfg_EccModeMem| SMC_EccCfg_EccValue_InPageEnd| SMC_EccCfg_EccNoJump| ecc1Config));
	return SUCCESS_FLAG;
}


void Nand_EccHwDisable(void)
{
	/* Cheak Ecc Busy */
	while(SMC_ReadReg((volatile void *)(SMC_BASE + SMC_REG_ECC1_STATUS)) & SMC_EccStatus_EccBusy);

	/* Set Ecc Model is bypass */
	*((volatile uint32_t *)(SMC_BASE+SMC_REG_ECC1_CFG)) &= ~SMC_EccCfg_EccModeMem;
}


/* Only Micron */
uint8_t Nand_EnableOnDieEcc(void)
{
	uint8_t EccSetFeature[4] = {0x08, 0x00, 0x00, 0x00};
	uint8_t EccGetFeature[4] = {0};

	Onfi_CmdSetFeature(0x90, &EccSetFeature[0]);
	Onfi_CmdGetFeature(0x90, &EccGetFeature[0]);

	if(EccGetFeature[0] == 0x08)
	{
		return SUCCESS_FLAG;
	}
	else
	{
		return FAILED_FLAG;
	}
}






void Onfi_CmdSetFeature(uint8_t Address, uint8_t *Value)
{
	uint8_t Status;

	SmcSendCommand(ONFI_CMD_SET_FEATURES1, ONFI_CMD_SET_FEATURES2, ONFI_CMD_SET_FEATURES_CYCLES,
			ONFI_CMD_SET_FEATURES_END_TIMING, ONFI_PAGE_NOT_VALID, Address);

	SmcWriteBuf(ONFI_CMD_SET_FEATURES2, ONFI_CMD_SET_FEATURES_CYCLES, Value,
			4, NO_CLEAR_CS, NO_ECC_LAST);

	/* Cheak Nand Status */
	do
	{
		Status = Onfi_CmdReadStatus();
	}
	while (!(Status & ONFI_STATUS_RDY));


}

void Onfi_CmdGetFeature(uint8_t Address, uint8_t *Value)
{
	uint8_t Status;
	uint64_t cmdPhaseAddr;
	SmcSendCommand(ONFI_CMD_GET_FEATURES1, ONFI_CMD_GET_FEATURES2, ONFI_CMD_GET_FEATURES_CYCLES,
			ONFI_CMD_GET_FEATURES_END_TIMING, ONFI_PAGE_NOT_VALID, Address);

	/* Cheak Nand Status */
	do
	{
		Status = Onfi_CmdReadStatus();
	}
	while (!(Status & ONFI_STATUS_RDY));

	/* re-issue a command value of 00h to start reading data */
	cmdPhaseAddr  = NAND_BASE 		|
			(0 	<< 21)				|
			(0 	<< 20)				|
			NAND_COMMAND_PHASE_FLAG	|
			(0 	<< 11)				|
			(0 	<< 10);

	SMC_WriteReg((volatile void *)cmdPhaseAddr, 0x00);

	SmcReadData(ONFI_CMD_GET_FEATURES2, ONFI_CMD_GET_FEATURES_CYCLES, Value, 4);
}




/*
 *进行必要的nand controller初始化，driver初始化，nand chip配置流程；
 *进行一次坏块扫描，后续读操作要能自动跳过坏块
 *通过NandParams结构体指针返回Nand ID、Size信息
 *函数返回成功或错误类型
 */
uint32_t Csu_NandInit(void)
{

	return 0;

}

/*
 *按照offset和length读取nand数据；
 *能够根据offset计算block位置
 *返回成功或错误类型
 */
uint32_t Csu_NandRead(uint32_t offset, uint8_t* dest, uint32_t length)
{
	// uint32_t tempNumPages;

	/*
	tempNumPages = offset/(nandSize.dataBytesPerPage);

	Nand_ReadPage(tempNumPages, 0, dest, &nandSize);
*/
	return 0;

}


