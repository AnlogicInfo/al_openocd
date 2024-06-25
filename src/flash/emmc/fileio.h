#ifndef OPENOCD_FLASH_EMMC_FILEIO_H
#define OPENOCD_FLASH_EMMC_FILEIO_H
#include <helper/time_support.h>
#include <helper/fileio.h>
#include <target/image.h>
struct emmc_fileio_state 
{
	uint32_t bank_num;
	target_addr_t address;
	uint8_t *block;
	uint32_t block_size;
	struct image image;
	bool file_opened;
	struct fileio *fileio;
	struct duration bench;
};


void emmc_fileio_init(struct emmc_fileio_state *state);
int emmc_fileio_start(struct command_invocation *cmd,
		struct emmc_device *emmc, const char *filename, int filemode,
		const char *filetype, struct emmc_fileio_state *state);
int emmc_fileio_cleanup(struct emmc_fileio_state *state);
int emmc_fileio_finish(struct emmc_fileio_state *state);

COMMAND_HELPER(emmc_fileio_parse_args, struct emmc_fileio_state *state,
	struct emmc_device **dev, enum fileio_access filemode);

int emmc_fileio_read_block(struct emmc_fileio_state *s);
int emmc_fileio_read(struct emmc_fileio_state *s);

#endif
