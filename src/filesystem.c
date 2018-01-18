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


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <dirent.h>
#include <unistd.h>
#endif

#include "filesystem.h"
#include "docopt.h"
#include "preprocess.h"
#include "utils.h"
#include "unistdwrapper.h"


#ifdef _WIN32
bool file_exists(char *path) {
    wchar_t *wc_path = malloc(sizeof(*wc_path) * (strlen(path) + 1));
    mbstowcs(wc_path, path, (strlen(path) + 1));
    unsigned long attrs = GetFileAttributes(wc_path);
    free (wc_path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

bool wc_file_exists(wchar_t *wc_path) {
    unsigned long attrs = GetFileAttributes(wc_path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

size_t getline(char **lineptr, size_t *n, FILE *stream) {
    char *bufptr = NULL;
    char *p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL)
        return -1;
    if (stream == NULL)
        return -1;
    if (n == NULL)
        return -1;

    bufptr = *lineptr;
    size = *n;

    c = fgetc(stream);
    if (c == EOF)
        return -1;

    if (bufptr == NULL) {
        bufptr = malloc(128);
        if (bufptr == NULL)
            return -1;
        size = 128;
    }
    p = bufptr;
    while(c != EOF) {
        if ((p - bufptr) > (size - 1)) {
            size = size + 128;
            bufptr = realloc(bufptr, size);
            p = bufptr + size - 128;
            if (bufptr == NULL)
                return -1;
        }

        *p++ = c;

        if (c == '\n')
            break;

        c = fgetc(stream);
    }

    *p++ = '\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}
#endif


void get_temp_path(char *path, size_t bufsize) {
    if (strlens(args.temppath) == 0) {
#ifdef _WIN32
        path[0] = 0;
        wchar_t wc_path[2048];
        wchar_t wc_path_tmp[2048];
        if (GetTempPath(bufsize, wc_path_tmp) > 0) {
            if ((wc_path_tmp[wcslens(wc_path_tmp) - 1]) == '\\') {
                wc_path_tmp[wcslens(wc_path_tmp) - 1] = 0;
            }
            swprintf(wc_path, 2048, L"%ls\\armake\\%ld\\", wc_path_tmp, GetCurrentProcessId());
            wcstombs(path, wc_path, bufsize);
        } else {
            path = TEMPPATH;
        }
        // TODO Add command switch to remove old directories, windows apparently doesn't remove temp files at startup.
#else
        strncpy(path, LINUX_TEMPPATH, bufsize);
        sprintf(path, "%s/armake/%ld/", path, getpid());
#endif
    } else {
        strcpy(path, args.temppath);
    }
    if (((path[strlen(path) - 1]) == '\\') || ((path[strlen(path) - 1]) == '/')) {
        path[strlen(path) - 1] = 0;
    }
}


int get_temp_name(char *target, char *suffix) {
#ifdef _WIN32
    wchar_t *wc_target = malloc(sizeof(*target) * (strlen(target) + 1));
    mbstowcs(wc_target, target, 2048);
    if (!GetTempFileName(L".", L"amk", 0, wc_target)) { return 1; }
    wcstombs(target, wc_target, (strlen(target) + 1));
    free(wc_target);
    strcat(target, suffix);
    return 0;
#else
    strcpy(target, "amk_XXXXXX");
    strcat(target, suffix);
    return mkstemps(target, strlen(suffix)) == -1;
#endif
}


int create_folder(char *path) {
    /*
     * Create the given folder. Returns -2 if the directory already exists,
     * -1 on error and 0 on success.
     */

#ifdef _WIN32

    wchar_t wc_path[2048];
    mbstowcs(wc_path, path, 2048);
    if (!CreateDirectory(wc_path, NULL)) {
        if (GetLastError() == ERROR_ALREADY_EXISTS)
            return -2;
        else
            return -1;
    }

    return 0;

#else

    struct stat st = {0};

    if (stat(path, &st) != -1)
        return -2;

    return mkdir(path, 0755);

#endif
}


int create_folders(char *path) {
    /*
     * Recursively create all folders for the given path. Returns -1 on
     * failure and 0 on success.
     */

    char tmp[2048];
    char *p = NULL;
    int success;
    size_t len;

    tmp[0] = 0;
    strcat(tmp, path);
    len = strlen(tmp);

    if (tmp[len - 1] == PATHSEP)
        tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == PATHSEP && *(p-1) != ':') {
            *p = 0;
            success = create_folder(tmp);
            if (success != -2 && success != 0)
                return success;
            *p = PATHSEP;
        }
    }

    success = create_folder(tmp);
    if (success != -2 && success != 0)
        return success;

    return 0;
}


int create_temp_folder(char *addon, char *temp_folder, size_t bufsize) {
    /*
     * Create a temp folder for the given addon in the proper place
     * depending on the operating system. Returns -1 on failure and 0
     * on success.
     */

    char temp[2048];
    char addon_sanitized[2048];
    int i;

    get_temp_path(temp, 2048);

    for (i = 0; i <= strlen(addon); i++)
        addon_sanitized[i] = (addon[i] == '\\' || addon[i] == '/') ? '_' : addon[i];

    snprintf(temp_folder, bufsize, "%s%c%s%c", temp, PATHSEP, addon_sanitized, PATHSEP);


    if (i == 1024)
        return -1;

    return create_folders(temp_folder);
}


int remove_file(char *path) {
    /*
     * Remove a file. Returns 0 on success and 1 on failure.
     */

#ifdef _WIN32
    wchar_t wc_path[2048];
    mbstowcs(wc_path, path, 2048);
    return !DeleteFile(wc_path);
#else
    return (remove(path) * -1);
#endif
}


int remove_folder(char *folder) {
    /*
     * Recursively removes a folder tree. Returns a negative integer on
     * failure and 0 on success.
     */

#ifdef _WIN32

    // MASSIVE @todo
    char cmd[512];
    //sprintf(cmd, "rmdir %s /s /q", folder);
    sprintf(cmd, "rmdir %s /s", folder);
    if (system(cmd))
        return -1;

#else

    // ALSO MASSIVE @todo
    char cmd[512];
    sprintf(cmd, "rm -rf %s", folder);
    if (system(cmd))
        return -1;

#endif

    return 0;
}


int copy_file(char *source, char *target) {
    /*
     * Copy the file from the source to the target. Overwrites if the target
     * already exists.
     * Returns a negative integer on failure and 0 on success.
     */

    // Create the containing folder
    if (!strcmp(source, target))
        return 0;

    char *containing = malloc(sizeof(*containing) * (strlen(target) + 1));
    int lastsep = 0;
    int i;
    for (i = 0; i < strlen(target); i++) {
        if (target[i] == PATHSEP)
            lastsep = i;
    }
    strcpy(containing, target);
    containing[lastsep] = 0;

    if (create_folders(containing))
    {
        free(containing);
        return -1;
    }
    free(containing);

#ifdef _WIN32

    wchar_t wc_source[2048];
    mbstowcs(wc_source, source, 2048);
    wchar_t wc_target[2048];
    mbstowcs(wc_target, target, 2048);
    if (!CopyFile(wc_source, wc_target, 0))
        return -2;

#else

    int f_target, f_source;
    char buf[4096];
    ssize_t nread;

    f_source = open(source, O_RDONLY);
    if (f_source < 0)
        return -2;

    f_target = open(target, O_WRONLY | O_CREAT, 0666);
    if (f_target < 0) {
        close(f_source);
        if (f_target >= 0)
            close(f_target);
        return -3;
    }

    while (nread = read(f_source, buf, sizeof buf), nread > 0) {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(f_target, out_ptr, nread);

            if (nwritten >= 0) {
                nread -= nwritten;
                out_ptr += nwritten;
            } else if (errno != EINTR) {
                close(f_source);
                if (f_target >= 0)
                    close(f_target);
                return -4;
            }
        } while (nread > 0);
    }

    close(f_source);
    if (close(f_target) < 0)
        return -5;

#endif

    return 0;
}


#ifndef _WIN32
int alphasort_ci(const struct dirent **a, const struct dirent **b) {
    /*
     * A case insensitive version of alphasort.
     */

    int i;
    char a_name[512];
    char b_name[512];

    strncpy(a_name, (*a)->d_name, sizeof(a_name));
    strncpy(b_name, (*b)->d_name, sizeof(b_name));

    for (i = 0; i < strlen(a_name); i++) {
        if (a_name[i] >= 'A' && a_name[i] <= 'Z')
            a_name[i] = a_name[i] - ('A' - 'a');
    }

    for (i = 0; i < strlen(b_name); i++) {
        if (b_name[i] >= 'A' && b_name[i] <= 'Z')
            b_name[i] = b_name[i] - ('A' - 'a');
    }

    return strcoll(a_name, b_name);
}
#endif


int traverse_directory_recursive(char *root, char *cwd, int (*callback)(char *, char *, char *), char *third_arg) {
    /*
     * Recursive helper function for directory traversal.
     */

#ifdef _WIN32

    WIN32_FIND_DATA file;
    HANDLE handle = NULL;
    char mask[2048];
    wchar_t wc_mask[2048];
    int success;

    if (cwd[strlen(cwd) - 1] == '\\')
        cwd[strlen(cwd) - 1] = 0;

    wchar_t wc_cwd[2048];
    mbstowcs(wc_cwd, cwd, 2048);
    GetFullPathName(wc_cwd, 2048, wc_mask, NULL);
    wcstombs(cwd, wc_cwd, 2048);
    swprintf(wc_mask, 2048, L"%s\\*", wc_mask);

    handle = FindFirstFile(wc_mask, &file);
    if (handle == INVALID_HANDLE_VALUE)
        return 1;

    do {
        if (wcscmp(file.cFileName, L".") == 0 || wcscmp(file.cFileName, L"..") == 0)
            continue;

        swprintf(wc_mask, 2048, L"%s\\%s", wc_cwd, file.cFileName);
        wcstombs(mask, wc_mask, 2048);
        if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            traverse_directory_recursive(root, mask, callback, third_arg);
        } else {
            success = callback(root, mask, third_arg);
            if (success)
                return success;
        }
    } while (FindNextFile(handle, &file));

    FindClose(handle);

    return 0;

#else

    struct dirent **namelist;
    struct stat st;
    char next[2048];
    int i;
    int n;
    int success;

    n = scandir(cwd, &namelist, NULL, alphasort_ci);
    if (n < 0)
        return 1;

    success = 0;
    for (i = 0; i < n; i++) {
        if (strcmp(namelist[i]->d_name, "..") == 0 ||
                strcmp(namelist[i]->d_name, ".") == 0)
            continue;

        strcpy(next, cwd);
        strcat(next, "/");
        strcat(next, namelist[i]->d_name);

        stat(next, &st);

        if (S_ISDIR(st.st_mode))
            success = traverse_directory_recursive(root, next, callback, third_arg);
        else
            success = callback(root, next, third_arg);

        if (success)
            goto cleanup;
    }

cleanup:
    for (i = 0; i < n; i++)
        free(namelist[i]);
    free(namelist);

    return success;

#endif
}


