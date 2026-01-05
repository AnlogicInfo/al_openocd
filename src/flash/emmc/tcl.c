/*
 * File: tcl.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-10-31
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-10-31
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "core.h"
#include "imp.h"
#include "fileio.h"
#include <helper/time_support.h>

extern struct emmc_device *emmc_devices;


// multi emmc support
COMMAND_HANDLER(handle_emmc_list_command)
{
	struct emmc_device *p;
	int i;

	if (!emmc_devices) {
		command_print(CMD, "no emmc flash devices configured");
		return ERROR_OK;
	}

	for (p = emmc_devices, i = 0; p; p = p->next, i++) {
		if (p->device)
			command_print(CMD, "#%i: %s blocksize: %i",
				i, p->device->name,
				p->device->block_size);
		else
			command_print(CMD, "#%i: not probed", i);
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_emmc_probe_command)
{
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct emmc_device *emmc;
	int retval = CALL_COMMAND_HANDLER(emmc_command_get_device, 0, &emmc);
	if (retval != ERROR_OK)
		return retval;

	retval = emmc_probe(emmc);
	if (retval == ERROR_OK) {
		command_print(CMD, "emmc flash probed");
		// command_print(CMD, "EMMC flash device '%s' found", p->device->name);
	}

	return retval;
}

COMMAND_HANDLER(handle_emmc_write_block_command)
{
	int retval = ERROR_OK;
	uint32_t start_block, value, len;
	uint8_t *buffer = NULL;
	struct emmc_device *emmc;

	if (CMD_ARGC != 3)
		return ERROR_COMMAND_SYNTAX_ERROR;

	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], start_block);
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], value);
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], len);

	emmc = get_emmc_device_by_num(0);
	if (!emmc) {
		LOG_ERROR("emmc get device error");
		return ERROR_FAIL;
	}
	if (!emmc->device) {
		LOG_ERROR("emmc not probed");
		return ERROR_FAIL;
	}

	buffer = malloc(emmc->device->block_size);
	if (!buffer) {
		LOG_ERROR("Out of memory");
		return ERROR_FAIL;
	}

	memset(buffer, (uint8_t)value, emmc->device->block_size);

	for (uint32_t i = 0; i < len; i++) {
		uint64_t addr_bytes = ((uint64_t)(start_block + i)) * emmc->device->block_size;
		retval = emmc_write_data_block(emmc, (uint32_t *)buffer, (uint32_t)addr_bytes);
		if (retval != ERROR_OK)
			break;
	}

	free(buffer);
	return retval;
}

COMMAND_HANDLER(handle_emmc_write_image_command)
{
	struct emmc_device *emmc = NULL;
	struct emmc_fileio_state s;
	size_t buf_cnt = 0;
	size_t write_size = 0;
	size_t total_write_size = 0;
	int retval;

	retval= CALL_COMMAND_HANDLER(emmc_fileio_parse_args,
			&s, &emmc, FILEIO_READ);
	if(retval != ERROR_OK) 
		return retval;

	/* concatenate sections into one buffer and prepare mailbox boundaries */
	s.section_block_addrs = NULL;
	s.section_offsets_blocks = NULL;
	s.section_sizes_blocks = NULL;
	if (s.image.num_sections > 0) {
		s.section_block_addrs = (uint32_t *)calloc(s.image.num_sections, sizeof(uint32_t));
		s.section_offsets_blocks = (uint32_t *)calloc(s.image.num_sections, sizeof(uint32_t));
		s.section_sizes_blocks = (uint32_t *)calloc(s.image.num_sections, sizeof(uint32_t));
		if (!s.section_block_addrs || !s.section_offsets_blocks || !s.section_sizes_blocks) {
			free(s.section_block_addrs);
			free(s.section_offsets_blocks);
			free(s.section_sizes_blocks);
			emmc_fileio_cleanup(&s);
			return ERROR_FAIL;
		}
	}

	s.block = NULL;
	for (unsigned int i = 0; i < s.image.num_sections; i++) {
		size_t sz = s.image.sections[i].size;
		if (sz % s.block_size != 0)
			write_size = (sz / s.block_size + 1) * s.block_size;
		else
			write_size = sz;
		s.section_block_addrs[i] = (uint32_t)(s.image.sections[i].base_address / s.block_size);
		s.section_offsets_blocks[i] = (uint32_t)(total_write_size / s.block_size);
		s.section_sizes_blocks[i] = (uint32_t)(write_size / s.block_size);

		uint8_t *tmp = (uint8_t *)realloc(s.block, total_write_size + write_size);
		if (!tmp) {
			free(s.block);
			free(s.section_block_addrs);
			free(s.section_offsets_blocks);
			free(s.section_sizes_blocks);
			emmc_fileio_cleanup(&s);
			return ERROR_FAIL;
		}
		s.block = tmp;

		retval = image_read_section(&s.image, i, 0x0, s.image.sections[i].size, s.block + total_write_size, &buf_cnt);
		if (retval != ERROR_OK) {
			free(s.block);
			free(s.section_block_addrs);
			free(s.section_offsets_blocks);
			free(s.section_sizes_blocks);
			emmc_fileio_cleanup(&s);
			return retval;
		}
		if (buf_cnt < s.image.sections[i].size)
			memset(s.block + total_write_size + buf_cnt, 0xff, write_size - buf_cnt);
		LOG_INFO("section %x write_size %"PRIx64, i, write_size);
		total_write_size += write_size;
	}

	LOG_INFO("num_sections %d", s.image.num_sections);
	unsigned int mb_count = s.image.num_sections;
	LOG_INFO("config mailbox sections count %u", mb_count);
	{
		int ms_ret = target_set_mailbox_sections(s.section_offsets_blocks, s.section_block_addrs, s.section_sizes_blocks, mb_count);
		LOG_INFO("target_set_mailbox_sections returned %d", ms_ret);
	}

	retval = emmc_write_image(emmc, s.block, s.image.sections[0].base_address, (int)total_write_size);

	{
		int ms_ret2 = target_set_mailbox_sections(NULL, NULL, NULL, 0);
		LOG_INFO("target_set_mailbox_sections(clear) returned %d", ms_ret2);
	}

	free(s.block);
	free(s.section_block_addrs);
	free(s.section_offsets_blocks);
	free(s.section_sizes_blocks);

	if (emmc_fileio_finish(&s) == ERROR_OK) {
		command_print(CMD, "wrote file %s to EMMC flash %d up to "
									"offset 0x%8.8" PRIx64 " in %fs (%0.3f KiB/s)",
									CMD_ARGV[0], s.bank_num, s.address, duration_elapsed(&s.bench),
									duration_kbps(&s.bench, s.image.size));
	}

	return retval;
}

