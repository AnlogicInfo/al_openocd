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
		LOG_DEBUG("Move TAP state from %s to IDLE", tap_state_name(cmd_queue_cur_state));

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

static int rbb_connection_check (struct connection *connection)
{
	struct rbb_service *service;
	int retval = -1;
	/* Check RBB connection */
	if (allow_tap_access == 0) {/* Not occuppied by RBB */
		/* If the TAP state not in TLR or RTI, just return back */
		if (cmd_queue_cur_state != TAP_IDLE &&
			cmd_queue_cur_state != TAP_RESET)
			return retval;

		if (jtag_command_queue != NULL)
			return retval;
	}

	if (allow_tap_access == 3)
		return retval;

	service = (struct rbb_service *)connection->service->priv;

	if (service->lasttime != 0 && allow_tap_access == 0) { /* More than one access cycle */
		int64_t curtime = timeval_ms();
		if ((curtime - service->lasttime) < service->spacingtime)
			return retval; /* Wait for spacing time passed */
	}

	retval = ERROR_OK;

	return retval;
}

static int rbb_connection_read (struct connection *connection, unsigned char* buffer, size_t* length)
{
	int bytes_read;
	allow_tap_access = 1;
	bytes_read = connection_read(connection, buffer, RBB_BUFFERSIZE + 1 - 128);
	if(!bytes_read) {
		allow_tap_access = 0;
		return ERROR_SERVER_REMOTE_CLOSED;
	}
	else if (bytes_read < 0) {
		allow_tap_access = 0;
		LOG_ERROR("error during read: %s", strerror(errno));
		return ERROR_SERVER_REMOTE_CLOSED;
	}

	*length = bytes_read;
	return ERROR_OK; 

}

