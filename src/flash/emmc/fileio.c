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
	if (state->address % emmc->block_size) {
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

    state->block_size = emmc->block_size;
    state->block = malloc(emmc->block_size);

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
	emmc_fileio_init(state);

	unsigned minargs = need_size ? 4 : 3;
	if (minargs > CMD_ARGC)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct emmc_device *emmc;
	int retval = CALL_COMMAND_HANDLER(emmc_command_get_device, 0, &emmc);
	if (retval != ERROR_OK)
		return retval;

	if (!emmc->device) {
		command_print(CMD, "#%s: not probed", CMD_ARGV[0]);
		return ERROR_EMMC_DEVICE_NOT_PROBED;
	}

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], state->address);
	if (need_size) {
		COMMAND_PARSE_NUMBER(u32, CMD_ARGV[3], state->size);
		if (state->size % emmc->block_size) {
			command_print(CMD, "only block-aligned sizes are supported");
			return ERROR_COMMAND_SYNTAX_ERROR;
		}
	}

	retval = emmc_fileio_start(CMD, emmc, CMD_ARGV[1], filemode, state);
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
int emmc_fileio_read(struct emmc_device *emmc, struct emmc_fileio_state *s)
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


