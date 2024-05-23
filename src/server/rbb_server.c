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
#include <jtag/interface.h>

#include "server.h"
#include "rbb_server.h"
#include <helper/time_support.h>

int allow_tap_access;
int arm_workaround;

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
	int64_t lasttime;
	int64_t backofftime;
	int64_t spacingtime;
	int allow_tlr;
};


static int rbb_new_connection(struct connection *connection)
{
	struct rbb_service *service;

	service = connection->service->priv;
	service->state = cmd_queue_cur_state;

	LOG_DEBUG("rbb: New connection for channel %u", service->channel);

	return ERROR_OK;
}

static int rbb_connection_closed(struct connection *connection)
{
	struct rbb_service *service;
	service = (struct rbb_service *)connection->service->priv;
	int retval = ERROR_OK;
	if (cmd_queue_cur_state != TAP_IDLE &&
		cmd_queue_cur_state != TAP_RESET) {
		LOG_INFO("Move TAP state from %s to IDLE", tap_state_name(cmd_queue_cur_state));

		/* If tap's current state not in idle */
		/* TODO: not issue JTAG RESET */
		retval = jtag_add_statemove(TAP_RESET);
		if (retval != ERROR_OK)
			LOG_ERROR("JTAG state move failed!");
		retval = jtag_add_statemove(TAP_IDLE);
		if (retval != ERROR_OK)
			LOG_ERROR("JTAG state move failed!");

		retval = jtag_execute_queue();
		if (retval != ERROR_OK)
			LOG_ERROR("JTAG queue execute failed!");
	}
	allow_tap_access = 0;
	arm_workaround = 1;
	service->lasttime = timeval_ms(); /* Record the time when client disconnects */
	LOG_DEBUG("rbb: Connection for channel %u closed", service->channel);

	return retval;
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
	LOG_DEBUG_IO("curstate = %s", tap_state_name(cur_state));
	service->region_count = 0;
	for (int i = 0; i < bits; i++) {
		uint8_t tms_bit = (tms[i / 8] >> (i % 8)) & 0x1;
		uint8_t tdo_read_bit = (read_bits[(i+1) / 8] >> ((i+1) % 8)) & 0x1;

		tap_state_t new_state = next_state(cur_state, tms_bit);
		/* LOG_DEBUG_IO("st %s -> %s, tms %d, pos %d, read %d",
			tap_state_name(cur_state), tap_state_name(new_state), tms_bit, i, tdo_read_bit); */
		if ((cur_state != TAP_DRSHIFT && new_state == TAP_DRSHIFT) ||
			(cur_state != TAP_IRSHIFT && new_state == TAP_IRSHIFT)) {
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = 1;
			region->flip_tms = 0;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			shift_pos = i + 1;
		} else if ((cur_state == TAP_DRSHIFT && new_state != TAP_DRSHIFT) ||
					(cur_state == TAP_IRSHIFT && new_state != TAP_IRSHIFT)) {
			/* end */
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = 0;
			region->flip_tms = 1;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			shift_pos = i + 1;

		} else if ((tdo_read_bit_old != tdo_read_bit && cur_state == TAP_DRSHIFT && new_state == cur_state) ||
					(tdo_read_bit_old != tdo_read_bit && cur_state == TAP_IRSHIFT && new_state == cur_state)) {
			/* skip some bits */
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = 0;
			region->flip_tms = 0;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			/* dprintf("region-> begin %d, end %d\n", region->begin, region->end); */

			shift_pos = i + 1;
			tdo_read_bit_old = tdo_read_bit;
		} else if ((tdo_read_bit_old != tdo_read_bit && cur_state == TAP_DRSHIFT && new_state != cur_state) ||
					(tdo_read_bit_old != tdo_read_bit && cur_state == TAP_IRSHIFT && new_state != cur_state)) {
			/* skip some bits */
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = 0;
			region->flip_tms = 1;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			/* dprintf("region-> begin %d, end %d\n", region->begin, region->end); */

			shift_pos = i + 1;
			tdo_read_bit_old = tdo_read_bit;
		}
		cur_state = new_state;
	}
	service->state = cur_state;
#if 0
	if (bits != 0) {
		if (service->region_count == 0) {
			service->region_count = 1;
			if (cur_state == TAP_DRSHIFT || cur_state == TAP_IRSHIFT) {
				struct jtag_region *region = &(service->regions[0]);
				region->is_tms = 0;
				region->flip_tms = 0;
				region->begin = 0;
				region->end = bits;
				region->endstate = cur_state;
			} else {
				struct jtag_region *region = &(service->regions[0]);
				region->is_tms = 1;
				region->flip_tms = 0;
				region->begin = 0;
				region->end = bits;
				region->endstate = cur_state;
			}
		} else {
			struct jtag_region *region = &(service->regions[service->region_count - 1]);
			int prev_end = region->end;
			if (prev_end != bits && service->region_count != 0) {
				region = &(service->regions[service->region_count]);
				region->flip_tms = 0; /* TODO: Maybe an issue*/

				if (cur_state == TAP_DRSHIFT || cur_state == TAP_IRSHIFT) {
					region->is_tms = 0;
					/* Need a better way */
				} else
					region->is_tms = 1;

				region->begin = prev_end + 1;
				region->end = bits;
				region->endstate = cur_state;
				service->region_count++;
				LOG_DEBUG("Add new region at last, start pos = %d, endpos = %d",
							region->begin, bits);
			}
		}
	}
#else
	if (shift_pos != bits) {
		struct jtag_region *region = &(service->regions[service->region_count++]);
		region->is_tms = cur_state != TAP_IRSHIFT && cur_state != TAP_DRSHIFT;
		region->flip_tms = 0;
		region->begin = shift_pos;
		region->end = bits;
		region->endstate = cur_state;
	}
#endif
	LOG_DEBUG_IO("bit len = %d", bits);
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

	/* LOG_DEBUG("called, st = %s", tap_state_name(cmd_queue_cur_state)); */

	if (allow_tap_access == 0) { /* Not occuppied by RBB */
		/* If the TAP state not in TLR or RTI, just return back */
		if (cmd_queue_cur_state != TAP_IDLE &&
			cmd_queue_cur_state != TAP_RESET)
			return ERROR_OK;

		if (jtag_command_queue != NULL)
			return ERROR_OK;
	}
	if (allow_tap_access == 3)
		return ERROR_OK;

	service = (struct rbb_service *)connection->service->priv;

	if (service->lasttime != 0 && allow_tap_access == 0) { /* More than one access cycle */
		int64_t curtime = timeval_ms();
		if ((curtime - service->lasttime) < service->spacingtime)
			return ERROR_OK; /* Wait for spacing time passed */
	}

	bytes_read = connection_read(connection, buffer, sizeof(buffer));
	/* Needs to Lock the adapter driver, reject any other access */
	/* TODO: dirty call, don't do that */
	allow_tap_access = 1;

	if (!bytes_read) {
		allow_tap_access = 0;
		return ERROR_SERVER_REMOTE_CLOSED;
	}
	else if (bytes_read < 0) {
#if 0
		errno = WSAGetLastError();
		if (errno == WSAECONNABORTED || errno == WSAECONNRESET) {
			allow_tap_access = 0;
			log_socket_error("rbb_server");
		}
		return ERROR_SERVER_REMOTE_CLOSED;
#else
		allow_tap_access = 0;
		LOG_ERROR("error during read: %s", strerror(errno));
		return ERROR_SERVER_REMOTE_CLOSED;
#endif
	}

	length = bytes_read;

#if 1
	char command;
	int bits = 0, read_bits = 0;
	size_t i;
	memset(tms_input, 0x00, sizeof(tms_input));
	memset(tdi_input, 0x00, sizeof(tdi_input));
	memset(read_input, 0x00, sizeof(read_input));
	LOG_DEBUG_IO("Here comes receive buffer");
	for (i = 0; i < length; i++) {
		command = buffer[i];

		if (debug_level >= LOG_LVL_DEBUG_IO)
			LOG_OUTPUT("%c", command);

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
	if (debug_level >= LOG_LVL_DEBUG_IO)
		LOG_OUTPUT("\n-\n");
	/* LOG_DEBUG("received length %lld, bits %d", length, bits); */

#ifndef RBB_NOT_HANDLE_LAST
	if (service->last_is_read) { /* Fix some TDO read issue */
		int firstbit = 0;
		read_input[firstbit / 8] |= 1 << (firstbit % 8);
		read_bits++;
	}
	if (command == 'R') {
		service->last_is_read = 1;
		read_bits--;
	} else {
		service->last_is_read = 0;
	}
#endif

	analyze_bitbang((uint8_t *)tms_input, bits, (uint8_t *)read_input, service);

	int j;
	int tms_buffer_count = 0;

	/* TODO: Dynamically assign the buffer and drop out RBB_MAX_BUF_COUNT for
		efficency and safety reason */

	uint8_t tms_buffer[RBB_MAX_BUF_COUNT][RBB_BUFFERSIZE];
	size_t tdi_buffer_count = 0;
	uint8_t tdi_buffer[RBB_MAX_BUF_COUNT][RBB_BUFFERSIZE];

	uint8_t tdo_buffer[RBB_MAX_BUF_COUNT][RBB_BUFFERSIZE];
	int tdo_read_bits[RBB_MAX_BUF_COUNT];

	tap_state_t last_st = TAP_IDLE;
	/* LOG_DEBUG_IO("region_count %lld", service->region_count); */

#if 0
	if (service->region_count == 0 && bits != 0) {
		int is_tms = 0;
		for (i = 0; i < (size_t)bits; i++) {
			uint8_t tms_bit = (tms_input[i / 8] >> (i % 8)) & 0x1;
			if (tms_bit) {
				is_tms = 1;
				break;
			}
		}
		if (is_tms)
			jtag_add_tms_seq(bits, tms_input,
					service->state);
		else
			jtag_add_tdi_seq(bits,
					tdi_input, NULL, service->state);
		assert(read_bits == 0);
	}
#endif
	for (i = 0; i < service->region_count; i++) {
		int do_read = 0;

		if (service->regions[i].is_tms) { /* Handle TAP state change */

			memset(tms_buffer[tms_buffer_count], 0x00, RBB_BUFFERSIZE);
			int first_tms_one_pos = -1;
			int cont_tms_count = 0;

			int tlr_startpos = -1;
			int tlr_endpos = -1;
			for (j = service->regions[i].begin; j < service->regions[i].end; j++) {
				uint8_t tms_bit = (tms_input[j / 8] >> (j % 8)) & 0x1;
				if (tms_bit != 1) {
					if (cont_tms_count >= 5) { /* 5 more ones, Issuing TLR-RST */
						tlr_endpos = j - 1;
						tlr_startpos = first_tms_one_pos;
					}
					first_tms_one_pos = -1;
					cont_tms_count = 0;
				}
				if (tms_bit == 1 && first_tms_one_pos == -1) {
					/* tms count start point */
					first_tms_one_pos = j;
				}
				if (tms_bit == 1 && first_tms_one_pos != -1) {
					/* count contingous tms bits */
					cont_tms_count++;
				}
				int off = j - service->regions[i].begin;
				tms_buffer[tms_buffer_count][off / 8] |= tms_bit << (off % 8);
			}

			if (service->allow_tlr == 0) {
				if (tlr_startpos != -1) {
					LOG_DEBUG("Blocking TLR-RESET, TLR start pos %d, TLR end pos %d", tlr_startpos, tlr_endpos);
					for (j = tlr_startpos; j <= tlr_endpos; j++) {
						uint8_t tms_data = tms_buffer[tms_buffer_count][j / 8];

						int ofs = j % 8;
						uint8_t mask = ~(1 << ofs); /* clear a specific bit */
						tms_data &= mask;

						tms_buffer[tms_buffer_count][j / 8] = tms_data;
					}
				}
			}
			jtag_add_tms_seq(service->regions[i].end - service->regions[i].begin, tms_buffer[tms_buffer_count],
				service->regions[i].endstate);

			tms_buffer_count++;
		} else {
			/* Handle SHIFT-DR/ SHIFT-IR */
			memset(tdi_buffer[tdi_buffer_count], 0x00, RBB_BUFFERSIZE);
			int warned = 0;
			for (j = service->regions[i].begin; j < service->regions[i].end; j++) {
				uint8_t tdi_bit = (tdi_input[j / 8] >> (j % 8)) & 0x1;
				int off = j - service->regions[i].begin;
				tdi_buffer[tdi_buffer_count][off / 8] |= tdi_bit << (off % 8);

				uint8_t read_bit = (read_input[j / 8] >> (j % 8)) & 0x1;
				if (read_bit) {
					do_read = 1;
					/* Raise a flag */
				} else {
					/* Needed some tweaking for unused bits for performance */
					/* TODO: May needs some post-processing */
				}
				/* verify our assumption: all read_bit remains the same */
				assert(do_read == read_bit);
				if (do_read != read_bit) {
					if (warned == 0) {
						LOG_ERROR("do read in region %d mismatch, off = %d!", (int)i, off);
						int k;
						for (k = 0; k < bytes_read; k++)
							LOG_OUTPUT("%c", buffer[k]);
						warned = 1;
					}
				}
			}

			if (do_read != 0) {
				tdo_read_bits[tdi_buffer_count] =
					service->regions[i].end - service->regions[i].begin;
				/* LOG_DEBUG("buf %lld, bits %d", tdi_buffer_count, tdo_read_bits[tdi_buffer_count]); */
			}
			else
				tdo_read_bits[tdi_buffer_count] = 0;

			uint8_t *scan_out_buf;
			if (do_read == 0)
				scan_out_buf = NULL;
			else
				scan_out_buf = tdo_buffer[tdi_buffer_count];

			int last_req = service->regions[i].flip_tms; /* Needs to flip TMS */
			if (last_st == TAP_IRSHIFT || last_st == TAP_IRCAPTURE)
				jtag_add_tdi_seq(service->regions[i].end - service->regions[i].begin,
					tdi_buffer[tdi_buffer_count], scan_out_buf, last_req ? TAP_IREXIT1 : TAP_IRSHIFT);
			else if (last_st == TAP_DRSHIFT || last_st == TAP_DRCAPTURE)
				jtag_add_tdi_seq(service->regions[i].end - service->regions[i].begin,
					tdi_buffer[tdi_buffer_count], scan_out_buf, last_req ? TAP_DREXIT1 : TAP_DRSHIFT);
			else /* Probably just RTI/PDR/PIR */
				jtag_add_tdi_seq(service->regions[i].end - service->regions[i].begin,
					tdi_buffer[tdi_buffer_count], scan_out_buf, last_st);

			tdi_buffer_count++;
		}
		last_st = service->regions[i].endstate;
#if 1
		LOG_DEBUG("region %d, type %s, start %d, size %d, ftms %d, do_read %d, end state %s", (int)i,
					service->regions[i].is_tms ? "TMS" : "TDI", service->regions[i].begin,
					service->regions[i].end - service->regions[i].begin, service->regions[i].flip_tms,
					do_read, tap_state_name(last_st));
#endif
	}

	int retval = jtag_execute_queue();
	if (retval != ERROR_OK) {
		allow_tap_access = 0; /* Need to release the TAP */
		return retval;
	}

	int tdo_bits_p = 0;
	if (read_bits != 0) {
		for (i = 0; i < tdi_buffer_count; i++) { /* Post-process the buffer into RBB style buffer */
			LOG_DEBUG_IO("buf %d, bits %d", tdo_read_bits[i], tdo_read_bits[i]);
			if (tdo_read_bits[i] != 0)
				for (j = 0; j < tdo_read_bits[i]; j++) {
					uint8_t tdo_bit = (tdo_buffer[i][j / 8] >> (j % 8)) & 0x1;
					send_buffer[tdo_bits_p++] = tdo_bit ? '1' : '0';
				}
		}
		assert(tdo_bits_p == read_bits);
		LOG_DEBUG("read_bits %d, tdo_bits %d, tdi cnt %d", read_bits, tdo_bits_p,
						(int)tdi_buffer_count);

		send_buffer[tdo_bits_p] = 0;
		LOG_DEBUG("send buffer '%s'", send_buffer);
		connection_write(connection, send_buffer, read_bits);
	}
#else
	/* New pipelined FSM */
	/* need an ringbuffer to store all data */
	/* functions we need:
		rbb_buf_add(inbuffer, length); // Adds data to the ringbuffer
		rbb_buf_getsize();	// Get current data count
		rbb_buf_getdata(); // Readout a control word, and increase write pointer
		rbb_buf_getrp(); // Get current read pointer
		rbb_buf_rstrp(rp); // Reset read pointer to a specification value

		How FSM works:
		read an control word from ringbuffer
		run JTAG FSM
	*/
#endif

#ifdef RBB_RELEASE_FAST
	if (cmd_queue_cur_state == TAP_IDLE || cmd_queue_cur_state == TAP_RESET)
		allow_tap_access = 0;
#endif

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

	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	service = malloc(sizeof(struct rbb_service));

	if (!service)
		return ERROR_FAIL;

	service->channel = 0;
	service->last_is_read = 0;
	service->state = TAP_RESET;
	service->lasttime = 0;

	ret = add_service(&rbb_service_driver, CMD_ARGV[0], CONNECTION_LIMIT_UNLIMITED, service);

	if (CMD_ARGC >= 2)
		service->spacingtime = atoi(CMD_ARGV[1]);
	else
		service->spacingtime = 100; /* 100ms */

	if (CMD_ARGC >= 3)
		service->backofftime = atoi(CMD_ARGV[2]);
	else
		service->backofftime = 100; /* 100ms */

	if (CMD_ARGC >= 4)
		service->allow_tlr = atoi(CMD_ARGV[3]);
	else
		service->allow_tlr = 1; /* allow TLR */


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
	 .usage = "<port> [spacing time] [backofftime] [allow_tlr]"},
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
	 .help = "Remote Bitbang server for Anlogic TD",
	 .usage = "",
	 .chain = rbb_server_command_handlers},
	COMMAND_REGISTRATION_DONE};

int rbb_server_register_commands(struct command_context *ctx)
{
	return register_commands(ctx, NULL, rbb_command_handlers);
}
