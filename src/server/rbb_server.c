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
#include <stdio.h>
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
	int buf_size;
	tap_state_t begin_state;
	tap_state_t end_state;
	uint8_t* tms_buffer;
	uint8_t* tdi_buffer;
	uint8_t* tdo_buffer;
	uint8_t* tdo_mask_buffer;
};

struct rbb_service {
	unsigned int channel;
	int last_is_read;
	tap_state_t state;
	struct jtag_region regions[64];
	int region_count;
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

static int rbb_connection_read (struct connection *connection, unsigned char* buffer, int* length)
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
	int tck = 0, tms, tdi;
	int bits = 0, read_bits = 0;
	int last_read_p = 0;
	// char input[8];
	// FILE* fp_input = fopen("D:\\work\\projs\\openocd_tester\\tools\\win\\bit_download_fail\\log\\td_input.log", "a");
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
			last_read_p = bits;		
			read_bits++;
		} else if (command == 'r' || command == 's') {
			/* TRST = 0 */
		} else if (command == 't' || command == 'u') {
			/* TRST = 1 */
		}

		// if (tck) {
		// 	memset(input, 0x00, sizeof(input));
		// 	input[0] = '1';
		// 	input[1] = tms ? '1' : '0';
		// 	input[2] = tdi ? '1' : '0';
		// 	input[3] = '\0';
		// 	fprintf(fp_input, "%s", input);
		// 	fprintf(fp_input, "\n");
		// }
	}

	// fclose(fp_input);

	if (read_bits == 1)
	{
		LOG_DEBUG("received length %lld bits %d last read %d", length, bits, last_read_p);
	}


