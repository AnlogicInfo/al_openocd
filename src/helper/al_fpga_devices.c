/***************************************************************************
 *   Copyright (C) 2024 Anlogic Inc.                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "al_fpga_devices.h"
#include "log.h"

const char *al_fpga_get_device_name(uint32_t idcode)
{
	for (int i = 0; al_fpga_devices[i].name != NULL; i++) {
		if (al_fpga_devices[i].id == idcode) {
			return al_fpga_devices[i].name;
		}
	}
	return NULL;
}

void al_fpga_print_device_info(uint32_t idcode)
{
	const char *device_name = al_fpga_get_device_name(idcode);
	
	if (device_name) {
		LOG_INFO("AL FPGA Device: %s (ID: 0x%08X)", device_name, idcode);
	}
}