COMMAND_HANDLER(handle_emmc_read_block_command)
{
    int retval;
    uint32_t block_cnt = 1, byte_cnt;
    uint8_t *buffer = NULL;

    struct target *target = get_current_target(CMD_CTX);
    struct emmc_device *emmc;

    if (CMD_ARGC < 1 || CMD_ARGC > 2)
        return ERROR_COMMAND_SYNTAX_ERROR;

    uint32_t block_addr;
    COMMAND_PARSE_NUMBER(u32, CMD_ARGV[0], block_addr);

    if(CMD_ARGC == 2)
        COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], block_cnt);

    emmc = get_emmc_device_by_num(0);
    if (!emmc)
    {
        LOG_INFO("emmc get device error");
        retval = ERROR_FAIL;
        goto fail;
    }

    if(!emmc->device)
    {
        LOG_INFO("emmc not probed");
        retval = ERROR_FAIL;
        goto fail;
    }

    byte_cnt = emmc->device->block_size * block_cnt;
    LOG_INFO("emmc read block start block %u byte cnt %x", block_addr, byte_cnt);
    buffer = calloc(byte_cnt, 1);

    retval = emmc_read_data_block(emmc, (uint32_t*) buffer, block_addr);

    if(retval == ERROR_OK) {
        uint64_t base_byte_addr = (uint64_t)block_addr * emmc->device->block_size;
        target_handle_md_output(CMD, target, base_byte_addr, 1, byte_cnt, (uint8_t*) buffer,false);
    }

fail:
    free(buffer);
    return retval;
}

