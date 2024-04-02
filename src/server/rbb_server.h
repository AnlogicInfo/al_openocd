/*
 * Copyright (C) 2024 by Ruigang Wan <ruigang.wan@anlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENOCD_SERVER_RBB_SERVER_H
#define OPENOCD_SERVER_RBB_SERVER_H

#include <helper/command.h>

int rbb_server_register_commands(struct command_context *ctx);

extern int allow_tap_access;

#define RBB_BUFFERSIZE 1024
#define RBB_MAX_BUF_COUNT 16

#endif /* OPENOCD_SERVER_RBB_SERVER_H */
