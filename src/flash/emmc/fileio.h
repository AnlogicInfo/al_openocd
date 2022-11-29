#ifndef OPENOCD_FLASH_EMMC_FILEIO_H
#define OPENOCD_FLASH_EMMC_FILEIO_H
#include <helper/time_support.h>
#include <helper/fileio.h>
struct emmc_fileio_state 
{
    uint32_t address;
    uint32_t size;
    uint8_t *block;
    uint32_t block_size;

    bool file_opened;
    struct fileio *fileio;

    struct duration bench;
};


void emmc_fileio_init(struct emmc_fileio_state *state);
int emmc_fileio_start(struct command_invocation *cmd,
		struct emmc_device *emmc, const char *filename, int filemode,
		struct emmc_fileio_state *state);
int emmc_fileio_cleanup(struct emmc_fileio_state *state);
int emmc_fileio_finish(struct emmc_fileio_state *state);

COMMAND_HELPER(emmc_fileio_parse_args, struct emmc_fileio_state *state,
	struct emmc_device **dev, enum fileio_access filemode,
	bool need_size);

int emmc_fileio_read(struct emmc_device *emmc, struct emmc_fileio_state *s);
#endif