#ifndef RBB_NOT_HANDLE_LAST
	if (service->last_is_read) { /* Fix some TDO read issue */
		LOG_INFO("last is read");
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

static void rbb_region_init(struct rbb_service* service, 
							uint8_t is_tms, uint8_t flip_tms,
							int shift_pos, int cur_pos, 
							tap_state_t cur_state, tap_state_t next_state,
							int total_read_bits)
{
	struct jtag_region *region = &(service->regions[service->region_count]);
	struct jtag_region *next_region = &(service->regions[service->region_count + 1]);
	region->is_tms = is_tms;
	region->flip_tms = flip_tms;
	region->begin = shift_pos;
	region->end = cur_pos + 1;
	region->end_state = cur_state;
	region->buf_size = (region->end - region->begin + 8 -1) / 8;

	region->tms_buffer = (uint8_t*)malloc(region->buf_size + 64);;
	region->tdi_buffer =(uint8_t*)malloc(region->buf_size + 64);
	if((total_read_bits > 0) && (is_tms == 0)) {
		region->tdo_buffer = (uint8_t*)malloc(region->buf_size + 64);
		region->tdo_mask_buffer = (uint8_t*)malloc(region->buf_size + 64);
	} else {
		region->tdo_buffer = NULL;
		region->tdo_mask_buffer = NULL;
	}

	next_region->begin_state = next_state;

	service->region_count ++;

}

static void analyze_bitbang(const uint8_t *tms, int total_bits, struct rbb_service *service,
							int total_read_bits)
{
	int shift_pos = 0;
	uint8_t is_tms;
	tap_state_t cur_state = service->state, new_state;
	service->region_count = 0;
	LOG_DEBUG("total bits %d read bits %d %s", total_bits, total_read_bits, tap_state_name(service->state));

	service->regions[0].begin_state = cur_state;
	for (int i = 0; i < total_bits; i++) {
		uint8_t tms_bit = (tms[i / 8] >> (i % 8)) & 0x1;

		new_state = next_state(cur_state, tms_bit);

		if ((cur_state != TAP_DRSHIFT && new_state == TAP_DRSHIFT) ||
			(cur_state != TAP_IRSHIFT && new_state == TAP_IRSHIFT) ||
			(cur_state != TAP_DRPAUSE && new_state == TAP_DRPAUSE) ||
			(cur_state != TAP_IRPAUSE && new_state == TAP_IRPAUSE)) {
			rbb_region_init(service, 1, 0, shift_pos, i, cur_state, new_state,total_read_bits);
			shift_pos = i + 1;
		} else if ((cur_state == TAP_DRSHIFT && new_state != TAP_DRSHIFT) ||
				   (cur_state == TAP_IRSHIFT && new_state != TAP_IRSHIFT) ||
				   (cur_state == TAP_DRPAUSE && new_state != TAP_DRPAUSE) ||
				   (cur_state == TAP_IRPAUSE && new_state != TAP_IRPAUSE)) {
			rbb_region_init(service, 0, 1, shift_pos, i, cur_state, new_state, total_read_bits);
			shift_pos = i + 1;
		} 
		cur_state = new_state;
	}

	service->state = cur_state;
	LOG_DEBUG("end service state %s", tap_state_name(service->state));

	if (shift_pos != total_bits) {
		is_tms = cur_state != TAP_IRSHIFT && cur_state != TAP_DRSHIFT;
		rbb_region_init(service, is_tms, 0, shift_pos, total_bits - 1, cur_state, new_state, total_read_bits);
	}
	LOG_DEBUG_IO("bit len = %d", total_bits);
}

static void analyze_bitbang_golden(const uint8_t *tms, int total_bits, struct rbb_service *service,
							const uint8_t *read_bits, int total_read_bits)
{
	int shift_pos = 0;
	uint8_t is_tms;
	uint8_t tdo_read_bit_old = read_bits[0] & 0x1;
	tap_state_t cur_state = service->state, new_state;
	service->region_count = 0;
	LOG_DEBUG("total bits %d read bits %d", total_bits, total_read_bits);

	service->regions[0].begin_state = cur_state;
	for (int i = 0; i < total_bits; i++) {
		uint8_t tms_bit = (tms[i / 8] >> (i % 8)) & 0x1;
		uint8_t tdo_read_bit = (read_bits[(i+1) / 8] >> ((i+1) % 8)) & 0x1;

		new_state = next_state(cur_state, tms_bit);

		if ((cur_state != TAP_DRSHIFT && new_state == TAP_DRSHIFT) ||
			(cur_state != TAP_IRSHIFT && new_state == TAP_IRSHIFT)) {
			rbb_region_init(service, 1, 0, shift_pos, i, cur_state, new_state,total_read_bits);
			shift_pos = i + 1;
		} else if ((cur_state == TAP_DRSHIFT && new_state != TAP_DRSHIFT) ||
				(cur_state == TAP_IRSHIFT && new_state != TAP_IRSHIFT) ) {
			rbb_region_init(service, 0, 1, shift_pos, i, cur_state, new_state, total_read_bits);
			shift_pos = i + 1;
		} else if ((tdo_read_bit_old != tdo_read_bit && cur_state == TAP_DRSHIFT && new_state == cur_state) ||
					(tdo_read_bit_old != tdo_read_bit && cur_state == TAP_IRSHIFT && new_state == cur_state)) {
			rbb_region_init(service, 0, 0, shift_pos, i, cur_state, new_state, total_read_bits);
			shift_pos = i + 1;
			tdo_read_bit_old = tdo_read_bit;
		} else if ((tdo_read_bit_old != tdo_read_bit && cur_state == TAP_DRSHIFT && new_state != cur_state) ||
					(tdo_read_bit_old != tdo_read_bit && cur_state == TAP_IRSHIFT && new_state != cur_state)) {
			rbb_region_init(service, 0, 1, shift_pos, i, cur_state, new_state, total_read_bits);
			shift_pos = i + 1;
			tdo_read_bit_old = tdo_read_bit;			
		}

		cur_state = new_state;
	}

	service->state = cur_state;
	if (shift_pos != total_bits) {
		is_tms = cur_state != TAP_IRSHIFT && cur_state != TAP_DRSHIFT;
		rbb_region_init(service, is_tms, 0, shift_pos, total_bits - 1, cur_state, new_state, total_read_bits);
	}
	LOG_DEBUG_IO("bit len = %d", total_bits);
}


#endif

static int rbb_create_out_buf(struct jtag_region* region, unsigned char* in_buf, unsigned char* out_buf)
{
	int i, offset;
	uint8_t in_bit;
	memset(out_buf, 0x00, region->buf_size + 64);

	for (i = region->begin; i < region->end; i++) {
		in_bit = (in_buf[i / 8] >> (i % 8)) & 0x1;
		offset = i - region->begin;
		out_buf[offset / 8] |= in_bit << (offset % 8);
	}

	return ERROR_OK;
}

static int rbb_add_tms_seq(struct jtag_region* region, unsigned char* tms_input, unsigned char* tdi_input)
{
	/* clear tms buffer */
	rbb_create_out_buf(region, tms_input, region->tms_buffer);
	rbb_create_out_buf(region, tdi_input, region->tdi_buffer);
	jtag_add_tms_seq(region->end - region->begin, region->tms_buffer, region->end_state);

	return ERROR_OK;
}

static int rbb_add_tdi_seq(struct jtag_region* region, 
						   unsigned char* tms_input, unsigned char* tdi_input, unsigned char* read_input,
						   tap_state_t last_st)
{
	tap_state_t next_st;
	// int last_req;
	if(region->tdo_mask_buffer != NULL)
	{
		rbb_create_out_buf(region, read_input, region->tdo_mask_buffer);
	}

	rbb_create_out_buf(region, tms_input, region->tms_buffer);
	rbb_create_out_buf(region, tdi_input, region->tdi_buffer);

	// last_req = region->flip_tms;

	// if(last_st == TAP_IRSHIFT || last_st == TAP_IRCAPTURE) {
	// 	next_st = last_req ? TAP_IREXIT1 : TAP_IRSHIFT;
	// } else if(last_st == TAP_DRSHIFT || last_st == TAP_DRCAPTURE) {
	// 	next_st = last_req ? TAP_DREXIT1 : TAP_DRSHIFT;
	// } else {
	// 	next_st = last_st;
	// }

	if (region->end_state == TAP_IRSHIFT)
		next_st = TAP_IREXIT1;
	else if (region->end_state == TAP_DRSHIFT)
		next_st = TAP_DREXIT1;
	else if (region->end_state == TAP_IRPAUSE)
		next_st = TAP_IREXIT2;
	else if (region->end_state == TAP_DRPAUSE)
		next_st = TAP_DREXIT2;
	else
	{
		LOG_ERROR("end state %s", tap_state_name(region->end_state));
		return ERROR_FAIL;
	}

	jtag_add_tdi_seq(region->end - region->begin, region->tdi_buffer, region->tdo_buffer, next_st);
	return ERROR_OK;
}

static int rbb_jtag_drive(struct rbb_service *service, size_t length, size_t total_bits,
						  unsigned char* tms_input, unsigned char* tdi_input, unsigned char* read_input)
{
	int i;
	tap_state_t last_st = TAP_IDLE;
	int retval = ERROR_OK;
	for (i = 0; i < service->region_count; i++) {
		if(service->regions[i].is_tms) {
			if(service->regions[i].tms_buffer == NULL) {
				LOG_ERROR("region %d tms buffer is NULL", i);
				return ERROR_FAIL;
			}
			rbb_add_tms_seq(&(service->regions[i]), tms_input, tdi_input);

		} else {
			if(service->regions[i].tdi_buffer == NULL) {
				LOG_ERROR("region %d tdi buffer is NULL", i);
				return ERROR_FAIL;
			}
			rbb_add_tdi_seq(&(service->regions[i]), tms_input, tdi_input, read_input, last_st);
			last_st = service->regions[i].end_state;
		}
	}

	retval = jtag_execute_queue();

	if(retval != ERROR_OK) {
		LOG_ERROR("rbb jtag execture fail");
		allow_tap_access = 0;
		return retval;
	}

	char vec_byte[8];
	uint8_t bit, mask;
	uint8_t tms_bit, tdi_bit, tdo_bit;
	int buf_byte_index, buf_bit_index;
	int j;
	FILE *fp_tdi = fopen("D:\\work\\projs\\openocd_tester\\tools\\win\\bit_download_fail\\log\\openocd_tdi.log", "a");
	FILE *fp_rbb = fopen("D:\\work\\projs\\openocd_tester\\tools\\win\\bit_download_fail\\log\\openocd_rbb.log", "a");
	FILE *fp_tdo = NULL;	

	fprintf(fp_tdi, "length %lld\n", length);
	for (i = 0; i < service->region_count; i++) {
		fprintf(fp_tdi, "region %d total size %lld is_tms %d st %s->%s start %d end %d size %d \n", 
				i, total_bits, 
				service->regions[i].is_tms,
				tap_state_name(service->regions[i].begin_state), tap_state_name(service->regions[i].end_state), 
				service->regions[i].begin, service->regions[i].end,
				service->regions[i].end - service->regions[i].begin);

		// for(j = 0; j < service->regions[i].buf_size; j++) {
		// 	// LOG_DEBUG("%02x", tdi_buffer[tdi_buffer_count][j]);
		// 	if(service->regions[i].tms_buffer != NULL)
		// 		fprintf(fp_tdi, "%02x ", service->regions[i].tms_buffer[j]);
		// 	else 
		// 		fprintf(fp_tdi, "XX ");

		// 	if(service->regions[i].tdi_buffer != NULL)
		// 		fprintf(fp_tdi, "%02x ", service->regions[i].tdi_buffer[j]);
		// 	else
		// 		fprintf(fp_tdi, "XX ");

		// 	if(service->regions[i].tdo_buffer != NULL)
		// 		fprintf(fp_tdi, "%02x ", service->regions[i].tdo_buffer[j]);
		// 	else
		// 		fprintf(fp_tdi, "XX ");
			
		// 	if(service->regions[i].tdo_mask_buffer != NULL)
		// 		fprintf(fp_tdi, "%02x ", service->regions[i].tdo_mask_buffer[j]);
		// 	else
		// 		fprintf(fp_tdi, "XX ");
			
		// 	fprintf(fp_tdi, "\n");
		// }

		for (j = service->regions[i].begin; j < (service->regions[i].end); j++) {
			buf_byte_index = (j - service->regions[i].begin) / 8;
			buf_bit_index = (j - service->regions[i].begin) % 8;
			if(service->regions[i].tms_buffer != NULL) {
				bit = (service->regions[i].tms_buffer[buf_byte_index] >> buf_bit_index) & 1;
				tms_bit = bit ? '1' : '0';
			}
			else
				tms_bit = '0';

			if(service->regions[i].tdi_buffer != NULL) {
				bit = (service->regions[i].tdi_buffer[buf_byte_index] >> buf_bit_index) & 1;
				tdi_bit = bit ? '1' : '0';
			} else
				tdi_bit = 'X';

			if(service->regions[i].tdo_buffer != NULL) {
				if(service->regions[i].tdo_mask_buffer != NULL) {
					mask = (service->regions[i].tdo_mask_buffer[buf_byte_index] >> buf_bit_index) & 1;
					bit = (service->regions[i].tdo_buffer[buf_byte_index] >> buf_bit_index) & 1;
					if(mask == 1) {
						tdo_bit = bit ? 'H' : 'L';
 					} else
						tdo_bit = 'X';
				}
				else {
					tdo_bit = 'X';
				}
			} else
				tdo_bit = 'X';

			memset(vec_byte, 0x00, sizeof(vec_byte));
			vec_byte[0] = '1';
			vec_byte[1] = tms_bit;
			vec_byte[2] = tdi_bit;
			vec_byte[3] = tdo_bit;
			vec_byte[3] = '\0';
			fprintf(fp_rbb, "%s\n", vec_byte);
		}
	}

	if(fp_tdi != NULL)
		fclose(fp_tdi);
	if(fp_tdo != NULL)
		fclose(fp_tdo);
	if(fp_rbb != NULL)
		fclose(fp_rbb);

	return retval;
}

static int rbb_send_buffer_gen(struct rbb_service *service, unsigned char* read_input,
							   unsigned char* read_output, size_t total_read_bits)
{
	int i, j;
	uint8_t read_en, read_bit;
	size_t read_p = 0;
	int retval = ERROR_OK;

	for (i = 0; i < service->region_count; i++) {
		if (service->regions[i].tdo_buffer != NULL) {
			for(j = 0; j < service->regions[i].end - service->regions[i].begin; j++) {
				read_en = service->regions[i].tdo_mask_buffer[j / 8] >> (j % 8) & 0x1;
				if(read_en) {
					read_bit = service->regions[i].tdo_buffer[j / 8] >> (j % 8) & 0x1;
					read_output[read_p++] = read_bit ? '1' : '0';
				}
			}
		}
	}

	read_output[read_p] = 0;

	if (read_p != total_read_bits) {
		LOG_ERROR("read_p %lld read bits %lld", read_p, total_read_bits);
		retval = ERROR_FAIL;
	}
	return retval;
}

static int free_rbb_buffer(struct rbb_service *service)
{
	int i;
	for (i = 0; i < service->region_count; i++) {
		if (service->regions[i].tms_buffer != NULL) {
			free(service->regions[i].tms_buffer);
			service->regions[i].tms_buffer = NULL;
		}
		if (service->regions[i].tdi_buffer != NULL) {
			free(service->regions[i].tdi_buffer);
			service->regions[i].tdi_buffer = NULL;
		}
		if (service->regions[i].tdo_buffer != NULL) {
			free(service->regions[i].tdo_buffer);
			service->regions[i].tdo_buffer = NULL;
		}
		if (service->regions[i].tdo_mask_buffer != NULL) {
			free(service->regions[i].tdo_mask_buffer);
			service->regions[i].tdo_mask_buffer = NULL;
		}
	}
	return ERROR_OK;
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

	struct rbb_service *service;
	int length, bytes_read;

	if(0) {
		retval = rbb_connection_check(connection);
		if(retval != ERROR_OK)
			return ERROR_OK;

		LOG_INFO("rbb connection check");
		memset(buffer, 0x00, sizeof(buffer));
		retval = rbb_connection_read(connection, buffer, &length);
		if(retval != ERROR_OK)
			return ERROR_SERVER_REMOTE_CLOSED;

		LOG_INFO("rbb connection read");
	}


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

	/* TODO: dirty call, don't do that */
	allow_tap_access = 1;
	bytes_read = connection_read(connection, buffer, sizeof(buffer) - 128);
	/* Needs to Lock the adapter driver, reject any other access */

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

	memset(tms_input, 0x00, sizeof(tms_input));
	memset(tdi_input, 0x00, sizeof(tdi_input));
	memset(read_input, 0x00, sizeof(read_input));	
	rbb_input_collect(service, buffer, length, 
					  tms_input, tdi_input, read_input,
					  &total_bits, &total_read_bits);

	FILE* fp_in = fopen("D:\\work\\projs\\openocd_tester\\tools\\win\\bit_download_fail\\log\\td_in.log", "a");
	for(int i = 0; i < length; i++)
		fprintf(fp_in, "%02x\n", buffer[i]);

	fclose(fp_in);
	if(1)
		analyze_bitbang((uint8_t *) tms_input, total_bits, service, total_read_bits);
	if(0)
		analyze_bitbang_golden((uint8_t *) tms_input, total_bits, service, (uint8_t *) read_input, total_read_bits);

	retval = rbb_jtag_drive(service, length, total_bits, tms_input, tdi_input, read_input);

	if(retval != ERROR_OK)
		return retval;

	if(total_read_bits != 0) {
		rbb_send_buffer_gen(service, read_input, send_buffer, total_read_bits);
		LOG_DEBUG("send buffer '%s'", send_buffer);
		connection_write(connection, send_buffer, total_read_bits);
	}

	free_rbb_buffer(service);

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