int traverse_directory(char *root, int (*callback)(char *, char *, char *), char *third_arg) {
    /*
     * Traverse the given path and call the callback with the root folder as
     * the first, the current file path as the second, and the given third
     * arg as the third argument.
     *
     * The callback should return 0 success and any negative integer on
     * failure.
     *
     * This function returns 0 on success, a positive integer on a traversal
     * error and the last callback return value should the callback fail.
     */

    return traverse_directory_recursive(root, root, callback, third_arg);
}


int copy_callback(char *source_root, char *source, char *target_root) {
    // Remove trailing path seperators
    if (source_root[strlen(source_root) - 1] == PATHSEP)
        source_root[strlen(source_root) - 1] = 0;
    if (target_root[strlen(target_root) - 1] == PATHSEP)
        target_root[strlen(target_root) - 1] = 0;

    if (strstr(source, source_root) != source)
        return -1;

    char *target = malloc(sizeof(*target)* (strlen(source) + strlen(target_root) + 1 + 1)); // assume worst case
    target[0] = 0;
    strcat(target, target_root);
    strcat(target, source + strlen(source_root));

    int status = copy_file(source, target);
    free(target);
    return status;
}


int copy_includes_callback(char *source_root, char *source, char *target_root) {
    // Remove trailing path seperators
    if (source_root[strlen(source_root) - 1] == PATHSEP)
        source_root[strlen(source_root) - 1] = 0;
    if (target_root[strlen(target_root) - 1] == PATHSEP)
        target_root[strlen(target_root) - 1] = 0;

    if (strstr(source, source_root) != source)
        return -1;
	
    int status = -1;
	char fileext[64];
	if (strrchr(source, '.') != NULL) {
		strcpy(fileext, strrchr(source, '.'));
		if (fileext != NULL && (!strcmp(fileext, ".h")) || (!strcmp(fileext, ".hpp"))) {
			char prefixedpath[2048];
			get_prefixpath(source, source_root, prefixedpath, sizeof(prefixedpath));

			char *target = malloc(sizeof(*target)* (strlen(prefixedpath) + strlen(target_root) + 1 + 1)); // assume worst case
			target[0] = 0;
			strcat(target, target_root);
			strcat(target, PATHSEP_STR);
			strcat(target, prefixedpath);
			infof("DEBUG: %s : %s\n", source, target);
			status = copy_file(source, target);
			free(target);
		}
	}


    return 0;
}


int copy_directory(char *source, char *target) {
    /*
     * Copy the entire directory given with source to the target folder.
     * Returns 0 on success and a non-zero integer on failure.
     */

    // Remove trailing path seperators
    if (source[strlen(source) - 1] == PATHSEP)
        source[strlen(source) - 1] = 0;
    if (target[strlen(target) - 1] == PATHSEP)
        target[strlen(target) - 1] = 0;

    return traverse_directory(source, copy_callback, target);
}


int copy_includes(char *source, char *target) {
    /*
     * Copy the entire directory given with source to the target folder.
     * Returns 0 on success and a non-zero integer on failure.
     */

    // Remove trailing path seperators
    if (source[strlen(source) - 1] == PATHSEP)
        source[strlen(source) - 1] = 0;
    if (target[strlen(target) - 1] == PATHSEP)
        target[strlen(target) - 1] = 0;

    return traverse_directory(source, copy_includes_callback, target);
}
