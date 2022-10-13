#ifndef OPENOCD_FLASH_NAND_SMC35X_H
#define OPENOCD_FLASH_NAND_SMC35X_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define SMC_BASE                  	(0xF841A000UL)
#define NAND_BASE                   (0x64000000UL)
#define PIN_MUX_BASE	            (0xF8803000UL)

#define PS_MIO0			(0x00+PIN_MUX_BASE)
#define PS_MIO1			(0x04+PIN_MUX_BASE)
#define PS_MIO2			(0x08+PIN_MUX_BASE)
#define PS_MIO3			(0x0c+PIN_MUX_BASE)
#define PS_MIO4			(0x10+PIN_MUX_BASE)
#define PS_MIO5			(0x14+PIN_MUX_BASE)
#define PS_MIO6			(0x18+PIN_MUX_BASE)
#define PS_MIO7			(0x1c+PIN_MUX_BASE)
#define PS_MIO8			(0x20+PIN_MUX_BASE)
#define PS_MIO9			(0x24+PIN_MUX_BASE)
#define PS_MIO10		(0x28+PIN_MUX_BASE)
#define PS_MIO11		(0x2c+PIN_MUX_BASE)
#define PS_MIO12		(0x30+PIN_MUX_BASE)
#define PS_MIO13		(0x34+PIN_MUX_BASE)
#define PS_MIO14		(0x38+PIN_MUX_BASE)
#define PS_MIO15		(0x3c+PIN_MUX_BASE)
#define PS_MIO16		(0x40+PIN_MUX_BASE)
#define PS_MIO17		(0x44+PIN_MUX_BASE)
#define PS_MIO18		(0x48+PIN_MUX_BASE)
#define PS_MIO19		(0x4c+PIN_MUX_BASE)
#define PS_MIO20		(0x50+PIN_MUX_BASE)
#define PS_MIO21		(0x54+PIN_MUX_BASE)
#define PS_MIO22		(0x58+PIN_MUX_BASE)
#define PS_MIO23		(0x5c+PIN_MUX_BASE)
#define PS_MIO24		(0x60+PIN_MUX_BASE)
#define PS_MIO25		(0x64+PIN_MUX_BASE)
#define PS_MIO26		(0x68+PIN_MUX_BASE)
#define PS_MIO27		(0x6c+PIN_MUX_BASE)
#define PS_MIO28		(0x70+PIN_MUX_BASE)

/********* Register offsets  *********/

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

/********* Interface1 ecc register *********/

#define SMC_REG_ECC1_STATUS             0x400UL
#define SMC_REG_ECC1_CFG                0X404UL
#define SMC_REG_ECC1_MEMCMD0            0X408UL
#define SMC_REG_ECC1_MEMCMD1            0X40CUL
#define SMC_REG_ECC1_ADDR0              0X410UL
#define SMC_REG_ECC1_ADDR1              0X414UL
#define SMC_REG_ECC1_BLOCK0             0X418UL
#define SMC_REG_ECC1_EXTRA_BLOCK        0X428UL

/********* Reserved reg 0x42C - 0x4FC *********/

#define SMC_REG_INT_CFG                 0XE00UL
#define SMC_REG_INT_INPUTS              0XE04UL
#define SMC_REG_INT_OUTPUTS             0XE08UL

/********* Reserved reg 0xEOC - 0xFDC *********/

#define SMC_REG_PERIPH_ID_0             0xFE0UL
#define SMC_REG_PERIPH_ID_1             0xFE4UL
#define SMC_REG_PERIPH_ID_2             0xFE8UL
#define SMC_REG_PERIPH_ID_3             0xFECUL

#define SMC_REG_PCELL_ID_0              0xFF0UL
#define SMC_REG_PCELL_ID_1              0xFF4UL
#define SMC_REG_PCELL_ID_2              0xFF8UL
#define SMC_REG_PCELL_ID_3              0xFFCUL

/****  Bit definition for SMC_MEMC_STATUS register  ****/

