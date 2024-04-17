/***************************************************************************
 *   Copyright (C) 2018 by Andreas Bolsch                                  *
 *   andreas.bolsch@mni.thm.de                                             *
 *                                                                         *
 *   Copyright (C) 2012 by George Harris                                   *
 *   george@luminairecoffee.com                                            *
 *                                                                         *
 *   Copyright (C) 2010 by Antonio Borneo                                  *
 *   borneo.antonio@gmail.com                                              *
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

#include "imp.h"
#include "spi.h"
// #include <flash/nor/devs/devs.h>
#include <jtag/jtag.h>

extern const flash_ops_t general_spi_ops;
extern const flash_ops_t sp_s25fl_24_ops;
extern const flash_ops_t sp_s25fl_32_ops;
extern const flash_ops_t zetta_zd25q_ops;
extern const flash_ops_t gd_gd25q_ops;
extern const flash_ops_t gd_gd25b512_ops;
extern const flash_ops_t issi_24_ops;
extern const flash_ops_t issi_32_ops;
extern const flash_ops_t win_ops;
extern const flash_ops_t win_32_ops;
extern const flash_ops_t mc_ops;
extern const flash_ops_t zb_ops;
extern const flash_ops_t puya_ops;
extern const flash_ops_t mksv_ops;
extern const flash_ops_t micron_24_ops;
extern const flash_ops_t micron_32_ops;
extern const flash_ops_t mac_25l_ops;
extern const flash_ops_t xtx_25_ops;

 /* Shared table of known SPI flash devices for SPI-based flash drivers. Taken
  * from device datasheets and Linux SPI flash drivers. */
