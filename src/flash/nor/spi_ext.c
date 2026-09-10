/***************************************************************************
 *   Copyright (C) 2018 by Andreas Bolsch                                  *
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

#include "spi_ext.h"
#include <jtag/jtag.h>
#include <helper/command.h>
#include <helper/log.h>

static struct ext_flash_device *ext_flash_list;

COMMAND_HANDLER(handle_flash_id_map_add_ext)
{
	if (CMD_ARGC != 10) {
		command_print(CMD, "Usage: handle_flash_id_map_add_ext <id_hex> <name> <read> "
			"<qread> <pprog> <erase> <chiperase> <pagesize> <sectorsize> <sizeinbyte>");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	/* parse params */
	uint32_t id				= (uint32_t)strtoul(CMD_ARGV[0], NULL, 16);
	const char *name		= CMD_ARGV[1];
	uint32_t read_cmd		= (uint32_t)strtoul(CMD_ARGV[2], NULL, 16);
	uint32_t qread_cmd		= (uint32_t)strtoul(CMD_ARGV[3], NULL, 16);
	uint32_t pprog_cmd		= (uint32_t)strtoul(CMD_ARGV[4], NULL, 16);
	uint32_t erase_cmd		= (uint32_t)strtoul(CMD_ARGV[5], NULL, 16);
	uint32_t chiperase_cmd	= (uint32_t)strtoul(CMD_ARGV[6], NULL, 16);
	uint32_t pagesize		= (uint32_t)strtoul(CMD_ARGV[7], NULL, 16);
	uint32_t sectorsize		= (uint32_t)strtoul(CMD_ARGV[8], NULL, 16);
	uint32_t size_in_bytes	= (uint32_t)strtoul(CMD_ARGV[9], NULL, 16);

	struct ext_flash_device *e = calloc(1, sizeof(*e));
	if (!e)
		return ERROR_FAIL;

	e->flash.device_id = (uint32_t)id;
	snprintf(e->name, sizeof(e->name), "%s", name);
	e->flash.name = e->name;
	e->flash.read_cmd = (uint8_t)read_cmd;
	e->flash.qread_cmd = (uint8_t)qread_cmd;
	e->flash.pprog_cmd = (uint8_t)pprog_cmd;
	e->flash.erase_cmd = (uint8_t)erase_cmd;
	e->flash.chip_erase_cmd = (uint8_t)chiperase_cmd;
	e->flash.pagesize = pagesize;
	e->flash.sectorsize = sectorsize;
	e->flash.size_in_bytes = size_in_bytes;

	/* insert list header */
	e->next = ext_flash_list;
	ext_flash_list = e;

	LOG_DEBUG("External flash map: added 0x%08x (%s)", e->flash.device_id, e->flash.name);

	return ERROR_OK;
}

static const struct command_registration flash_ext_map_commands[] = {
	{
		.name = "handle_flash_id_map_add_ext",
		.handler = handle_flash_id_map_add_ext,
		.mode = COMMAND_ANY,
		.help = "Load external flash ID-to-parameter mapping",
		.usage = "<dict>",
	},
	COMMAND_REGISTRATION_DONE
};

const struct flash_device *lookup_external_flash_id_map(uint32_t id)
{
	struct ext_flash_device *p;
	for (p = ext_flash_list; p; p = p->next) {
		if (p->flash.device_id == id)
			return &(p->flash);
	}
	return NULL;
}

int flash_ext_map_register_commands(struct command_context *cmd_ctx)
{
	return register_commands(cmd_ctx, NULL, flash_ext_map_commands);
}