//Peripheral_RegName_RegBitsFeature_Value

/* Raw status of the ecc_int1 interrupt signal */
#define SMC_MemcStatus_EccInt1RawStatus_FIELD		(12)

/* Raw status of the ecc_int0 interrupt signal */
#define SMC_MemcStatus_EccInt0RawStatus_FIELD		(11)

/* Status of the ecc_int1 interrupt signal after ANDing with its enable bit, ecc_int_en1 */
#define SMC_MemcStatus_EccInt1Status_FIELD		(10)

/* Status of the ecc_int0 interrupt signal after ANDing with its enable bit, ecc_int_en0 */
#define SMC_MemcStatus_EccInt0Status_FIELD		(9)

/* Interrupt enable status for ecc_int1 */
#define SMC_MemcStatus_EccInt2En_FIELD		        (8)

/* Interrupt enable status for ecc_int0 */
#define SMC_MemcStatus_EccInt1En_FIELD		        (7)

/* Raw status of the smc_int1 interrupt signal */
#define SMC_MemcStatus_SmcInt1RawStatus_FIELD		(6)

/* Raw status of the smc_int0 interrupt signal */
#define SMC_MemcStatus_SmcInt0RawStatus_FIELD		(5)

/* Status of the smc_int1 interrupt signal after ANDing with its enable bit, int_en1 */
#define SMC_MemcStatus_SmcInt1Status_FIELD		    (4)

/* Status of the smc_int0 interrupt signal after ANDing with its enable bit, int_en0 */
#define SMC_MemcStatus_SmcInt0Status_FIELD		    (3)

/* Interrupt enable status for smc_int1 */
#define SMC_MemcStatus_IntEn1_FIELD		        (2)

/* Interrupt enable status for smc_int0 */
#define SMC_MemcStatus_IntEn0_FIELD		        (1)

/* Operating state of the SMC */
#define SMC_MemcStatus_OperaeState_FIELD		(0)



/****  Bit definition for memif_cfg register  ****/

/* Returns the number of exclusive access monitor resources that are implemented in the SMC */
#define SMC_MemifCfg_MonitorNum_FIELD              (16)

/* Returns the value of the remap_1 input */
#define SMC_MemifCfg_Remap1Value_FIELD              (14)

/* Returns the maximum width of the SMC memory data bus for interface 1 */
#define SMC_MemifCfg_Interface1MaxBusWidth_FIELD    (12)

/* Returns the memory interface 1 type */
#define SMC_MemifCfg_Interface1ChipSelNum_FIELD		(10)

/* Returns the maximum width of the SMC memory data bus for interface 1 */
#define SMC_MemifCfg_Interface1Type_FIELD		    (8)

/* Returns the value of the remap_0 input */
#define SMC_MemifCfg_Remap0Value_FIELD              (6)

/* Returns the maximum width of the SMC memory data bus for interface 0 */
#define SMC_MemifCfg_Interface0MaxBusWidth_FIELD    (4)

/* Returns the number of different chip selects that the memory interface 0 supports */
#define SMC_MemifCfg_Interface0ChipSelNum_FIELD		(2)

/* Returns the memory interface 0 type */
#define SMC_MemifCfg_Interface0Type_FIELD		    (0)


/****  Bit definition for mem_cfg_set register  ****/

/*  Enables the ecc_int1 interrupt */
#define SMC_MemCfgSet_EccInt1_ENABLE_FIELD         (6)
#define SMC_MemCfgSet_EccInt1_ENABLE   (1 << SMC_MemCfgSet_EccInt1_ENABLE_FIELD)

/*  Enables the ecc_int0 interrupt */
#define SMC_MemCfgSet_EccInt0_ENABLE_FIELD         (5)
#define SMC_MemCfgSet_EccInt0_ENABLE   (1 << SMC_MemCfgSet_EccInt0_ENABLE_FIELD)

