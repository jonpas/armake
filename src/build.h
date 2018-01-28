/*
 * Copyright (C)  2016  Felix "KoffeinFlummi" Wiegand
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <stdbool.h>


struct filelist {
    char filename[2048];
    struct filelist *next;
};

struct build_ignore_data {
    char tempfolder_root[2048];
    struct filelist *fileslist;
};

#ifdef _WIN32
void build_add_ignore(char *filename, struct build_ignore_data *data);

void build_revert_ignores(struct build_ignore_data *data);
#endif

int build(char *prefixpath, char *tempfolder, char *addonprefix, char *tempfolder_root, char *target);

int cmd_build(bool recursive);
