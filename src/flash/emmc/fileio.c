/*
 * File: fileio.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-11-29
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-11-29
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "core.h"
#include "fileio.h"

void emmc_fileio_init(struct emmc_fileio_state *state)
{
    memset(state, 0, sizeof(*state));
}

int emmc_fileio_start(struct command_invocation *cmd,
	struct emmc_device *emmc, const char *filename, int filemode,
	const char *filetype, struct emmc_fileio_state *state)
{
	int retval;
	if (state->address % emmc->device->block_size) {
		LOG_ERROR("only block-aligned addresses are supported");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	duration_start(&state->bench);

	retval = image_open(&state->image, filename, filetype);
	if (retval != ERROR_OK) {
		LOG_ERROR("failed to open '%s'", filename);
		return retval;
	}

	state->file_opened = true;
	state->block_size = emmc->device->block_size;

	return ERROR_OK;
}

int emmc_fileio_cleanup(struct emmc_fileio_state *state)
{
	if (state->file_opened)
		image_close(&state->image);
	return ERROR_OK;
}

int emmc_fileio_finish(struct emmc_fileio_state *state)
{
	int retval;
	emmc_fileio_cleanup(state);
	retval = duration_measure(&state->bench);
	return retval;
}

/* parse cmd and open image */
COMMAND_HELPER(emmc_fileio_parse_args, struct emmc_fileio_state *state,
	struct emmc_device **dev, enum fileio_access filemode)
{
	int retval;
	emmc_fileio_init(state);

	while (CMD_ARGC) {
		if (strcmp(CMD_ARGV[0], "erase") == 0) {
			CMD_ARGV++;
			CMD_ARGC--;
		} else
			break;
	}

	if (CMD_ARGC < 1)
	{
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	/* parse start addr */
	if(CMD_ARGC >= 2) {
		COMMAND_PARSE_NUMBER(llong, CMD_ARGV[1], state->image.base_address);
		state->image.base_address_set = true;
		state->address = state->image.base_address;
	}

	struct emmc_device *emmc;
	emmc = get_emmc_device_by_num(state->bank_num); 

	if (!emmc)
		return ERROR_FAIL;

	*dev = emmc;

	if (!emmc->device) {
		retval = CALL_COMMAND_HANDLER(emmc_command_auto_probe, state->bank_num, &emmc); // auto probe bank 0, need update index according to offset later
		if(retval != ERROR_OK)
		{
			LOG_ERROR("auto probe fail");
			command_print(CMD, " not probed");
			return ERROR_EMMC_DEVICE_NOT_PROBED;
		}
	}


	retval = emmc_fileio_start(CMD, emmc, CMD_ARGV[0], filemode, (CMD_ARGC >= 3) ? CMD_ARGV[2] : NULL
														, state);
	if (retval != ERROR_OK)
		return retval;

	return ERROR_OK;
}

/**
 * @returns If no error occurred, returns number of bytes consumed;
 * otherwise, returns a negative error code.)
 */
int emmc_fileio_read_block(struct emmc_fileio_state *s)
{
	size_t total_read = 0;
	size_t one_read;

	if (s->block) {
		fileio_read(s->fileio, s->block_size, s->block, &one_read);
		if (one_read < s->block_size)
			memset(s->block + one_read, 0xff, s->block_size - one_read);
		total_read += one_read;
	}
	return total_read;
}

int emmc_fileio_read(struct emmc_fileio_state *s)
{
	size_t one_read;
	uint32_t total_size;

	total_size = (s->image.size / s->block_size + 1) * s->block_size;

	s->block = malloc(total_size);
	if(s->block) 
	{
		fileio_read(s->fileio, total_size, s->block, &one_read);
		if(one_read < total_size)
			memset(s->block + one_read, 0xff, total_size - one_read);
	}

	// for(uint32_t i=0; i<total_size; i++)
	// 	LOG_INFO("file rd index %x val %x", i, *(s->block + i));

	return total_size;
}


