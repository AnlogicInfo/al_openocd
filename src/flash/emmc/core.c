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
		while (p && p->next) {
			p = p->next;
		}
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
  {EMMC_MFR_SAMSUNG, 0x414a54443452, 0x200,       16,           "Samsung KLMAG1JETD-B041 16GB EMMC "},
  {EMMC_MFR_SAMSUNG, 0x4d4347384743, 0x200,       64,           "Samsung KLMCG8GEAC-B031 64GB EMMC "},
	{EMMC_MFR_MICRON,  0x53304a353658, 0x200,       16,           "Micron MTFC16GAPALBH-IT 16GB EMMC "},
	{EMMC_MFR_MICRON,  0x52314a35364c, 0x200,       16,           "Micron MTFC16GAKAEJP-4MIT 16GB EMMC "},
	{EMMC_MFR_MICRON,  0x51324a35354c, 0x200,       8,            "Micron MTFC8GAKAJCN-4MIT 8GB EMMC "},
	{EMMC_MFR_MICRON,  0x53304a333541, 0x200,       8,            "Micron MTFC8GAMALBH-AIT 8GB EMMC "},
	{EMMC_MFR_FORESEE, 0x383841333938, 0x200,       8,            "Foresee FEMDRW008g-88A39 8GB EMMC "},
	{EMMC_MFR_FORESEE, 0x353841343631, 0x200,       16,           "Foresee FEMDNN016G-58A46 16GB EMMC "},
	{EMMC_MFR_FORESEE, 0x413341353632, 0x200,       128,          "Foresee FEMDNN128G-A3A56 128GB EMMC "},
	{EMMC_MFR_HYNIX,   0x483847346132, 0x200,       8,            "Hynix H26M41208HPR 8GB EMMC "},
	{EMMC_MFR_SANDISK, 0x444734303038, 0x200,       8,            "Sandisk SDINBDG4-8G 8GB EMMC "},
	{EMMC_MFR_SANDISK, 0x444134313238, 0x200,       128,          "Sandisk SDINBDA4-128G 128GB EMMC "},
	{EMMC_MFR_XINCUN0, 0x654d4d432020, 0x200,       8,            "Xincun XC08MAAJ-NTS 8GB EMMC "},
	{EMMC_MFR_XINCUN1, 0x4b4d4d524441, 0x200,       32,           "Xincun XC32MAAJ-NTS 32GB EMMC "},
	{EMMC_MFR_XINCUN1, 0x000093005100, 0x200,       64,           "Xincun XC64MAAJ-NTS 64GB EMMC "},

    {0, 0, 0, 0, NULL},
	{0, 0, 0x200, 0, "Compatible Mode"},
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
	int i = 0;
	int mrf_id = 0;
	size_t pnm;
	mrf_id = (cid_buf[3] >> 16) & 0xff;
	pnm = ((size_t) cid_buf[2] << 16) | (cid_buf[1] >> 16);

	for (i = 0; emmc_flash_ids[i].name; i++) {
		if (emmc_flash_ids[i].pnm == pnm && (emmc_flash_ids[i].mfr_id == mrf_id)) {
			emmc->device = &emmc_flash_ids[i];
			emmc->device->block_size = emmc_flash_ids[i].block_size;
			break;
		}
	}
	
	if (!emmc->device) {
		LOG_ERROR("unknown EMMC device, pid: %" PRIx64 " mrf_id: %x", pnm, mrf_id);
		emmc->device = &emmc_flash_ids[i + 1];
		for (i = 0; i < 4; i++)
			LOG_ERROR("cid %d %"PRIx32 , i, cid_buf[i]);
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


int emmc_write_data_block(struct emmc_device *emmc, uint32_t *buffer, uint32_t addr)
{
	emmc->controller->write_block_data(emmc, buffer, addr);
    return ERROR_OK;
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
