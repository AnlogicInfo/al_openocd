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

#ifndef OPENOCD_HELPER_AL_FPGA_DEVICES_H
#define OPENOCD_HELPER_AL_FPGA_DEVICES_H

#include <stdint.h>
#include <stddef.h>

/**
 * Structure to hold AL FPGA device information
 */
struct al_fpga_device_info {
	uint32_t id;        /* Device IDCODE */
	const char *name;   /* Device name */
};

/**
 * AL FPGA Device ID mapping table
 * Maps IDCODE values to device names for AL FPGA devices
 */
static const struct al_fpga_device_info al_fpga_devices[] = {
	{ 0x621570E3, "DR1V90GEG484/DR1V90GEG400" },
	{ 0x6A1570E3, "DR1V90MEG484/DR1V90MEG400" },
	{ 0x641570E3, "DR1M90GEG484/DR1M90GEG400/DR1M90GEG484A" },
	{ 0x6C1570E3, "DR1M90MEG484/DR1M90MEG400/DR1M90MEG484A" },
	{ 0x721570E3, "DR1V90GEG484ID7" },
	{ 0x7A1570E3, "DR1V90MEG484ID7" },
	{ 0x741570E3, "DR1M90GEG484ID7" },
	{ 0x7C1570E3, "DR1M90MEG484ID7" },
	{ 0, NULL }  /* End marker */
};

/**
 * Get the device name for a given AL FPGA IDCODE
 * @param idcode The 32-bit IDCODE value
 * @return A pointer to the device name string, or NULL if not found
 */
const char *al_fpga_get_device_name(uint32_t idcode);

/**
 * Print AL FPGA device information for a given IDCODE
 * @param idcode The 32-bit IDCODE value
 */
void al_fpga_print_device_info(uint32_t idcode);

#endif /* OPENOCD_HELPER_AL_FPGA_DEVICES_H */
