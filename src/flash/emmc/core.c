/*
 * File: core.c
 * Author: Tianyi Wang (tianyi.wang@anlogic.com)
 * Date:  2022-11-02
 * Modified By: Tianyi Wang (tianyi.wang@anlogic.com>)
 * Last Modified: 2022-11-02
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"

struct emmc_device *emmc_devices;

/* configured EMMC devices and EMMC Flash command handler */
void emmc_device_add(struct emmc_device *c)
{
	if (emmc_devices) {
		struct emmc_device *p = emmc_devices;
		while (p && p->next)
			p = p->next;
		p->next = c;
	} else
		emmc_devices = c;
}

/*	Chip ID list
* Manufacturer ID, product name, blocksize, chipsize in MegaByte, name
 */

static struct emmc_info emmc_flash_ids[] = 
{

    {EMMC_MFR_SAMSUNG, 0x38474E443352, 0x2000, "Samsung KLM8G1GEND-B031 8GB EMMC "},
    {0, 0, 0, NULL},

};
/**
 * Returns the flash bank specified by @a name, which matches the
 * driver name and a suffix (option) specify the driver-specific
 * bank number. The suffix consists of the '.' and the driver-specific
 * bank number: when two davinci banks are defined, then 'davinci.1' refers
 * to the second (e.g. DM355EVM).
 */
static struct emmc_device *get_emmc_device_by_name(const char* name)
{
	unsigned requested = get_flash_name_index(name);
	unsigned found = 0;

	struct emmc_device *emmc;
	for (emmc = emmc_devices; emmc; emmc = emmc->next) {
		if (strcmp(emmc->name, name) == 0)
			return emmc;
		if (!flash_driver_name_matches(emmc->controller->name, name))
			continue;
		if (++found < requested)
			continue;
		return emmc;
	}
	return NULL;
}


struct emmc_device *get_emmc_device_by_num(int num)
{
	struct emmc_device *p;
	int i = 0;

	for (p = emmc_devices; p; p = p->next) {
		if (i++ == num)
			return p;
	}

	return NULL;
}

COMMAND_HELPER(emmc_command_get_device, unsigned name_index,
	struct emmc_device **emmc)
{
	const char *str = CMD_ARGV[name_index];
	*emmc = get_emmc_device_by_name(str);
	if (*emmc)
		return ERROR_OK;

	unsigned num;
	COMMAND_PARSE_NUMBER(uint, str, num);
	*emmc = get_emmc_device_by_num(num);
	if (!*emmc) {
		command_print(CMD, "emmc flash device '%s' not found", str);
		return ERROR_COMMAND_SYNTAX_ERROR;
	}
	return ERROR_OK;
}


int emmc_read_status(struct emmc_device *emmc, uint8_t *status)
{

	return ERROR_OK;
}


// static int emmc_poll_ready(struct emmc_device *emmc, int timeout)
// {
//     return ERROR_OK;
// }

int emmc_probe(struct emmc_device *emmc)
{
    size_t device_id = 0;
    int i;

	emmc->controller->init(emmc);
	
    for (i = 0; emmc_flash_ids[i].name; i++)
    {
        if(emmc_flash_ids[i].id == device_id)
            emmc->device = &emmc_flash_ids[i];
        break;
    }

    LOG_INFO("found %s", emmc->device->name);

    return ERROR_OK;
}

int emmc_read_data_block(struct emmc_device *emmc, uint8_t *data, uint32_t block, uint32_t size)
{
    return ERROR_OK;
}

int emmc_write_data_block(struct emmc_device *emmc, uint8_t *data, uint32_t block, uint32_t size)
{
    return ERROR_OK;
}

