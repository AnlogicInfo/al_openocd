#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "core.h"
#include "driver.h"
extern struct emmc_flash_controller dwcmshc_emmc_controller;


static struct emmc_flash_controller *emmc_flash_controllers[] = {
	&dwcmshc_emmc_controller,
/*	&boundary_scan_nand_controller, */
	NULL
};

struct emmc_flash_controller *emmc_driver_find_by_name(const char *name)
{
	LOG_INFO("finding emmc driver");
	for (unsigned i = 0; emmc_flash_controllers[i]; i++) {
		struct emmc_flash_controller *controller = emmc_flash_controllers[i];
		if (strcmp(name, controller->name) == 0)
		{
			LOG_INFO("emmc driver found %s", name);
			return controller;
		}
		else{
			LOG_ERROR("Driver not found");
		}
	}
	return NULL;
}

int emmc_driver_walk(emmc_driver_walker_t f, void *x)
{
	for (unsigned i = 0; emmc_flash_controllers[i]; i++) {
		int retval = (*f)(emmc_flash_controllers[i], x);
		if (retval != ERROR_OK)
			return retval;
	}
	return ERROR_OK;
}
