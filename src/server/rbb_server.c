/***************************************************************************
 *   Copyright (C) 2024 Ruigang Wan										*
 *   ruigang.wan@anlogic.com											   *
 *																		 *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or	 *
 *   (at your option) any later version.								   *
 *																		 *
 *   This program is distributed in the hope that it will be useful,	   *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of		*
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		 *
 *   GNU General Public License for more details.						  *
 *																		 *
 *   You should have received a copy of the GNU General Public License	 *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <jtag/jtag.h>

#include "server.h"
#include "rbb_server.h"

int allow_tap_access;

/**
 * @file
 *
 * RBB server.
 *
 * This server allows RBB transport from TD to OpenOCD
 */

struct rbb_service {
	unsigned int channel;
};


static int rbb_new_connection(struct connection *connection)
{
	struct rbb_service *service;

	service = connection->service->priv;

	LOG_DEBUG("rbb: New connection for channel %u", service->channel);

	return ERROR_OK;
}

static int rbb_connection_closed(struct connection *connection)
{
	struct rbb_service *service;

	service = (struct rbb_service *)connection->service->priv;
	free(service);

	LOG_DEBUG("rbb: Connection for channel %u closed", service->channel);

	return ERROR_OK;
}

static int rbb_input(struct connection *connection)
{
	int bytes_read;
	unsigned char buffer[1024];
	struct rbb_service *service;
	/* size_t length; */

	service = (struct rbb_service *)connection->service->priv;
	/* If the TAP state not in TLR or RTI, just return back */
	bytes_read = connection_read(connection, buffer, sizeof(buffer));
	/* Needs to Lock the adapter driver, reject any other access */
	/* TODO: dirty call, don't do that */
	service = service;
	if (!bytes_read)
		return ERROR_SERVER_REMOTE_CLOSED;
	else if (bytes_read < 0) {
		LOG_ERROR("error during read: %s", strerror(errno));
		return ERROR_SERVER_REMOTE_CLOSED;
	}

	/* length = bytes_read; */

	return ERROR_OK;
}

static const struct service_driver rbb_service_driver = {
	.name = "rbb",
	.new_connection_during_keep_alive_handler = NULL,
	.new_connection_handler = rbb_new_connection,
	.input_handler = rbb_input,
	.connection_closed_handler = rbb_connection_closed,
	.keep_client_alive_handler = NULL,
};

COMMAND_HANDLER(handle_rbb_start_command)
{
	int ret;
	struct rbb_service *service;

	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	service = malloc(sizeof(struct rbb_service));

	if (!service)
		return ERROR_FAIL;

	service->channel = 0;

	ret = add_service(&rbb_service_driver, CMD_ARGV[0], CONNECTION_LIMIT_UNLIMITED, service);

	if (ret != ERROR_OK) {
		free(service);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_rbb_stop_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	remove_service("rbb", CMD_ARGV[0]);

	return ERROR_OK;
}

static const struct command_registration rbb_server_subcommand_handlers[] = {
	{.name = "start",
	 .handler = handle_rbb_start_command,
	 .mode = COMMAND_ANY,
	 .help = "Start a rbb server",
	 .usage = "<port>"},
	{.name = "stop",
	 .handler = handle_rbb_stop_command,
	 .mode = COMMAND_ANY,
	 .help = "Stop a rbb server",
	 .usage = "<port>"},
	COMMAND_REGISTRATION_DONE};

static const struct command_registration rbb_server_command_handlers[] = {
	{.name = "server",
	 .mode = COMMAND_ANY,
	 .help = "rbb server",
	 .usage = "",
	 .chain = rbb_server_subcommand_handlers},
	COMMAND_REGISTRATION_DONE};

static const struct command_registration rbb_command_handlers[] = {
	{.name = "rbb",
	 .mode = COMMAND_ANY,
	 .help = "rbb",
	 .usage = "",
	 .chain = rbb_server_command_handlers},
	COMMAND_REGISTRATION_DONE};

int rbb_server_register_commands(struct command_context *ctx)
{
	return register_commands(ctx, NULL, rbb_command_handlers);
}
