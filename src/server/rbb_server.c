/***************************************************************************
 *	 Copyright (C) 2024 Ruigang Wan										*
 *	 ruigang.wan@anlogic.com												 *
 *																		 *
 *	 This program is free software; you can redistribute it and/or modify	*
 *	 it under the terms of the GNU General Public License as published by	*
 *	 the Free Software Foundation; either version 2 of the License, or	 *
 *	 (at your option) any later version.									 *
 *																		 *
 *	 This program is distributed in the hope that it will be useful,		 *
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of		*
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the		 *
 *	 GNU General Public License for more details.							*
 *																		 *
 *	 You should have received a copy of the GNU General Public License	 *
 *	 along with this program.	If not, see <http://www.gnu.org/licenses/>. *
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


struct jtag_region {
	int is_tms;
	int begin;
	int end;
	int flip_tms;
	tap_state_t endstate;
};

struct rbb_service {
	unsigned int channel;
	int last_is_read;
	tap_state_t state;
	struct jtag_region regions[64];
	size_t region_count;
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

tap_state_t next_state(tap_state_t cur, int bit)
{
	switch (cur) {
	case TAP_RESET:
		return bit ? TAP_RESET : TAP_IDLE;
	case TAP_IDLE:
		return bit ? TAP_DRSELECT : TAP_IDLE;
	case TAP_DRSELECT:
		return bit ? TAP_IRSELECT : TAP_DRCAPTURE;
	case TAP_DRCAPTURE:
		return bit ? TAP_DREXIT1 : TAP_DRSHIFT;
	case TAP_DRSHIFT:
		return bit ? TAP_DREXIT1 : TAP_DRSHIFT;
	case TAP_DREXIT1:
		return bit ? TAP_DRUPDATE : TAP_DRPAUSE;
	case TAP_DRPAUSE:
		return bit ? TAP_DREXIT2 : TAP_DRPAUSE;
	case TAP_DREXIT2:
		return bit ? TAP_DRUPDATE : TAP_DRSHIFT;
	case TAP_DRUPDATE:
		return bit ? TAP_DRSELECT : TAP_IDLE;
	case TAP_IRSELECT:
		return bit ? TAP_RESET : TAP_IRCAPTURE;
	case TAP_IRCAPTURE:
		return bit ? TAP_IREXIT1 : TAP_IRSHIFT;
	case TAP_IRSHIFT:
		return bit ? TAP_IREXIT1 : TAP_IRSHIFT;
	case TAP_IREXIT1:
		return bit ? TAP_IRUPDATE : TAP_IRPAUSE;
	case TAP_IRPAUSE:
		return bit ? TAP_IREXIT2 : TAP_IRPAUSE;
	case TAP_IREXIT2:
		return bit ? TAP_IRUPDATE : TAP_IRSHIFT;
	case TAP_IRUPDATE:
		return bit ? TAP_DRSELECT : TAP_IDLE;
	default:
		assert(false);
	}
	return TAP_RESET;
}

#define ANALYZE_COMPLETE

#ifdef ANALYZE_COMPLETE



static void analyze_bitbang(const uint8_t *tms, int bits,
				const uint8_t *read_bits, struct rbb_service *service)
{
	/* Should skip anything doesn't require to read in */
	int shift_pos = 0;
	uint8_t tdo_read_bit_old = read_bits[0] & 0x1;
	tap_state_t cur_state = service->state;

	for (int i = 0; i < bits; i++) {
		uint8_t tms_bit = (tms[i / 8] >> (i % 8)) & 0x1;
		uint8_t tdo_read_bit = (read_bits[(i+1) / 8] >> ((i+1) % 8)) & 0x1;

		service->region_count = 0;

		tap_state_t new_state = next_state(cur_state, tms_bit);
		if ((cur_state != TAP_DRSHIFT && new_state == TAP_DRSHIFT) ||
			(cur_state != TAP_IRSHIFT && new_state == TAP_IRSHIFT)) {
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = true;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			shift_pos = i + 1;
		} else if ((cur_state == TAP_DRSHIFT && new_state != TAP_DRSHIFT) ||
					(cur_state == TAP_IRSHIFT && new_state != TAP_IRSHIFT)) {
			/* end */
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = false;
			region->flip_tms = true;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			shift_pos = i + 1;

		} else if ((tdo_read_bit_old != tdo_read_bit && cur_state == TAP_DRSHIFT && new_state == cur_state) ||
					(tdo_read_bit_old != tdo_read_bit && cur_state == TAP_IRSHIFT && new_state == cur_state)) {
			/* skip some bits */
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = false;
			region->flip_tms = false;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			/* dprintf("region-> begin %d, end %d\n", region->begin, region->end); */

			tdo_read_bit_old = tdo_read_bit;

			shift_pos = i + 1;
		} else if ((tdo_read_bit_old != tdo_read_bit && cur_state == TAP_DRSHIFT && new_state != cur_state) ||
					(tdo_read_bit_old != tdo_read_bit && cur_state == TAP_IRSHIFT && new_state != cur_state)) {
			/* skip some bits */
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = false;
			region->flip_tms = true;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			/* dprintf("region-> begin %d, end %d\n", region->begin, region->end); */

			tdo_read_bit_old = tdo_read_bit;

			shift_pos = i + 1;

		}

		cur_state = new_state;
	}
}
#endif

