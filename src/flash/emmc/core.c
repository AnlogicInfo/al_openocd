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
	// mfr             prd_cid         block_size   chip_size     name
    {EMMC_MFR_SAMSUNG, 0x004d43473847, 0x200,       64,           "Samsung KLMCG8GEAC-B031 8GB EMMC "},
	{EMMC_MFR_MICRON,  0x4e53304a3536, 0x200,       16,           "Micron MTFC16GAPALBH-IT 16GB EMMC "},
	{EMMC_MFR_MICRON,  0x4e52314a3536, 0x200,       16,           "Micron MTFC16GAKAEJP-4MIT 16GB EMMC "},
	{EMMC_MFR_MICRON,  0x4e51324a3535, 0x200,       8,            "Micron MTFC8GAKAJCN-4MIT 8GB EMMC "},
	{EMMC_MFR_MICRON,  0x4e53304a3335, 0x200,       8,            "Micron MTFC8GAMALBH-AIT 8GB EMMC "},
	{EMMC_MFR_FORESEE, 0x033538413436, 0x200,       16,           "Foresee FEMDNN016G-58A46 16GB EMMC "},
	{EMMC_MFR_FORESEE, 0x034133413536, 0x200,       128,          "Foresee FEMDNN128G-A3A56 128GB EMMC "},
	{EMMC_MFR_HYNIX,   0x4a4838473461, 0x200,       8,            "Hynix H26M41208HPR 8GB EMMC "},
	{EMMC_MFR_SANDISK, 0x004447343030, 0x200,       8,            "Sandisk SDINBDG4-8G 8GB EMMC "},
	{EMMC_MFR_SANDISK, 0x004441343132, 0x200,       128,          "Sandisk SDINBDA4-128G 128GB EMMC "},
	{EMMC_MFR_XINCUN,  0x00654d4d4320, 0x200,       8,            "Xincun XC08MAAJ-NTS 8GB EMMC "},
	{EMMC_MFR_XINCUN,  0x164b4d4d5244, 0x200,       32,           "Xincun XC32MAAJ-NTS 32GB EMMC "},
	{EMMC_MFR_XINCUN,  0x160000930051, 0x200,       64,           "Xincun XC64MAAJ-NTS 64GB EMMC "},

    {0, 0, 0, 0, NULL},
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

COMMAND_HELPER(emmc_command_auto_probe, unsigned name_index, struct emmc_device **emmc)
{
	return emmc_probe(*emmc);
}

static int emmc_cid_parse(struct emmc_device *emmc, uint32_t* cid_buf)
{
	int mrf_id = 0;
	size_t prd_name;
	mrf_id = (cid_buf[3] >> 16) & 0xff;
	prd_name = ((((size_t)cid_buf[3]) & 0xff) << 40)| (((size_t) cid_buf[2]) << 8) | (cid_buf[1] >> 24);

    for (int i = 0; emmc_flash_ids[i].name; i++)
    {
	    if(emmc_flash_ids[i].prd_name == prd_name && (emmc_flash_ids[i].mfr_id == mrf_id))
		{
            emmc->device = &emmc_flash_ids[i];
			emmc->device->block_size = emmc_flash_ids[i].block_size;
	        break;
		}
    }
	
	if(!emmc->device)
	{
		LOG_ERROR("unknown EMMC device, cid: %" PRIx64 " mrf_id: %x", prd_name, mrf_id);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static void emmc_csd_parse(struct emmc_device *emmc, uint32_t* csd_buf)
{
	size_t sec_cnt = 0;
	uint32_t actual_size;

	sec_cnt = csd_buf[SECTOR_COUNT_OFFSET + 4];

	actual_size = (sec_cnt * emmc->device->block_size) >> 30;
	emmc->device->chip_size =  ((actual_size >> 3) + 1) << 3;
}

int emmc_probe(struct emmc_device *emmc)
{
	int status = ERROR_OK;
	// uint32_t in_field[32] = {0};
	uint32_t* in_field;

	in_field = malloc(1024);
	// emmc->device->block_size = EMMC_BLOCK_SIZE;

	status = emmc->controller->init(emmc, in_field);
	if(status != ERROR_OK)
		return ERROR_FAIL;

	status = emmc_cid_parse(emmc, in_field);
	if(status != ERROR_OK)
		return ERROR_FAIL;

	if(emmc->device->chip_size == 0)
		emmc_csd_parse(emmc, in_field + 4);

	if(!emmc->device)
	{
		LOG_ERROR("unknown EMMC flash device found");
		return ERROR_EMMC_OPERATION_FAILED;
	}
    LOG_INFO("found %s", emmc->device->name);
	free(in_field);
    return status;
}

int emmc_read_data_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr)
{
	emmc->controller->read_block_data(emmc, buffer, addr);
    return ERROR_OK;
}

int emmc_write_image(struct emmc_device *emmc, uint8_t *buffer, uint32_t addr, int size)
{
	emmc->controller->write_image(emmc, buffer, addr, size);
	return ERROR_OK;
}

int emmc_verify_image(struct emmc_device *emmc, uint8_t *buffer, uint32_t addr, int size)
{
	int retval;
	retval = emmc->controller->verify_image(emmc, buffer, addr, size);
	return retval;
}
