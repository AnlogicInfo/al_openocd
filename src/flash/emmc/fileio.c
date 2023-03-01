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
	struct emmc_fileio_state *state)
{
	if (state->address % emmc->device->block_size) {
		command_print(cmd, "only block-aligned addresses are supported");
		return ERROR_COMMAND_SYNTAX_ERROR;
	}

	duration_start(&state->bench);

	if (filename) {
		int retval = fileio_open(&state->fileio, filename, filemode, FILEIO_BINARY);
		if (retval != ERROR_OK) {
			const char *msg = (filemode == FILEIO_READ) ? "read" : "write";
			command_print(cmd, "failed to open '%s' for %s access",
				filename, msg);
			return retval;
		}
		state->file_opened = true;
	}

    state->block_size = emmc->device->block_size;
    state->block = malloc(emmc->device->block_size);

	return ERROR_OK;
}
int emmc_fileio_cleanup(struct emmc_fileio_state *state)
{
	if (state->file_opened)
		fileio_close(state->fileio);

	free(state->block);
	state->block = NULL;
	return ERROR_OK;
}
int emmc_fileio_finish(struct emmc_fileio_state *state)
{
	emmc_fileio_cleanup(state);
	return duration_measure(&state->bench);
}

COMMAND_HELPER(emmc_fileio_parse_args, struct emmc_fileio_state *state,
	struct emmc_device **dev, enum fileio_access filemode,
	bool need_size)
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

	if(CMD_ARGC >= 2) {
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], state->address);
	}

	struct emmc_device *emmc;
	emmc = get_emmc_device_by_num(state->bank_num); 

	if (!emmc)
		return ERROR_FAIL;

	if (!emmc->device) {
		retval = CALL_COMMAND_HANDLER(emmc_command_auto_probe, state->bank_num, &emmc); // auto probe bank 0, need update index according to offset later
		if(retval != ERROR_OK)
		{
			LOG_ERROR("auto probe fail");
			command_print(CMD, " not probed");
			return ERROR_EMMC_DEVICE_NOT_PROBED;
		}
	}

	if (need_size) {
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], state->size);
		if (state->size % emmc->device->block_size) {
			command_print(CMD, "only block-aligned sizes are supported");
			return ERROR_COMMAND_SYNTAX_ERROR;
		}
	}

	retval = emmc_fileio_start(CMD, emmc, CMD_ARGV[0], filemode, state);
	if (retval != ERROR_OK)
		return retval;

	if (!need_size) {
		size_t filesize;
		retval = fileio_size(state->fileio, &filesize);
		if (retval != ERROR_OK)
			return retval;
		state->size = filesize;
	}

	*dev = emmc;

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

	total_size = (s->size / s->block_size + 1) * s->block_size;

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