/* Requests the SMC to enter Low-power state when it next becomes idle */
#define SMC_MemCfgSet_SmcLowPower_FIELD           (2)
#define SMC_MemCfgSet_SmcLowPowe_ENABLE   (1 << SMC_MemCfgSet_SmcLowPowe_FIELD)

/* Enables the smc_int1 interrupt */
#define SMC_MemCfgSet_SmcInt1_ENABLE_FIELD        (1)
#define SMC_MemCfgSet_SmcInt1_ENABLE   	(1 << SMC_MemCfgSet_SmcInt1_ENABLE_FIELD)

/* Enables the smc_int0 interrupt */
#define SMC_MemCfgSet_SmcInt0_ENABLE_FIELD        (0)
#define SMC_MemCfgSet_SmcInt0_ENABLE   	(1 << SMC_MemCfgSet_SmcInt0_ENABLE_FIELD)


/****  Bit definition for mem_cfg_clr register  ****/

/*  Disables the ecc_int1 interrupt. SMC sets the ecc_int_en1 bit to 0 in the memc_status Register */
#define SMC_MemCfgClr_EccInt1_DISENABLE_FIELD       (6)
#define SMC_MemCfgClr_EccInt1_DISENABLE     (1 << SMC_MemCfgClr_EccInt1_DISENABLE_FIELD)

/*  Disables the ecc_int0 interrupt. SMC sets the ecc_int_en0 bit to 0 in the memc_status Register */
#define SMC_MemCfgClr_EccInt0_DISENABLE_FIELD       (5)
#define SMC_MemCfgClr_EccInt0_DISENABLE     (1 << SMC_MemCfgClr_EccInt0_DISENABLE_FIELD)

/*  Clear SMC Interrupt 1, as an alternative to an AXI read */
#define SMC_MemCfgClr_ClrSmcInt1_FIELD              (4)
#define SMC_MemCfgClr_ClrSmcInt1     (1 << SMC_MemCfgClr_ClrSmcInt1_FIELD)

/* Clear SMC Interrupt 0, as an alternative to an AXI read */
#define SMC_MemCfgClr_ClrSmcInt0_FIELD              (3)
#define SMC_MemCfgClr_ClrSmcInt0     (1 << SMC_MemCfgClr_ClrSmcInt0_FIELD)

/* Request the SMC to exit Low-power state */
#define SMC_MemCfgClr_SmcLowPowerExit_FIELD         (2)
#define SMC_MemCfgClr_SmcLowPower_EXIT      (1 << SMC_MemCfgClr_SmcLowPowerExit_FIELD)

/* Disables the smc_int1 interrupt */
#define SMC_MemCfgClr_SmcInt1_DISABLE_FIELD          (1)
#define SMC_MemCfgClr_SmcInt1_DISABLE   	(1 << SMC_MemCfgClr_SmcInt1_DISABLE_FIELD)

/*  Disables the smc_int0 interrupt */
#define SMC_MemCfgClr_SmcInt0_DISABLE_FIELD         (0)
#define SMC_MemCfgClr_SmcInt0_DISABLE   	(1 << SMC_MemCfgClr_SmcInt0_DISABLE_FIELD)


/****  Bit definition for direct_cmd register  ****/

/* Selects chip configuration register bank to update, and enables chip mode register access depending on cmd_type */
#define SMC_DirectCmd_SelChip_FIELD     (23)

#define SMC_DirectCmd_SelChip_Interface1Chip4       (7 << SMC_DirectCmd_SelChip_FIELD)
#define SMC_DirectCmd_SelChip_Interface1Chip3	    (6 << SMC_DirectCmd_SelChip_FIELD)
#define SMC_DirectCmd_SelChip_Interface1Chip2	    (5 << SMC_DirectCmd_SelChip_FIELD)
#define SMC_DirectCmd_SelChip_Interface1Chip1  	    (4 << SMC_DirectCmd_SelChip_FIELD)