static int rbb_input(struct connection *connection)
{
	int bytes_read;
	unsigned char buffer[RBB_BUFFERSIZE];
	struct rbb_service *service;
	size_t length;

	unsigned char tms_input[RBB_BUFFERSIZE];
	unsigned char tdi_input[RBB_BUFFERSIZE];
	unsigned char read_input[RBB_BUFFERSIZE];
	unsigned char send_buffer[RBB_BUFFERSIZE];

		/* If the TAP state not in TLR or RTI, just return back */
	if (cmd_queue_cur_state != TAP_IDLE && cmd_queue_cur_state != TAP_RESET)
		return ERROR_OK;

	service = (struct rbb_service *)connection->service->priv;
	bytes_read = connection_read(connection, buffer, sizeof(buffer));
	/* Needs to Lock the adapter driver, reject any other access */
	/* TODO: dirty call, don't do that */
	allow_tap_access = 1;

	service = service;
	if (!bytes_read) {
		allow_tap_access = 0;
		return ERROR_SERVER_REMOTE_CLOSED;
	}
	else if (bytes_read < 0) {
		allow_tap_access = 0;
		LOG_ERROR("error during read: %s", strerror(errno));
		return ERROR_SERVER_REMOTE_CLOSED;
	}

	length = bytes_read;

	char command;
	int bits = 0, read_bits = 0;
	size_t i;
	for (i = 0; i < length; i++) {
		command = buffer[i];
		if ('0' <= command && command <= '7') {
			/* set */
		char offset = command - '0';
		int tck = (offset >> 2) & 1;
		int tms = (offset >> 1) & 1;
		int tdi = (offset >> 0) & 1;
		if (tck) {
			tms_input[bits / 8] |= tms << (bits % 8);
			tdi_input[bits / 8] |= tdi << (bits % 8);
			bits++;
		}
		} else if (command == 'R') {
			/* read */
		read_input[bits / 8] |= 1 << (bits % 8);
		read_bits++;
		} else if (command == 'r' || command == 's') {
			/* TRST = 0 */
		} else if (command == 't' || command == 'u') {
			/* TRST = 1 */
		}
	}

#ifndef RBB_NOT_HANDLE_LAST
	if (service->last_is_read) { /* Fix some TDO read issue */
		int firstbit = 0;
		read_input[firstbit / 8] |= 1 << (firstbit % 8);
		read_bits++;
	}
	if (command == 'R') {
		service->last_is_read = true;
		read_bits--;
	} else {
		service->last_is_read = false;
	}
#endif

	analyze_bitbang((uint8_t *)tms_input, bits, (uint8_t *)read_input, service);

	int j;
	uint8_t tms_buffer[RBB_BUFFERSIZE] = {};
	for (i = 0; i < service->region_count; i++) {
		if (service->regions[i].flip_tms) { /* Needs to flip TMS */

			memset(tms_buffer, 0x00, sizeof(tms_buffer));
			for (j = service->regions[i].begin; j < service->regions[i].end; j++) {
				uint8_t tms_bit = (tms_input[j / 8] >> (j % 8)) & 0x1;
				int off = j - service->regions[i].begin;
				tms_buffer[off / 8] |= tms_bit << (off % 8);
			}
			jtag_add_tms_seq(service->regions[i].end - service->regions[i].begin, tms_buffer,
				service->regions[i].endstate);
		}
	}

	int retval = jtag_execute_queue();
	if (retval != ERROR_OK)
		return retval;

	if (read_bits != 0) {
		connection_write(connection, send_buffer, read_bits);
		/* TODO: some post-processing */
	}
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
	service->last_is_read = 0;
	service->state = TAP_RESET;

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
