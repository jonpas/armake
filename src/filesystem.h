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

#ifdef _WIN32
#define PATHSEP '\\'
#define PATHSEP_STR "\\"
#define TEMPPATH "C:\\Windows\\Temp\\armake\\"
#else
#define PATHSEP '/'
#define PATHSEP_STR "/"
#define TEMPPATH "/tmp/armake/"
#endif


#define strlens(s) (s==NULL?0:strlen(s))
#ifdef _WIN32
#define wcslens(s) (s==NULL?0:wcslen(s))
#endif


#ifdef _WIN32
bool file_exists(char *path);
bool file_exists_fuzzy(char *path);
bool wc_file_exists(wchar_t *wc_path);
size_t getline(char **lineptr, size_t *n, FILE *stream);

int copy_bulk_p3ds_dependencies(char *source);
#endif

int get_temp_name(char *target, char *suffix);

int create_folder(char *path);

int create_folders(char *path);

int create_temp_folder(char *addon, char *temp_folder, size_t bufsize);

void get_temp_path(char *path, size_t bufsize);

int rename_file(char *oldname, char *newname);

int remove_file(char *path);

int remove_folder(char *folder);

int copy_file(char *source, char *target);

int traverse_directory(char *root, bool avoid_other_pboprefixes, int (*callback)(char *, char *, char *),
    char *third_arg);

int copy_directory(char *source, char *target);

int copy_directory_keep_prefix_path(char *source);

int copy_includes(char *source, char *target);