#define SMC_DirectCmd_SelChip_Interface0Chip4       (3 << SMC_DirectCmd_SelChip_FIELD)
#define SMC_DirectCmd_SelChip_Interface0Chip3   	(2 << SMC_DirectCmd_SelChip_FIELD)
#define SMC_DirectCmd_SelChip_Interface0Chip2   	(1 << SMC_DirectCmd_SelChip_FIELD)
#define SMC_DirectCmd_SelChip_Interface0Chip1   	(0 << SMC_DirectCmd_SelChip_FIELD)

/*  Selects the command type */
#define SMC_DirectCmd_CmdType_FIELD     (21)

#define SMC_DirectCmd_CmdType_ModeRegAndUpdateRegs  (3 << SMC_DirectCmd_CmdType_FIELD)
#define SMC_DirectCmd_CmdType_UpdateRegs   	        (2 << SMC_DirectCmd_CmdType_FIELD)
#define SMC_DirectCmd_CmdType_ModeReg   	        (1 << SMC_DirectCmd_CmdType_FIELD)
#define SMC_DirectCmd_CmdType_UpdateRegsAndAXI   	(0 << SMC_DirectCmd_CmdType_FIELD)

/*  Maps to the configuration register enable signal, cre, when a ModeReg command is issued */
#define SMC_DirectCmd_CreSet_FIELD      (20)
#define SMC_DirectCmd_CreSet_LOW        (0 << SMC_DirectCmd_CreSet_FIELD)
#define SMC_DirectCmd_CreSet_HIGH       (1 << SMC_DirectCmd_CreSet_FIELD)

/*  When cmd_type = UpdateRegs and AXI then: bits [15:0] are used to match wdata[15:0]
 *  bits [19:16] are reserved. Write as zero
 *  When cmd_type = ModeReg or ModeReg and UpdateRegs, these bits map to the external
 *  memory address bits [19:0]
 *  When cmd_type = UpdateRegs, these bits are reserved. Write as zero
**/
#define SMC_DirectCmd_Addr19To0_FIELD       (0)





/****  Bit definition for set_cycles register  ****/

/* Contains the value to be written to the t_rr field in either the */
#define SMC_SetCycles_Trr_FIELD             (20)

/* Contains the value to be written to the t_ar field in either the */
#define SMC_SetCycles_Tar_FIELD             (17)

/* Contains the value to be written to the t_clr field in either the */
#define SMC_SetCycles_Tclr_FIELD            (14)

/* Contains the value to be written to the t_wp field in either the */
#define SMC_SetCycles_Twp_FIELD             (11)

/* Contains the value to be written to the t_rea field in either the */
#define SMC_SetCycles_Trea_FIELD            (8)

/* Contains the value to be written to the t_wc field in either the */
#define SMC_SetCycles_Twc_FIELD             (4)

/* Contains the value to be written to the t_rc field in either the */
#define SMC_SetCycles_Trc_FIELD             (0)


/****  Bit definition for set_opmode register  ****/

/* For a NAND memory interface these bits are reserved, and written as zero */
#define SMC_SetOpmode_SetZero_FIELD       (2)
#define SMC_SetOpmode_SetZero             (0 << SMC_SetOpmode_SetZero_FIELD)


/* Contains the value to be written to the specific chip opmode Register memory width (mw) field. */
#define SMC_SetOpmode_SetMw_FIELD       (0)
#define SMC_SetOpmode_SetMw_32BIT   	(2 << SMC_SetOpmode_SetMw_FIELD)
#define SMC_SetOpmode_SetMw_16BIT  	    (1 << SMC_SetOpmode_SetMw_FIELD)
#define SMC_SetOpmode_SetMw_8Bit  	    (0 << SMC_SetOpmode_SetMw_FIELD)



/****  Bit definition for refresh_0 register  ****/

