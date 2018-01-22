/*
* Copyright (C)  2015  SnakeManPMC
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

// Code originally from https://github.com/SnakeManPMC/arma-2-WRP_Stats



#pragma once

#include "build.h"

struct filelist *wrp_p3d_fileslist;
struct filelist *wrp_rvmat_fileslist;

int wrp_parse(char *path);

void wrp_get_filelist(struct filelist **list, char *tempfolder_root);
