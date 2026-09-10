/***************************************************************************
 *   Copyright (C) 2018-2019 by Andreas Bolsch                             *
 *   andreas.bolsch@mni.thm.de                                             *
 *                                                                         *
 *   Copyright (C) 2012 by George Harris                                   *
 *   george@luminairecoffee.com                                            *
 *                                                                         *
 *   Copyright (C) 2010 by Antonio Borneo                                  *
 *   borneo.antonio@gmail.com                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef OPENOCD_FLASH_NOR_SPI_EXT_H
#define OPENOCD_FLASH_NOR_SPI_EXT_H

#include "imp.h"
#include "spi.h"

#ifndef __ASSEMBLER__

/* external flash map struct */
struct ext_flash_device {
	char name[32];
	struct flash_device flash;
	struct ext_flash_device *next;
};

#endif

/* fields in SPI flash status register */
#define	SPIFLASH_BSY		0
#define SPIFLASH_BSY_BIT	(1 << SPIFLASH_BSY)	/* WIP Bit of SPI SR */
#define	SPIFLASH_WE			1
#define SPIFLASH_WE_BIT		(1 << SPIFLASH_WE)	/* WEL Bit of SPI SR */

/* SPI Flash Commands */
#define SPIFLASH_READ_ID		0x9F /* Read Flash Identification */
#define SPIFLASH_READ_MID		0xAF /* Read Flash Identification, multi-io */
#define SPIFLASH_READ_STATUS	0x05 /* Read Status Register */
#define SPIFLASH_WRITE_STATUS   0x01
#define SPIFLASH_WRITE_ENABLE	0x06 /* Write Enable */
#define SPIFLASH_PAGE_PROGRAM	0x02 /* Page Program */
#define SPIFLASH_FAST_READ		0x0B /* Fast Read */
#define SPIFLASH_READ			0x03 /* Normal Read */
#define SPIFLASH_MASS_ERASE		0xC7 /* Mass Erase */
#define SPIFLASH_READ_SFDP		0x5A /* Read Serial Flash Discoverable Parameters */
#define SPIFLASH_FAST_READ_QUAD 0xEB /* Fast Read Quad Output */

/* WINBOND Commands */
#define WINBOND_EN4B            0xB7
#define WINBOND_EX4B            0xE9

#define SPIFLASH_DEF_PAGESIZE	256  /* default for non-page-oriented devices (FRAMs) */

const struct flash_device *lookup_external_flash_id_map(uint32_t id);
int flash_ext_map_register_commands(struct command_context *cmd_ctx);

#endif /* OPENOCD_FLASH_NOR_SPI_EXT_H */