COMMAND_HANDLER(handle_emmc_verify_command)
{
	struct emmc_device *emmc = NULL;
	struct emmc_fileio_state s;
	size_t buf_cnt;

	int retval = CALL_COMMAND_HANDLER(emmc_fileio_parse_args,
			&s, &emmc, FILEIO_READ);
	if (retval != ERROR_OK)
		return retval;

	for (unsigned int i = 0; i < s.image.num_sections; i++) {
		s.block = malloc(s.image.sections[i].size);
		retval = image_read_section(&s.image, i, 0x0, s.image.sections[i].size, s.block, &buf_cnt);

		if (retval != ERROR_OK) {
			LOG_ERROR("read section fail");
			free(s.block);
			emmc_fileio_cleanup(&s);
			return retval;
		}

		retval = emmc_verify_image(emmc, s.block, s.image.sections[i].base_address, s.image.sections[i].size);

		if (retval != ERROR_OK) {
			LOG_ERROR("verify image fail");
			free(s.block);
			emmc_fileio_cleanup(&s);
			return retval;
		}

		free(s.block);
	}

	if (emmc_fileio_finish(&s) == ERROR_OK) {
		command_print(CMD, "verified file %s "
			"up to offset 0x%8.8" PRIx64 " in %fs (%0.3f KiB/s)",
			CMD_ARGV[0], s.address, duration_elapsed(&s.bench),
			duration_kbps(&s.bench, s.image.size));
	}

	return ERROR_OK;
}

COMMAND_HANDLER(handle_emmc_erase_command)
{
    if (CMD_ARGC != 1 && CMD_ARGC != 3)
        return ERROR_COMMAND_SYNTAX_ERROR;

    struct emmc_device *emmc;
    int retval = CALL_COMMAND_HANDLER(emmc_command_get_device, 0, &emmc);
    if (retval != ERROR_OK)
        return retval;

    if (!emmc->device || emmc->device->chip_size == 0) {
        retval = CALL_COMMAND_HANDLER(emmc_command_auto_probe, 0, &emmc);
        if (retval != ERROR_OK)
            return retval;
    }

    if (CMD_ARGC == 3) {
        uint32_t start_block, end_block;
        COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], start_block);
        if (strcmp(CMD_ARGV[2], "last") == 0) {
            end_block = (emmc->num_blocks > 0) ? (emmc->num_blocks - 1) : 0;
        } else {
            COMMAND_PARSE_NUMBER(u32, CMD_ARGV[2], end_block);
        }
        retval = emmc_erase_block(emmc, start_block, end_block);
        if (retval == ERROR_OK)
            command_print(CMD, "emmc erase blocks [%u - %u] successful", start_block, end_block);
        else
            command_print(CMD, "emmc erase blocks [%u - %u] failed", start_block, end_block);
    }
	else {
		LOG_ERROR("please specify start_block and end_block");
		retval = ERROR_COMMAND_SYNTAX_ERROR;
	}
    return retval;
}

static const struct command_registration emmc_exec_command_handlers[] = {
	{
		.name = "list",
		.handler = handle_emmc_list_command,
		.mode = COMMAND_EXEC,
		.help = "list configured EMMC flash devices",
		.usage = "",
	},
	{
		.name = "probe",
		.handler = handle_emmc_probe_command,
		.mode = COMMAND_EXEC,
		.usage = "bank_id",
		.help = "identify EMMC flash device",
	},
	{
		.name = "write_block",
		.handler = handle_emmc_write_block_command,
		.mode = COMMAND_EXEC,
		.usage = "start_block value len",
		.help = "Write len blocks starting at start_block with byte value",
	},	
	{
		.name = "write_image",
		.handler = handle_emmc_write_image_command,
		.mode = COMMAND_EXEC,
		.usage = "filename offset",
		.help = "Write an image to flash. "
			"Allow optional offset from beginning of bank (defaults to zero)",	
	},
	{
		.name = "read_block",
		.handler = handle_emmc_read_block_command,
		.mode = COMMAND_EXEC,
		.usage = "start_block [block_cnt]",
		.help = "Read blk from emmc",
	},
    {
        .name = "verify_image",
        .handler = handle_emmc_verify_command,
        .mode = COMMAND_EXEC,
        .usage = "bank_id filename offset ",
        .help = "Verify an image against emmc. Allow optional "
            "offset from beginning of bank (defaults to zero)",
    }, 
    {
        .name = "erase_block",
        .handler = handle_emmc_erase_command,
        .mode = COMMAND_EXEC,
        .usage = "bank_id [start_block end_block]",
        .help = "Erase entire emmc or range by blocks",
    },

    COMMAND_REGISTRATION_DONE
};

