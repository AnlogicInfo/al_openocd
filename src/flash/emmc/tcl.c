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
#include <target/target.h>

static int emmc_list_walker(struct emmc_flash_controller *c, void *x)
{
	struct command_invocation *cmd = x;
	command_print(cmd, "  %s", c->name);
	return ERROR_OK;
}

COMMAND_HANDLER(handle_emmc_list_drivers)
{
	command_print(CMD, "Available NAND flash controller drivers:");
	return emmc_driver_walk(&emmc_list_walker, CMD);
}

static COMMAND_HELPER(create_emmc_device, const char *bank_name,
	struct emmc_flash_controller *controller)
{
	// struct nand_device *c;
	// struct target *target;
	// int retval;

	// if (CMD_ARGC < 2)
	// 	return ERROR_COMMAND_SYNTAX_ERROR;
	// target = get_target(CMD_ARGV[1]);
	// if (!target) {
	// 	LOG_ERROR("invalid target %s", CMD_ARGV[1]);
	// 	return ERROR_COMMAND_ARGUMENT_INVALID;
	// }

	// if (controller->commands) {
	// 	retval = register_commands(CMD_CTX, NULL, controller->commands);
	// 	if (retval != ERROR_OK)
	// 		return retval;
	// }
	// c = malloc(sizeof(struct nand_device));
	// if (!c) {
	// 	LOG_ERROR("End of memory");
	// 	return ERROR_FAIL;
	// }

	// c->name = strdup(bank_name);
	// c->target = target;
	// c->controller = controller;
	// c->controller_priv = NULL;
	// c->manufacturer = NULL;
	// c->device = NULL;
	// c->bus_width = 0;
	// c->address_cycles = 0;
	// c->page_size = 0;
	// c->use_raw = false;
	// c->next = NULL;

	// retval = CALL_COMMAND_HANDLER(controller->nand_device_command, c);
	// if (retval != ERROR_OK) {
	// 	LOG_ERROR("'%s' driver rejected nand flash. Usage: %s",
	// 		controller->name,
	// 		controller->usage);
	// 	free(c);
	// 	return retval;
	// }

	// if (!controller->usage)
	// 	LOG_DEBUG("'%s' driver usage field missing", controller->name);

	// nand_device_add(c);

	return ERROR_OK;
}

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
		.help = "defines a new NAND bank",
		.usage = "bank_id driver target [driver_options ...]",
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