static int rbb_input_collect (struct rbb_service *service , unsigned char* rbb_in_buffer, size_t length,
							  unsigned char* tms_input, unsigned char* tdi_input, unsigned char* read_input,
							  size_t* total_bits, size_t* total_read_bits)
{
	char command;
	size_t i;
	int tck, tms, tdi;
	int bits = 0, read_bits = 0;
	for (i = 0; i < length; i++)
	{
		command = rbb_in_buffer[i];
		// if(length < 400)
		// 	LOG_DEBUG("command %x", command);

		if ('0' <= command && command <= '7') {
			char offset = command - '0';
			tck = (offset >> 2) & 1;
			tms = (offset >> 1) & 1;
			tdi = (offset >> 0) & 1; 
			if (tck) {
				tms_input [bits / 8] |= tms << (bits % 8);
				tdi_input [bits / 8] |= tdi << (bits % 8);
				bits ++;
			} 		
		}
		else if (command == 'R') {
			/* read */
			read_input[bits / 8] |= 1 << (bits % 8);
			// if(length < 400)
			// 	LOG_DEBUG("read at %d buffer index %d value %x", bits, bits/8, read_input[bits / 8]);

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
		service->last_is_read = 1;
		read_bits--;
	} else {
		service->last_is_read = 0;
	}
#endif

	*total_bits = bits;
	*total_read_bits = read_bits;
	return ERROR_OK;
}

#define ANALYZE_COMPLETE

#ifdef ANALYZE_COMPLETE

static void analyze_bitbang(const uint8_t *tms, int total_bits, struct rbb_service *service)
{
	int shift_pos = 0;
	tap_state_t cur_state = service->state;
	LOG_DEBUG("curstate = %s", tap_state_name(cur_state));
	service->region_count = 0;
	for (int i = 0; i < total_bits; i++) {
		uint8_t tms_bit = (tms[i / 8] >> (i % 8)) & 0x1;

		tap_state_t new_state = next_state(cur_state, tms_bit);
		if ((cur_state != TAP_DRSHIFT && new_state == TAP_DRSHIFT) ||
			(cur_state != TAP_IRSHIFT && new_state == TAP_IRSHIFT)) {

			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = 1;
			region->flip_tms = 0;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;

			shift_pos = i + 1;
			LOG_DEBUG("st %s -> %s, begin %d end %d", tap_state_name(cur_state), tap_state_name(new_state), region->begin, region->end);

		} else if ((cur_state == TAP_DRSHIFT && new_state != TAP_DRSHIFT) ||
				(cur_state == TAP_IRSHIFT && new_state != TAP_IRSHIFT) ) {
			struct jtag_region *region = &(service->regions[service->region_count++]);
			region->is_tms = 0;
			region->flip_tms = 1;
			region->begin = shift_pos;
			region->end = i + 1;
			region->endstate = cur_state;
			shift_pos = i + 1;						
			LOG_DEBUG("st %s -> %s, begin %d end %d", tap_state_name(cur_state), tap_state_name(new_state), region->begin, region->end);

		} 

		cur_state = new_state;
	}

	service->state = cur_state;
	if (shift_pos != total_bits) {
		struct jtag_region *region = &(service->regions[service->region_count++]);
		region->is_tms = cur_state != TAP_IRSHIFT && cur_state != TAP_DRSHIFT;
		region->flip_tms = 0;
		region->begin = shift_pos;
		region->end = total_bits;
		region->endstate = cur_state;
	}
	LOG_DEBUG_IO("bit len = %d", total_bits);
}
#endif

static int rbb_add_tms_seq(struct jtag_region* region, unsigned char* tms_input, uint8_t* tms_output)
{
	int i;
	int in_byte_index, in_bit_index;
	int out_byte_index, out_bit_index;
	uint8_t tms_bit;

	/* clear tms buffer */
	memset(tms_output, 0x00, RBB_BUFFERSIZE);
	for (i = region->begin; i < region->end; i++) {
		in_byte_index = i / 8;
		in_bit_index = i % 8;
		tms_bit = (tms_input[in_byte_index] >> (in_bit_index)) & 0x1;
		out_byte_index = (i - region->begin) / 8;
		out_bit_index  = (i - region->begin) % 8;
		tms_output[out_byte_index] |= tms_bit << out_bit_index;
	}
	jtag_add_tms_seq(region->end - region->begin , tms_output, region->endstate);

	return ERROR_OK;
}

static int rbb_add_tdi_seq(struct jtag_region* region, unsigned char* tdi_input, uint8_t* tdi_output, 
						   uint8_t* scan_out_buf, tap_state_t last_st)
{
	int i;
	int in_byte_index, in_bit_index;
	int out_byte_index, out_bit_index;
	uint8_t tdi_bit;
	tap_state_t next_st;

	memset(tdi_output, 0x00, RBB_BUFFERSIZE);
	for (i = region->begin; i < region->end; i++) {
		in_byte_index = i / 8;
		in_bit_index = i % 8;
		tdi_bit = (tdi_input[in_byte_index] >> (in_bit_index)) & 0x1;
		out_byte_index = (i - region->begin) / 8;
		out_bit_index  = (i - region->begin) % 8;
		tdi_output[out_byte_index] |= tdi_bit << out_bit_index;
	}

	if (last_st == TAP_IRSHIFT || last_st == TAP_IRCAPTURE)
		next_st = TAP_IREXIT1;
	else if (last_st == TAP_DRSHIFT || last_st == TAP_DRCAPTURE)
		next_st = TAP_DREXIT1;
	else
		next_st = last_st;

	jtag_add_tdi_seq(region->end - region->begin, tdi_output, scan_out_buf, next_st);
	return ERROR_OK;
}

static int rbb_jtag_drive(struct rbb_service *service, unsigned char* tms_input, unsigned char* tdi_input,
						  uint8_t (*tdo_buffer)[RBB_BUFFERSIZE], int* tdo_buffer_count)
{
	size_t i;

	size_t tms_buffer_count = 0;
	size_t tdi_buffer_count = 0;

	tap_state_t last_st = TAP_IDLE;
	int retval = ERROR_OK;

	uint8_t tms_buffer[RBB_MAX_BUF_COUNT][RBB_BUFFERSIZE];
	uint8_t tdi_buffer[RBB_MAX_BUF_COUNT][RBB_BUFFERSIZE];

	for (i = 0; i < service->region_count; i++) {
		if(service->regions[i].is_tms) {
			rbb_add_tms_seq(&(service->regions[i]), tms_input, tms_buffer[tms_buffer_count]);
			tms_buffer_count ++;
		} else {
			rbb_add_tdi_seq(&(service->regions[i]), tdi_input, tdi_buffer[tdi_buffer_count], tdo_buffer[tdi_buffer_count], last_st);
			tdi_buffer_count ++;
		}
		last_st = service->regions[i].endstate;
	}

	*tdo_buffer_count = tdi_buffer_count;

	LOG_DEBUG("execute jtag queue");
	retval = jtag_execute_queue();
	if(retval != ERROR_OK) {
		LOG_ERROR("rbb jtag execture fail");
		allow_tap_access = 0;
		return retval;
	}
	return retval;
}

static int rbb_read_sel(struct jtag_region* region, unsigned char* read_input, uint8_t* read_mask, int* tdo_size)
{
	int i;
	int in_byte_index, in_bit_index;
	int out_byte_index, out_bit_index;
	uint8_t read_bit;
	
	memset(read_mask, 0x00, RBB_BUFFERSIZE);
	for (i = region->begin; i < region->end; i++) {
		in_byte_index = i / 8;
		in_bit_index = i % 8;
		read_bit = (read_input[in_byte_index] >> (in_bit_index)) & 0x1;
		out_byte_index = (i - region->begin) / 8;
		out_bit_index  = (i - region->begin) % 8;
		read_mask[out_byte_index] |= read_bit << out_bit_index;
	}
	*tdo_size = region->end - region->begin;
	return ERROR_OK;
}


static int rbb_send_buffer_gen(struct rbb_service *service, unsigned char* read_input,
							   uint8_t (*tdo_buffer)[RBB_BUFFERSIZE], int tdo_buffer_count, 
							   unsigned char* read_output, size_t total_read_bits)
{
	int i, j;
	uint8_t read_en, read_bit;
	size_t read_p = 0;
	uint8_t read_mask_buffer[RBB_MAX_BUF_COUNT][RBB_BUFFERSIZE];
	int tdo_size[RBB_MAX_BUF_COUNT];
	int buffer_cnt = 0;
	int retval = ERROR_OK;
	int tdo_byte_cnt;
	for (i = 0; i < (int) service->region_count; i++) {
		if(!service->regions[i].is_tms) {
			rbb_read_sel(&(service->regions[i]), read_input, read_mask_buffer[buffer_cnt], &(tdo_size[buffer_cnt]));
			buffer_cnt ++;
		}
	}
	LOG_DEBUG("tdo buf cnt %d", tdo_buffer_count);
	for(i = 0; i < tdo_buffer_count; i++) {
		LOG_DEBUG("buf %d size %d", i, tdo_size[i]);
		for (tdo_byte_cnt = 0; tdo_byte_cnt < (tdo_size[i]/8 + 1); tdo_byte_cnt ++) {
			LOG_DEBUG("byte %d value %x mask %x", tdo_byte_cnt, tdo_buffer[i][tdo_byte_cnt], read_mask_buffer[i][tdo_byte_cnt]);
		}

		for (j = 0; j < tdo_size[i]; j++) {
			read_en = (read_mask_buffer[i][j / 8] >> (j % 8)) & 0x1;
			if(read_en) {
				read_bit = (tdo_buffer[i][j / 8] >> (j % 8)) & 0x1;
				read_output[read_p/8] |= read_bit << (read_p%8);
				read_p ++;
			}
		}
	}

	if (read_p != total_read_bits) {
		LOG_ERROR("read_p %lld read bits %lld", read_p, total_read_bits);
		retval = ERROR_FAIL;
	}
	return retval;
}

static int rbb_input(struct connection *connection)
{
	int retval;
	size_t total_bits = 0, total_read_bits = 0;
	unsigned char buffer[RBB_BUFFERSIZE + 1];
	unsigned char tms_input[RBB_BUFFERSIZE];
	unsigned char tdi_input[RBB_BUFFERSIZE];
	unsigned char read_input[RBB_BUFFERSIZE];
	unsigned char send_buffer[RBB_BUFFERSIZE];
	uint8_t tdo_buffer[RBB_MAX_BUF_COUNT][RBB_BUFFERSIZE];
	int tdo_buffer_count= 0;

	struct rbb_service *service;
	size_t length;
	retval = rbb_connection_check(connection);
	if(retval != ERROR_OK)
		return ERROR_OK;

	retval = rbb_connection_read(connection, buffer, &length);
	if(retval != ERROR_OK)
		return ERROR_SERVER_REMOTE_CLOSED;

	service = (struct rbb_service *)connection->service->priv;

	memset(tms_input, 0x00, sizeof(tms_input));
	memset(tdi_input, 0x00, sizeof(tdi_input));
	memset(read_input, 0x00, sizeof(read_input));	
	LOG_INFO("rbb input collect");
	rbb_input_collect(service, buffer, length, 
					  tms_input, tdi_input, read_input,
					  &total_bits, &total_read_bits);
	for (int i = 0; i <10; i++)
		LOG_DEBUG("read input %x", read_input[i]);
	LOG_INFO("received length %lld total bits %lld total read bits %lld", length, total_bits, total_read_bits);

	analyze_bitbang((uint8_t *) tms_input, total_bits, service);

	LOG_INFO("region count %d", (int) (service->region_count));

	retval = rbb_jtag_drive(service, tms_input, tdi_input,
							tdo_buffer, &tdo_buffer_count);	
	if(retval != ERROR_OK)
		return retval;

	rbb_send_buffer_gen(service, read_input, 
						tdo_buffer, tdo_buffer_count, 
						send_buffer, total_read_bits);

	LOG_DEBUG("send buffer '%s'", send_buffer);
	connection_write(connection, send_buffer, total_read_bits);

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