static int emmc_init(struct command_context *cmd_ctx)
{
    if (!emmc_devices)
        return ERROR_OK;

    return register_commands(cmd_ctx, "emmc", emmc_exec_command_handlers);
}

// init emmc commands
COMMAND_HANDLER(handle_emmc_init_command)
{
	if (CMD_ARGC != 0)
		return ERROR_COMMAND_SYNTAX_ERROR;

	static bool emmc_initialized;
	if (emmc_initialized) {
		LOG_INFO("'emmc init' has already been called");
		return ERROR_OK;
	}
	emmc_initialized = true;

	return emmc_init(CMD_CTX);
}


static int emmc_list_walker(struct emmc_flash_controller *c, void *x)
{
	struct command_invocation *cmd = x;
	command_print(cmd, "  %s", c->name);
	return ERROR_OK;
}


COMMAND_HANDLER(handle_emmc_list_drivers)
{
	command_print(CMD, "Available emmc flash controller drivers:");
	return emmc_driver_walk(&emmc_list_walker, CMD);
}

static COMMAND_HELPER(create_emmc_device, const char *bank_name,
	struct emmc_flash_controller *controller)
{
	struct emmc_device *c;
	struct target *target;
	int retval;

	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;
	target = get_target(CMD_ARGV[1]);
	if (!target) {
		LOG_ERROR("invalid target %s", CMD_ARGV[1]);
		return ERROR_COMMAND_ARGUMENT_INVALID;
	}

	if (controller->commands) {
		retval = register_commands(CMD_CTX, NULL, controller->commands);
		if (retval != ERROR_OK)
			return retval;
	}
	c = malloc(sizeof(struct emmc_device));
	if (!c) {
		LOG_ERROR("End of memory");
		return ERROR_FAIL;
	}

	c->name = strdup(bank_name);
	c->target = target;
	c->controller = controller;
	c->controller_priv = NULL;
	c->device = NULL;
	c->next = NULL;

	retval = CALL_COMMAND_HANDLER(controller->emmc_device_command, c);
	if (retval != ERROR_OK) {
		LOG_ERROR("'%s' driver rejected emmc flash. Usage: %s", 
			controller->name,
			controller->usage);
		free(c);
		return retval;
	}

	if (!controller->usage)
		LOG_DEBUG("'%s' driver usage field missing", controller->name);

	emmc_device_add(c);

	return ERROR_OK;
}

// set update driver for emmc device
COMMAND_HANDLER(handle_emmc_device_command)
{
	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	/* save name and increment (for compatibility) with drivers */
	const char *bank_name = *CMD_ARGV++;
	CMD_ARGC--;

	const char *driver_name = CMD_ARGV[0];
	struct emmc_flash_controller *controller;
	controller = emmc_driver_find_by_name(CMD_ARGV[0]);
	if (!controller) {
		LOG_ERROR("No valid EMMC flash driver found (%s)", driver_name);
		return CALL_COMMAND_HANDLER(handle_emmc_list_drivers);
	}

	return CALL_COMMAND_HANDLER(create_emmc_device, bank_name, controller);
}


static const struct command_registration emmc_config_command_handlers[] = {
	{
		.name = "device",
		.handler = &handle_emmc_device_command,
		.mode = COMMAND_CONFIG,
		.help = "defines a new EMMC bank",
		.usage = "bank_id driver target [driver_options ...]",
	},
	{
		.name = "drivers",
		.handler = &handle_emmc_list_drivers,
		.mode = COMMAND_ANY,
		.help = "lists available EMMC drivers",
		.usage = ""
	},
	{
		.name = "init",
		.mode = COMMAND_CONFIG,
		.handler = &handle_emmc_init_command,
		.help = "initialize EMMC devices",
		.usage = ""
	},		
	COMMAND_REGISTRATION_DONE
};



static const struct command_registration emmc_command_handlers[] = {
	{
		.name = "emmc",
		.mode = COMMAND_ANY,
		.help = "EMMC flash command group",
		.usage = "",
		.chain = emmc_config_command_handlers,
	},

	COMMAND_REGISTRATION_DONE
};

int emmc_register_commands(struct command_context *cmd_ctx)
{
	return register_commands(cmd_ctx, NULL, emmc_command_handlers);
}