/* Sets the number of consecutive memory burstsa that the SMC permits, on memory interface 0 */
#define SMC_Refresh0_SetIntface0MemBurNum_FIELD     (23)

 /*
#define SMC_refresh0_Set4Bur  	            (4 << SMC_Refresh0_SetIntface0MemBurNum_FIELD)
#define SMC_refresh0_Set3Bur                (3 << SMC_Refresh0_SetIntface0MemBurNum_FIELD)
#define SMC_refresh0_Set2Bur   	            (2 << SMC_Refresh0_SetIntface0MemBurNum_FIELD)
#define SMC_refresh0_Set1Bur   	            (1 << SMC_Refresh0_SetIntface0MemBurNum_FIELD)
#define SMC_refresh0_SetDisableIdle   	    (0 << SMC_Refresh0_SetIntface0MemBurNum_FIELD)

*/


/****  Bit definition for refresh_1 register  ****/

/* Controls the insertion of idle cycles during consecutive bursts.  */
#define SMC_Refresh1_SetIdleCycDuringBur_FIELD     (2)

/*
#define SMC_refresh0_Set7Bur   	            (7 << SMC_Refresh1_SetIdleCycDuringBur_FIELD)
#define SMC_refresh0_Set6Bur   	            (6 << SMC_Refresh1_SetIdleCycDuringBur_FIELD)
#define SMC_refresh0_Set5Bur   	            (5 << SMC_Refresh1_SetIdleCycDuringBur_FIELD)
#define SMC_refresh0_Set4Bur  	            (4 << SMC_Refresh1_SetIdleCycDuringBur_FIELD)
#define SMC_refresh0_Set3Bur                (3 << SMC_Refresh1_SetIdleCycDuringBur_FIELD)
#define SMC_refresh0_Set2Bur  	            (2 << SMC_Refresh1_SetIdleCycDuringBur_FIELD)
#define SMC_refresh0_Set1Bur	            (1 << SMC_Refresh1_SetIdleCycDuringBur_FIELD)
#define SMC_refresh0_DisableIdle 	        (0 << SMC_Refresh1_SetIdleCycDuringBur_FIELD)
*/




/****  Bit definition for nand_cycles<x>_<n> register  ****/

/* Contains the value to be written to the t_rr field in either the */
#define SMC_NandCycles_Trr_FIELD             (20)

/* Contains the value to be written to the t_ar field in either the */
#define SMC_NandCycles_Tar_FIELD             (17)

/* Contains the value to be written to the t_clr field in either the */
#define SMC_NandCycles_Tclr_FIELD            (14)

/* Contains the value to be written to the t_wp field in either the */
#define SMC_NandCycles_Twp_FIELD             (11)

/* Contains the value to be written to the t_rea field in either the */
#define SMC_NandCycles_Trea_FIELD            (8)

/* Contains the value to be written to the t_wc field in either the */
#define SMC_NandCycles_Twc_FIELD             (4)

/* Contains the value to be written to the t_rc field in either the */
#define SMC_NandCycles_Trc_FIELD             (0)



/****   Bit definition for opmode<x>_<n>  register  ****/

/* For a NAND memory interface [31-2] bits are reserved*/

/* Returns the SMC memory data bus width */
#define SMC_OpmodeStatus_MemWidth_FIELD       (0)


/****   Bit definition for user_status register  ****/

/* This value returns the state of the user_status[7:0] inputs */
#define SMC_UserStatus_Value_FIELD       (0)



/****   Bit definition for ecc<x>_status register  ****/

/* Read flags for ECC blocks. */
#define SMC_EccStatus_EccRead_FIELD         (25)

/* ecc_can_correct */
#define SMC_EccStatus_EccCorrectFlag_FIELD  (20)

/* ecc_fail */
#define SMC_EccStatus_EccFail_FIELD         (15)

/* ecc_value_valid Valid flag for each ECC block. */
#define SMC_EccStatus_ECCValueValid_FIELD   (10)

/* ecc_rd_n_wr 0 = write 1 = read.*/
#define SMC_EccStatus_EccRDnWR_FIELD        (9)

/* ecc_last_status [8:7] */
#define SMC_EccStatus_EccLastStatus_FIELD       (7)

/* Provides the status of the ECC block 0 = idle 1 = busy*/
#define SMC_EccStatus_EccStatus_FIELD           (6)