const struct flash_device flash_devices[] = {
/* name, read_cmd, qread_cmd, pprog_cmd, erase_cmd, chip_erase_cmd, device_id,
 * pagesize, sectorsize, size_in_bytes */
FLASH_ID("st m25p05",           0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00102020, 0x80,  0x8000,  0x10000, NULL),
FLASH_ID("st m25p10",           0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00112020, 0x80,  0x8000,  0x20000, NULL),
FLASH_ID("st m25p20",           0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00122020, 0x100, 0x10000, 0x40000, NULL),
FLASH_ID("st m25p40",           0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00132020, 0x100, 0x10000, 0x80000, NULL),
FLASH_ID("st m25p80",           0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00142020, 0x100, 0x10000, 0x100000, NULL),
FLASH_ID("st m25p16",           0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00152020, 0x100, 0x10000, 0x200000, NULL),
FLASH_ID("st m25p32",           0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00162020, 0x100, 0x10000, 0x400000, NULL),
FLASH_ID("st m25p64",           0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00172020, 0x100, 0x10000, 0x800000, NULL),
FLASH_ID("st m25p128",          0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00182020, 0x100, 0x40000, 0x1000000, NULL),
FLASH_ID("st m45pe10",          0x03, 0x00, 0x02, 0xd8, 0xd8, 0x00114020, 0x100, 0x10000, 0x20000, NULL),
FLASH_ID("st m45pe20",          0x03, 0x00, 0x02, 0xd8, 0xd8, 0x00124020, 0x100, 0x10000, 0x40000, NULL),
FLASH_ID("st m45pe40",          0x03, 0x00, 0x02, 0xd8, 0xd8, 0x00134020, 0x100, 0x10000, 0x80000, NULL),
FLASH_ID("st m45pe80",          0x03, 0x00, 0x02, 0xd8, 0xd8, 0x00144020, 0x100, 0x10000, 0x100000, NULL),
FLASH_ID("sp s25fl004",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00120201, 0x100, 0x10000, 0x80000, NULL),
FLASH_ID("sp s25fl008",         0x03, 0x08, 0x02, 0xd8, 0xc7, 0x00130201, 0x100, 0x10000, 0x100000, NULL),
FLASH_ID("sp s25fl016",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00140201, 0x100, 0x10000, 0x200000, NULL),
FLASH_ID("sp s25fl116k",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00154001, 0x100, 0x10000, 0x200000, NULL),
FLASH_ID("sp s25fl032",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00150201, 0x100, 0x10000, 0x400000, NULL),
FLASH_ID("sp s25fl132k",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00164001, 0x100, 0x10000, 0x400000, NULL),
FLASH_ID("sp s25fl064",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00160201, 0x100, 0x10000, 0x800000, NULL),
FLASH_ID("sp s25fl164k",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00174001, 0x100, 0x10000, 0x800000, NULL),
FLASH_ID("sp s25fl128s",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00182001, 0x100, 0x10000, 0x1000000, &sp_s25fl_24_ops),
FLASH_ID("sp s25fl256s",        0x13, 0x00, 0x12, 0xdc, 0xc7, 0x00190201, 0x100, 0x10000, 0x2000000, &sp_s25fl_32_ops),
FLASH_ID("sp s25fl512s",        0x13, 0x00, 0x12, 0xdc, 0xc7, 0x00200201, 0x200, 0x40000, 0x4000000, &sp_s25fl_32_ops),
FLASH_ID("cyp s25fl064l",       0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00176001, 0x100, 0x10000, 0x800000, NULL),
FLASH_ID("cyp s25fl128l",       0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00186001, 0x100, 0x10000, 0x1000000, NULL),
FLASH_ID("cyp s25fl256l",       0x13, 0x00, 0x12, 0xdc, 0xc7, 0x00196001, 0x100, 0x10000, 0x2000000, NULL),
FLASH_ID("atmel 25f512",        0x03, 0x00, 0x02, 0x52, 0xc7, 0x0065001f, 0x80,  0x8000,  0x10000, NULL),
FLASH_ID("atmel 25f1024",       0x03, 0x00, 0x02, 0x52, 0x62, 0x0060001f, 0x100, 0x8000,  0x20000, NULL),
FLASH_ID("atmel 25f2048",       0x03, 0x00, 0x02, 0x52, 0x62, 0x0063001f, 0x100, 0x10000, 0x40000, NULL),
FLASH_ID("atmel 25f4096",       0x03, 0x00, 0x02, 0x52, 0x62, 0x0064001f, 0x100, 0x10000, 0x80000, NULL),
FLASH_ID("atmel 25fs040",       0x03, 0x00, 0x02, 0xd7, 0xc7, 0x0004661f, 0x100, 0x10000, 0x80000, NULL),
FLASH_ID("adesto 25df081a",     0x03, 0x00, 0x02, 0xd8, 0xc7, 0x0001451f, 0x100, 0x10000, 0x100000, NULL),
FLASH_ID("mac 25l512",          0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001020c2, 0x010, 0x10000, 0x10000, NULL),
FLASH_ID("mac 25l1005",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001120c2, 0x010, 0x10000, 0x20000, NULL),
FLASH_ID("mac 25l2005",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001220c2, 0x010, 0x10000, 0x40000, NULL),
FLASH_ID("mac 25l4005",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001320c2, 0x010, 0x10000, 0x80000, NULL),
FLASH_ID("mac 25l8005",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001420c2, 0x010, 0x10000, 0x100000, NULL),
FLASH_ID("mac 25l1605",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001520c2, 0x100, 0x10000, 0x200000, NULL),
FLASH_ID("mac 25l3205",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001620c2, 0x100, 0x10000, 0x400000, NULL),
FLASH_ID("mac 25l6405",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001720c2, 0x100, 0x10000, 0x800000, NULL),
FLASH_ID("mac 25l12845",        0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001820c2, 0x100, 0x10000, 0x1000000, &mac_25l_ops),
FLASH_ID("mac 25l25645",        0x13, 0xec, 0x12, 0xdc, 0xc7, 0x001920c2, 0x100, 0x10000, 0x2000000, NULL),
FLASH_ID("mac 25l51245",        0x13, 0xec, 0x12, 0xdc, 0xc7, 0x001a20c2, 0x100, 0x10000, 0x4000000, NULL),
FLASH_ID("mac 25lm51245",       0x13, 0xec, 0x12, 0xdc, 0xc7, 0x003a85c2, 0x100, 0x10000, 0x4000000, NULL),
FLASH_ID("mac 25r512f",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001028c2, 0x100, 0x10000, 0x10000, NULL),
FLASH_ID("mac 25r1035f",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001128c2, 0x100, 0x10000, 0x20000, NULL),
FLASH_ID("mac 25r2035f",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001228c2, 0x100, 0x10000, 0x40000, NULL),
FLASH_ID("mac 25r4035f",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001328c2, 0x100, 0x10000, 0x80000, NULL),
FLASH_ID("mac 25r8035f",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001428c2, 0x100, 0x10000, 0x100000, NULL),
FLASH_ID("mac 25r1635f",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001528c2, 0x100, 0x10000, 0x200000, NULL),
FLASH_ID("mac 25r3235f",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001628c2, 0x100, 0x10000, 0x400000, NULL),
FLASH_ID("mac 25r6435f",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001728c2, 0x100, 0x10000, 0x800000, NULL),
FLASH_ID("mac 25u1635e",        0x03, 0xeb, 0x02, 0x20, 0xc7, 0x003525c2, 0x100, 0x1000,  0x100000, NULL),
FLASH_ID("micron n25q032",      0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x0016ba20, 0x100, 0x10000, 0x400000, NULL),
FLASH_ID("micron n25q064",      0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x0017ba20, 0x100, 0x10000, 0x800000, NULL),
FLASH_ID("micron n25q128 1.8v", 0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x0018ba20, 0x100, 0x10000, 0x1000000, &micron_24_ops),
FLASH_ID("micron n25q256 3v",   0x13, 0xeb, 0x12, 0xdc, 0xc7, 0x0019ba20, 0x100, 0x10000, 0x2000000, NULL),
FLASH_ID("micron n25q256 1.8v", 0x13, 0xec, 0x12, 0xdc, 0xc7, 0x0019bb20, 0x100, 0x10000, 0x2000000, NULL),
FLASH_ID("micron mt25qu128aba", 0x03, 0x6B, 0x02, 0xd8, 0xc7, 0x0018bb20, 0x100, 0x10000, 0x1000000, &micron_24_ops),
FLASH_ID("micron mt25ql512",    0x13, 0xec, 0x12, 0xdc, 0xc7, 0x0020ba20, 0x100, 0x10000, 0x4000000, NULL),
FLASH_ID("micron mt25qu512",    0x13, 0xec, 0x12, 0xdc, 0xc7, 0x0020bb20, 0x100, 0x10000, 0x4000000, &micron_32_ops),
FLASH_ID("micron mt25ql01",     0x13, 0xec, 0x12, 0xdc, 0xc7, 0x0021ba20, 0x100, 0x10000, 0x8000000, NULL),
FLASH_ID("micron mt25qu01",     0x13, 0xec, 0x12, 0xdc, 0xc7, 0x0021bb20, 0x100, 0x10000, 0x8000000, NULL),
FLASH_ID("micron mt25ql02",     0x13, 0xec, 0x12, 0xdc, 0xc7, 0x0022ba20, 0x100, 0x10000, 0x10000000, NULL),
FLASH_ID("win w25q80bv",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001440ef, 0x100, 0x10000, 0x100000, &win_ops),
FLASH_ID("win w25q16jv",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001540ef, 0x100, 0x10000, 0x200000, &win_ops),
FLASH_ID("win w25q16jv",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001570ef, 0x100, 0x10000, 0x200000, &win_ops),
FLASH_ID("win w25q32fv/jv",     0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001640ef, 0x100, 0x10000, 0x400000, &win_ops),
FLASH_ID("win w25q32fv",        0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001660ef, 0x100, 0x10000, 0x400000, &win_ops),
FLASH_ID("win w25q32jv",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001670ef, 0x100, 0x10000, 0x400000, &win_ops),
FLASH_ID("win w25q6 4fv/jv",     0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001740ef, 0x100, 0x10000, 0x800000, &win_ops),
FLASH_ID("win w25q64fv",        0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001760ef, 0x100, 0x10000, 0x800000, &win_ops),
FLASH_ID("win w25q64jv",        0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001770ef, 0x100, 0x10000, 0x800000, &win_ops),
FLASH_ID("win w25q128fv/jv",    0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001840ef, 0x100, 0x10000, 0x1000000, &win_ops),
FLASH_ID("win w25q128fv",       0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001860ef, 0x100, 0x10000, 0x1000000, &win_ops),
FLASH_ID("win w25q128jv",       0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001870ef, 0x100, 0x10000, 0x1000000, &win_ops),
FLASH_ID("win w25q256fv/jv",    0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001940ef, 0x100, 0x10000, 0x2000000, &win_ops),
FLASH_ID("win w25q256fv",       0x13, 0xec, 0x12, 0xdc, 0xc7, 0x001960ef, 0x100, 0x10000, 0x2000000, &win_32_ops),
FLASH_ID("win w25q256jv",       0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001970ef, 0x100, 0x10000, 0x2000000, &win_ops),
FLASH_ID("win w25q01jv",        0x13, 0x6b, 0x12, 0xdc, 0xc7, 0x002140ef, 0x100, 0x10000, 0x8000000, &win_32_ops),
FLASH_ID("gd gd25q512",         0x03, 0x00, 0x02, 0x20, 0xc7, 0x001040c8, 0x100, 0x1000,  0x10000, &gd_gd25q_ops),
FLASH_ID("gd gd25q10",          0x03, 0x00, 0x02, 0x20, 0xc7, 0x001140c8, 0x100, 0x1000,  0x20000, &gd_gd25q_ops),
FLASH_ID("gd gd25q20",          0x03, 0x00, 0x02, 0x20, 0xc7, 0x001240c8, 0x100, 0x1000,  0x40000, &gd_gd25q_ops),
FLASH_ID("gd gd25q40",          0x03, 0x00, 0x02, 0x20, 0xc7, 0x001340c8, 0x100, 0x1000,  0x80000, &gd_gd25q_ops),
FLASH_ID("gd gd25q16c",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001540c8, 0x100, 0x10000, 0x200000, &gd_gd25q_ops),
FLASH_ID("gd gd25q32c",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001640c8, 0x100, 0x10000, 0x400000, &gd_gd25q_ops),
FLASH_ID("gd gd25q64c",         0x03, 0x00, 0x02, 0xd8, 0xc7, 0x001740c8, 0x100, 0x10000, 0x800000, &gd_gd25q_ops),
FLASH_ID("gd gd25q128c",        0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001840c8, 0x100, 0x10000, 0x1000000, &gd_gd25q_ops),
FLASH_ID("gd gd25lb128d",       0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x001860c8, 0x100, 0x10000, 0x1000000, &gd_gd25q_ops),
FLASH_ID("gd gd25q256c",        0x13, 0x00, 0x12, 0xdc, 0xc7, 0x001940c8, 0x100, 0x10000, 0x2000000, &gd_gd25q_ops),
FLASH_ID("gd gd25q256d",        0x13, 0x00, 0x12, 0xdc, 0xc7, 0x001967c8, 0x100, 0x10000, 0x2000000, NULL),
FLASH_ID("gd gd25b512me",       0x13, 0x6c, 0x12, 0xdc, 0xc7, 0x001a47c8, 0x100, 0x10000, 0x4000000, &gd_gd25b512_ops),
FLASH_ID("gd gd25q512mc",       0x13, 0x00, 0x12, 0xdc, 0xc7, 0x002040c8, 0x100, 0x10000, 0x4000000, &gd_gd25b512_ops),
FLASH_ID("issi is25lq040",      0x03, 0x6b, 0x02, 0xd7, 0xc7, 0x0013409d, 0x100, 0x1000,  0x80000,  &issi_24_ops),
FLASH_ID("issi is25lp032",      0x03, 0x00, 0x02, 0xd8, 0xc7, 0x0016609d, 0x100, 0x10000, 0x400000, &issi_24_ops),
FLASH_ID("issi is25lp064",      0x03, 0x00, 0x02, 0xd8, 0xc7, 0x0017609d, 0x100, 0x10000, 0x800000, &issi_24_ops),
FLASH_ID("issi is25lp128d",     0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x0018609d, 0x100, 0x10000, 0x1000000, &issi_24_ops),
FLASH_ID("issi is25wp128d",     0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x0018709d, 0x100, 0x10000, 0x1000000, &issi_24_ops),
FLASH_ID("issi is25lp256d",     0x13, 0xec, 0x12, 0xdc, 0xc7, 0x0019609d, 0x100, 0x10000, 0x2000000, &issi_32_ops),
FLASH_ID("issi is25wp256d",     0x13, 0xec, 0x12, 0xdc, 0xc7, 0x0019709d, 0x100, 0x10000, 0x2000000, &issi_32_ops),
FLASH_ID("issi is25lp512m",     0x13, 0xec, 0x12, 0xdc, 0xc7, 0x001a609d, 0x100, 0x10000, 0x4000000, &issi_32_ops),
FLASH_ID("issi is25wp512m",     0x13, 0xec, 0x12, 0xdc, 0xc7, 0x001a709d, 0x100, 0x10000, 0x4000000, &issi_32_ops),
FLASH_ID("mc sst26vf032",       0x03, 0x00, 0x02, 0x20, 0xc7, 0x004226bf, 0x100, 0x1000,  0x400000,  &mc_ops),
FLASH_ID("mksv mksv064a",       0x03, 0x6b, 0x02, 0x20, 0xc7, 0x0017408f, 0x100, 0x1000,  0x800000, &mksv_ops),
FLASH_ID("puya p25q32sh",       0x03, 0x6b, 0x02, 0x20, 0xc7, 0x00166085, 0x100, 0x1000,  0x400000, &puya_ops),
FLASH_ID("puya p25q128sh",      0x03, 0x6b, 0x02, 0x20, 0xc7, 0x00186585, 0x100, 0x1000,  0x1000000, NULL),
FLASH_ID("zd zd25q16",          0x03, 0x6b, 0x02, 0xd8, 0xc7, 0x001560ba, 0x100, 0x10000, 0x200000,  &zetta_zd25q_ops),
FLASH_ID("zb zb25vq64",         0x03, 0x6b, 0x02, 0x20, 0xc7, 0x0017405e, 0x100, 0x1000,  0x800000, &zb_ops),
FLASH_ID("xtx xt25f128b",       0x03, 0x6b, 0x02, 0x20, 0xc7, 0x0018400b, 0x100, 0x1000,  0x1000000, &xtx_25_ops),

/* FRAM, no erase commands, no write page or sectors */
FRAM_ID("fu mb85rs16n",         0x03, 0,    0x02, 0x00010104, 0x800),
FRAM_ID("fu mb85rs32v",         0x03, 0,    0x02, 0x00010204, 0x1000), /* exists ? */
FRAM_ID("fu mb85rs64v",         0x03, 0,    0x02, 0x00020304, 0x2000),
FRAM_ID("fu mb85rs128b",        0x03, 0,    0x02, 0x00090404, 0x4000),
FRAM_ID("fu mb85rs256b",        0x03, 0,    0x02, 0x00090504, 0x8000),
FRAM_ID("fu mb85rs512t",        0x03, 0,    0x02, 0x00032604, 0x10000),
FRAM_ID("fu mb85rs1mt",         0x03, 0,    0x02, 0x00032704, 0x20000),
FRAM_ID("fu mb85rs2mta",        0x03, 0,    0x02, 0x00034804, 0x40000),
FRAM_ID("cyp fm25v01a",         0x03, 0,    0x02, 0x000821c2, 0x4000),
FRAM_ID("cyp fm25v02",          0x03, 0,    0x02, 0x000022c2, 0x8000),
FRAM_ID("cyp fm25v02a",         0x03, 0,    0x02, 0x000822c2, 0x8000),
FRAM_ID("cyp fm25v05",          0x03, 0,    0x02, 0x000023c2, 0x10000),
FRAM_ID("cyp fm25v10",          0x03, 0,    0x02, 0x000024c2, 0x20000),
FRAM_ID("cyp fm25v20a",         0x03, 0,    0x02, 0x000825c2, 0x40000),
FRAM_ID("cyp fm25v40",          0x03, 0,    0x02, 0x004026c2, 0x80000),

FLASH_ID(NULL,                  0,    0,    0,    0,    0,    0,          0,     0,       0, NULL)
};

