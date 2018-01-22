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


#include "docopt.h"


#ifdef _WIN32
wchar_t wc_binarize[2048];
int found_bis_binarize;
int check_bis_binarize();

int get_p3d_dependencies(char *source, char *tempfolder_root, bool bulk_binarize);

int attempt_bis_binarize_wrp(char *source, char *target);
int attempt_bis_texheader(char *target);
int attempt_bis_bulk_binarize(char *source);
#endif

int binarize(char *source, char *target, bool force_p3d);
int cmd_binarize();