#define SMC_EccStatus_EccIdle         (0 << SMC_EccStatus_EccStatus_FIELD)
#define SMC_EccStatus_EccBusy         (1 << SMC_EccStatus_EccStatus_FIELD)

/* ecc_int_status  The interrupts are.  To clear an interrupt, write a 1 to the appropriate bit*/
#define SMC_EccStatus_EccIntStatus_FIELD        (0)

#define SMC_EccStatus_Abort_StatusClr           (32 << SMC_EccStatus_EccIntStatus_FIELD)
#define SMC_EccStatus_ExtBlock_StatusClr        (16 << SMC_EccStatus_EccIntStatus_FIELD)
#define SMC_EccStatus_Block3_StatusClr          (8 << SMC_EccStatus_EccIntStatus_FIELD)
#define SMC_EccStatus_Block2_StatusClr          (4 << SMC_EccStatus_EccIntStatus_FIELD)
#define SMC_EccStatus_Block1_StatusClr          (2 << SMC_EccStatus_EccIntStatus_FIELD)
#define SMC_EccStatus_Block0_StatusClr          (1 << SMC_EccStatus_EccIntStatus_FIELD)

/****   Bit definition for ecc<x>_cfg  register  ****/

/* Indicates that the memory supports column change address commands */
#define SMC_EccCfg_EccJump_FIELD           	(5)

#define SMC_EccCfg_EccNoJump     			(0 << SMC_EccCfg_EccJump_FIELD)
#define SMC_EccCfg_EccJump_UseColChange     (1 << SMC_EccCfg_EccJump_FIELD)
#define SMC_EccCfg_EccJump_UseFullChange    (2 << SMC_EccCfg_EccJump_FIELD)

/* Indicates when ECC values are read from memory */
#define SMC_EccCfg_EccValueOffset_FIELD           (4)

#define SMC_EccCfg_EccValue_InBoundary      (0 << SMC_EccCfg_EccValueOffset_FIELD)
#define SMC_EccCfg_EccValue_InPageEnd       (1 << SMC_EccCfg_EccValueOffset_FIELD)


/* This specifies the mode of the ECC block */
#define SMC_EccCfg_EccMode_FIELD           (2)

#define SMC_EccCfg_EccByPasss       (0 << SMC_EccCfg_EccMode_FIELD)
#define SMC_EccCfg_EccModeAPB       (1 << SMC_EccCfg_EccMode_FIELD)
#define SMC_EccCfg_EccModeMem  		(2 << SMC_EccCfg_EccMode_FIELD)


/* The number of 512 byte blocks in a page*/
#define SMC_EccCfg_PageSize_FIELD           (0)

#define SMC_EccCfg_Four_PageSize        (3 << SMC_EccCfg_PageSize_FIELD)
#define SMC_EccCfg_Two_PageSize         (2 << SMC_EccCfg_PageSize_FIELD)
#define SMC_EccCfg_One_PageSize         (1 << SMC_EccCfg_PageSize_FIELD)




/****   Bit definition for ecc<x>_memcmd0 register  ****/

 /* Use end command */
 #define SMC_EccMemCmd0_UseEndCmd_FIELD      (24)
 #define SMC_EccMemCmd0_UseEndCmd            (1 << SMC_EccMemCmd0_UseEndCmd_FIELD)

 /* The NAND<x> command to indicate the end of a read (0x30) */
 #define SMC_EccMemCmd0_EndReadCmd_FIELD     (16)
 #define SMC_EccMemCmd0_EndReadCmd           (0x30 << SMC_EccMemCmd0_EndReadCmd_FIELD)
 #define SMC_EccMemCmd0_EndReadCacheCmd      (0x31 << SMC_EccMemCmd0_EndReadCmd_FIELD)
 #define SMC_EccMemCmd0_EndReadChangeColumnCmd      (0xE0 << SMC_EccMemCmd0_EndReadCmd_FIELD)

 /* The NAND<x> command to initiate a read (0x00) */
 #define SMC_EccMemCmd0_InitReadCmd_FIELD    (8)
 #define SMC_EccMemCmd0_InitReadCmd          (0x00 << SMC_EccMemCmd0_InitReadCmd_FIELD)
 #define SMC_EccMemCmd0_InitReadChangeColumnCmd          (0x05 << SMC_EccMemCmd0_InitReadCmd_FIELD)

 /* The NAND<x> command to initiate a write (0x80) */
 #define SMC_EccMemCmd0_InitWriteCmd_FIELD   (0)
 #define SMC_EccMemCmd0_InitWriteCmd         (0x80 << SMC_EccMemCmd0_InitWriteCmd_FIELD)



