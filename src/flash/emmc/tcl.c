/*
 * File: tcl.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "core.h"
#include "imp.h"
#include "fileio.h"
#include <helper/time_support.h>

extern struct emmc_device *emmc_devices;


// multi emmc support
COMMAND_HANDLER(handle_emmc_list_command)
{
	struct emmc_device *p;
	int i;

	if (!emmc_devices) {
		command_print(CMD, "no emmc flash devices configured");
		return ERROR_OK;
	}

	for (p = emmc_devices, i = 0; p; p = p->next, i++) {
		if (p->device)
			command_print(CMD, "#%i: %s blocksize: %i",
				i, p->device->name,
				p->device->block_size);
		else
			command_print(CMD, "#%i: not probed", i);
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_emmc_probe_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct emmc_device *emmc;
	int retval = CALL_COMMAND_HANDLER(emmc_command_get_device, 0, &emmc);
	if (retval != ERROR_OK)
		return retval;

	retval = emmc_probe(emmc);
	if (retval == ERROR_OK) {
		command_print(CMD, "emmc flash probed");
		// command_print(CMD, "EMMC flash device '%s' found", p->device->name);
	}

	return retval;
}


COMMAND_HANDLER(handle_emmc_write_image_command)
{
	struct emmc_device *emmc = NULL;
	struct emmc_fileio_state s;
	int file_size, total_size;
	int retval ;	

	retval= CALL_COMMAND_HANDLER(emmc_fileio_parse_args,
			&s, &emmc, FILEIO_READ, false);
	if(retval != ERROR_OK) 
	{
		return retval;
	}

	file_size = s.size;
	total_size = emmc_fileio_read(&s);
	retval = emmc_write_image(emmc, s.block, s.address, total_size);

	if (emmc_fileio_finish(&s) == ERROR_OK) {
		command_print(CMD, "wrote file %s to EMMC flash %d up to "
			"offset 0x%8.8" PRIx32 " in %fs (%0.3f KiB/s)",
			CMD_ARGV[0], s.bank_num, s.address, duration_elapsed(&s.bench),
			duration_kbps(&s.bench, file_size));
	}
	return retval;
}

COMMAND_HANDLER(handle_emmc_read_block_command)
{
	int retval;
	uint32_t block_cnt = 1, byte_cnt;
	uint8_t *buffer = NULL;

	struct target *target = get_current_target(CMD_CTX);
	struct emmc_device *emmc;

	if (CMD_ARGC < 1 || CMD_ARGC > 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	target_addr_t address;
	COMMAND_PARSE_ADDRESS(CMD_ARGV[0], address);

	if(address % 512 != 0)
	{
		LOG_ERROR("address should be block aligned");
		return ERROR_FAIL;
	}

	if(CMD_ARGC == 2)
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], block_cnt);


	byte_cnt = 512 * block_cnt;
	LOG_INFO("emmc read block start addr %" PRIx64 " byte cnt %x", address, byte_cnt);
	buffer = calloc(byte_cnt, 1);
	emmc = get_emmc_device_by_num(0);
	if (!emmc)
	{
		LOG_INFO("emmc get device error");
		retval = ERROR_FAIL;
		goto fail;
	}

	if(!emmc->device)
	{
		LOG_INFO("emmc not probed");
		retval = ERROR_FAIL;
		goto fail;
	}

	retval = emmc_read_data_block(emmc, (uint32_t*) buffer, address);

	if(retval == ERROR_OK)
		target_handle_md_output(CMD, target, address, 1, byte_cnt, (uint8_t*) buffer,false);

fail:
	free(buffer);
	return retval;
}


COMMAND_HANDLER(handle_emmc_verify_command)
{
	struct emmc_device *emmc = NULL;
	struct emmc_fileio_state file;
	int file_size;
	int retval = CALL_COMMAND_HANDLER(emmc_fileio_parse_args,
			&file, &emmc, FILEIO_READ, false);
	if (retval != ERROR_OK)
		return retval;

	file_size = file.size;
	emmc_fileio_read(&file);
	retval = emmc_verify_image(emmc, file.block, file.address, file_size);
	if(retval != ERROR_OK)
	{
		command_print(CMD, "emmc verify file %s fail", CMD_ARGV[0]);
		emmc_fileio_finish(&file);
		return retval;
	}
	
	if (emmc_fileio_finish(&file) == ERROR_OK) {
		command_print(CMD, "verified file %s "
			"up to offset 0x%8.8" PRIx32 " in %fs (%0.3f KiB/s)",
			CMD_ARGV[0], file.address, duration_elapsed(&file.bench),
			duration_kbps(&file.bench, file.size));
	}

	return ERROR_OK;
}


static const struct command_registration emmc_exec_command_handlers[] = {
	{
		.name = "list",
		.handler = handle_emmc_list_command,
		.mode = COMMAND_EXEC,
		.help = "list configured EMMC flash devices",
		.usage = "",
	},
	{
		.name = "probe",
		.handler = handle_emmc_probe_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id",
		.help = "identify EMMC flash device",
	},	
	{
		.name = "write_image",
		.handler = handle_emmc_write_image_command,
		.mode = COMMAND_EXEC,
		.usage = "filename offset",
		.help = "Write an image to flash. "
			"Allow optional offset from beginning of bank (defaults to zero)",	
	},
	{
		.name = "read_block",
		.handler = handle_emmc_read_block_command,
		.mode = COMMAND_EXEC,
		.usage = "addr [blk_cnt]",
		.help = "Read blk from emmc",
	},
	{
		.name = "verify_image",
		.handler = handle_emmc_verify_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id filename offset ",
		.help = "Verify an image against emmc. Allow optional "
			"offset from beginning of bank (defaults to zero)",
	},	

	COMMAND_REGISTRATION_DONE
};

static int emmc_init(struct command_context *cmd_ctx)
{
	if (!emmc_devices)
		return ERROR_OK;

	return register_commands(cmd_ctx, "emmc", emmc_exec_command_handlers);
}

// init emmc commands
COMMAND_HANDLER(handle_emmc_init_command)
{
	if (CMD_ARGC != 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	static bool emmc_initialized;
	if (emmc_initialized) {
		LOG_INFO("'emmc init' has already been called");
		return ERROR_OK;
	}
	emmc_initialized = true;

	return emmc_init(CMD_CTX);
}


static int emmc_list_walker(struct emmc_flash_controller *c, void *x)
{
	struct command_invocation *cmd = x;
	command_print(cmd, "  %s", c->name);
	return ERROR_OK;
}


COMMAND_HANDLER(handle_emmc_list_drivers)
{
	command_print(CMD, "Available emmc flash controller drivers:");
	return emmc_driver_walk(&emmc_list_walker, CMD);
}

static COMMAND_HELPER(create_emmc_device, const char *bank_name,
	struct emmc_flash_controller *controller)
{
	struct emmc_device *c;
	struct target *target;
	int retval;

	LOG_INFO("get target");
	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;
	target = get_target(CMD_ARGV[1]);
	if (!target) {
		LOG_ERROR("invalid target %s", CMD_ARGV[1]);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	LOG_INFO("register cmd");

	if (controller->commands) {
		retval = register_commands(CMD_CTX, NULL, controller->commands);
		if (retval != ERROR_OK)
			return retval;
	}
	c = malloc(sizeof(struct emmc_device));
	if (!c) {
		LOG_ERROR("End of memory");
		return ERROR_FAIL;
	}

	c->name = strdup(bank_name);
	c->target = target;
	c->controller = controller;
	c->controller_priv = NULL;
	c->device = NULL;
	c->next = NULL;

	retval = CALL_COMMAND_HANDLER(controller->emmc_device_command, c);
	if (retval != ERROR_OK) {
		LOG_ERROR("'%s' driver rejected emmc flash. Usage: %s", 
			controller->name,
			controller->usage);
		free(c);
		return retval;
	}

	if (!controller->usage)
		LOG_DEBUG("'%s' driver usage field missing", controller->name);

	emmc_device_add(c);

	return ERROR_OK;
}

// set update driver for emmc device
COMMAND_HANDLER(handle_emmc_device_command)
{
	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	/* save name and increment (for compatibility) with drivers */
	const char *bank_name = *CMD_ARGV++;
	CMD_ARGC--;

	const char *driver_name = CMD_ARGV[0];
	struct emmc_flash_controller *controller;
	controller = emmc_driver_find_by_name(CMD_ARGV[0]);
	if (!controller) {
		LOG_ERROR("No valid EMMC flash driver found (%s)", driver_name);
		return CALL_COMMAND_HANDLER(handle_emmc_list_drivers);
	}

	return CALL_COMMAND_HANDLER(create_emmc_device, bank_name, controller);
}


static const struct command_registration emmc_config_command_handlers[] = {
	{
		.name = "device",
		.handler = &handle_emmc_device_command,
		.mode = COMMAND_CONFIG,
		.help = "defines a new EMMC bank",
		.usage = "bank_id driver target [driver_options ...]",
	},
	{
		.name = "drivers",
		.handler = &handle_emmc_list_drivers,
		.mode = COMMAND_ANY,
		.help = "lists available EMMC drivers",
		.usage = ""
	},
	{
		.name = "init",
		.mode = COMMAND_CONFIG,
		.handler = &handle_emmc_init_command,
		.help = "initialize EMMC devices",
		.usage = ""
	},		
	COMMAND_REGISTRATION_DONE
};



static const struct command_registration emmc_command_handlers[] = {
	{
		.name = "emmc",
		.mode = COMMAND_ANY,
		.help = "EMMC flash command group",
		.usage = "",
		.chain = emmc_config_command_handlers,
	},

	COMMAND_REGISTRATION_DONE
};

int emmc_register_commands(struct command_context *cmd_ctx)
{
	return register_commands(cmd_ctx, NULL, emmc_command_handlers);
}