/****   Bit definition for ecc<x>_memcmd1 register  ****/


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

/****   Bit definition for ecc<x>_addr0 register  ****/

/* Returns the lower 32 bits of the ECC address, for memory interface <x> */
/* Read Only */


/****   Bit definition for ecc<x>_addr1 register  ****/

/* Returns the upper 24 bits of the ECC address, for memory interface <x> */
/* Read Only */


/****   Bit definition for ecc<x>_block<3:0>  register  ****/

/* Interrupt flag for block <a> */
#define SMC_EccBlock_ClearBlockIntFlag_FIELD        (31)

/* Indicates if the ECC value for block <a> is valid */
#define SMC_EccBlock_ISCheakValueValid_FIELD        (30)

/* Indicates if the ECC value for block <a> has been read from memory */
#define SMC_EccBlock_IsEccValueReadFormMem_FIELD    (29)

/* Indicates if the ECC value for block <a> has failed */
#define SMC_EccBlock_IsEccValueFailed_FIELD (28)

/* Indicates if block <a> is correctable */
#define SMC_EccBlock_IsCorrectable_FIELD    (27)

/* ECC value of check result for block <a> */
#define SMC_EccBlock_CheakValue_FIELD       (0)



/****   Bit definition for ecc<x>_extra_block  register  ****/


/* Interrupt flag for the extra block. To clear this bit, write any value to the register */
#define SMC_EccExtBlock_ClearBlockIntFlag_FIELD       (31)

/* Indicates if the ECC value for extra block is valid. */
#define SMC_EccExtBlock_ISCheakValueValid_FIELD       (30)

/* Indicates if the ECC value for extra block has been read from memory */
#define SMC_EccExtBlock_IsEccValueReadFormMem_FIELD       (29)

/* Indicates if the ECC value for extra block has failed. */
#define SMC_EccExtBlock_IsEccValueFailed_FIELD       (28)

/* Indicates if the extra block is correctable. */
#define SMC_EccExtBlock_IsCorrectable_FIELD       (27)

/* ECC value of check result for the extra block. */
#define SMC_EccExtBlock_CheakValue_FIELD       (0)



 /*  ecc_fail_bit ecc_can_correct_bit */
 #define NO_ERROR_FLAG        0x00
 #define PARITY_ERROR_FLAG    0x01
 #define MULTIPLE_ERROR_FLAG  0x02
 #define SINGLE_ERROR_FLAG    0x03


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


/* */
#define CRC16_INIT			0x4F4E		/* CRC16 initial constant */
#define CRC16_POLYNOM		0x8005		/* CRC16 polynomial */
#define CRC16_LEN			254			/* Onfi CRC16 length */

#define ONFI_PARAM_LEN		256			/* Onfi Parameter Page Length */
#define ONFI_SIGNATURE_LEN	4			/* Onfi Signature Length */

/* */
#define NAND_FEAT_POS		6
#define DATA_PER_PAGE_POS	80
#define SPARE_PER_PAGE_POS	84
#define PAGE_PER_BLOCK_POS	92
#define BLOCKS_PER_UINT_POS	96
#define TOTAL_UINT_POS		100
#define ECC_NUM_POS			112

#define ZoneBitError	0
#define OneBitError		1

#define NAND_MFR_AMD		0x01

#endif
