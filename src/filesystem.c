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

#include "binarize.h"
#include "build.h"
#include "filesystem.h"
#include "docopt.h"
#include "p3d.h"
#include "preprocess.h"
#include "utils.h"
#include "unistdwrapper.h"



bool file_exists(char *path) {
#ifdef _WIN32
    size_t len = (strlens(path) + 1);
    wchar_t *wc_path = malloc(sizeof(*wc_path) * len);
    mbstowcs(wc_path, path, len);
    unsigned long attrs = GetFileAttributes(wc_path);
    free(wc_path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    return (access(path, F_OK) != -1);
#endif
}

bool file_exists_fuzzy(char *path) {
#ifdef _WIN32
    size_t len = (strlens(path) + 1);
    wchar_t *wc_path = malloc(sizeof(*wc_path) * len);
    mbstowcs(wc_path, path, len);
    unsigned long attrs = GetFileAttributes(wc_path);

    int status = (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
    if (!status) {
        if ((!stricmp(strrchr(path, '.'), ".tga")) || (!stricmp(strrchr(path, '.'), ".png"))) {
            char *alternative_path = malloc(sizeof(*alternative_path) * len);
            strncpy(alternative_path, path, len);
            strcpy(strchr(alternative_path, '.'), ".paa");
            mbstowcs(wc_path, alternative_path, len);
            free(alternative_path);
            attrs = GetFileAttributes(wc_path);
            status = (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
        }
    }

    free(wc_path);
    return status;
#else
    return (access(path, F_OK) != -1);
#endif
}

#ifdef _WIN32
bool wc_file_exists(wchar_t *wc_path) {
    unsigned long attrs = GetFileAttributes(wc_path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

// https://stackoverflow.com/questions/735126/are-there-alternate-implementations-of-gnu-getline-interface/47229318#47229318
ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    size_t pos;
    int c;

    if (lineptr == NULL || stream == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }

    c = fgetc(stream);
    if (c == EOF) {
        return -1;
    }

    if (*lineptr == NULL) {
        *lineptr = malloc(128);
        if (*lineptr == NULL) {
            return -1;
        }
        *n = 128;
    }

    pos = 0;
    while (c != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n + (*n >> 2);
            if (new_size < 128) {
                new_size = 128;
            }
            char *new_ptr = realloc(*lineptr, new_size);
            if (new_ptr == NULL) {
                return -1;
            }
            *n = new_size;
            *lineptr = new_ptr;
        }

        (*lineptr)[pos++] = c;
        if (c == '\n') {
            break;
        }
        c = fgetc(stream);
    }

    (*lineptr)[pos] = '\0';
    return pos - 1;
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
        strncpy(path, TEMPPATH, bufsize);
        sprintf(path, "%s/armake/%ld/", path, getpid());
#endif
    } else {
        strcpy(path, args.temppath);
    }
    if ((path[strlen(path) - 1]) == PATHSEP) {
        path[strlen(path) - 1] = 0;
    }
}


int get_temp_name(char *target, char *suffix) {
#ifdef _WIN32
    wchar_t *wc_target = malloc(sizeof(*wc_target) * (strlen(target) + 1));
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
        addon_sanitized[i] = (addon[i] == '\\' || addon[i] == '/') ? PATHSEP : addon[i];

    snprintf(temp_folder, bufsize, "%s%c%s%c", temp, PATHSEP, addon_sanitized, PATHSEP);


    if (i == 1024)
        return -1;

    return create_folders(temp_folder);
}


int rename_file(char *oldname, char *newname) {
    /*
     * Rename a file. Returns 0 on success and non zero on failure.
     */

    #ifdef _WIN32
        wchar_t wc_oldname[2048];
        mbstowcs(wc_oldname, oldname, 2048);
        wchar_t wc_newname[2048];
        mbstowcs(wc_newname, newname, 2048);
        return (_wrename(wc_oldname, wc_newname));
    #else
        return (rename(oldname,newname));
    #endif
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

     // Just extra safety that folder path is inside root temp path.
    char temp_rootfolder[2048];
    get_temp_path(temp_rootfolder, sizeof(temp_rootfolder));
    if (strncmp(folder, temp_rootfolder, strlens(temp_rootfolder)) != 0) {
        errorf("Remove folder error path: %s", folder);
        return -1;
    }
        

#ifdef _WIN32
    // MASSIVE @todo
    char cmd[512];
    sprintf(cmd, "rmdir %s /s /q", folder);
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

    bool copy_file = true;
    
    if (file_exists(target)) {
        FILETIME ft_source;
        FILETIME ft_target;
        HANDLE  f_file_source = CreateFile(wc_source, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        HANDLE  f_file_target = CreateFile(wc_target, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if ((GetFileTime(f_file_source, NULL, NULL, &ft_source)) && (GetFileTime(f_file_target, NULL, NULL, &ft_target))) {
            ULARGE_INTEGER ul_source;
            ul_source.LowPart = ft_source.dwLowDateTime;
            ul_source.HighPart = ft_source.dwHighDateTime;
            __int64 fileTime64_source = ul_source.QuadPart;

            ULARGE_INTEGER ul_target;
            ul_target.LowPart = ft_target.dwLowDateTime;
            ul_target.HighPart = ft_target.dwHighDateTime;
            __int64 fileTime64_target = ul_target.QuadPart;

            if (args.force) {
                if (fileTime64_source == fileTime64_target) {
                    //infof("Skipping file %s\n.", source);
                    copy_file = false;
                }
            } else {
                if (fileTime64_source < fileTime64_target) {
                    //infof("Skipping file %s\n.", source);
                    copy_file = false;
                }
            }
        }
        CloseHandle(f_file_source);
        CloseHandle(f_file_target);
    }

    if (copy_file)
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



int traverse_directory_recursive(char *root, char *cwd, bool *avoid_other_pboprefixes, int (*callback)(char *, char *, char *), char *third_arg) {
    /*
     * Recursive helper function for directory traversal.
     */

#ifdef _WIN32

    WIN32_FIND_DATA file;
    HANDLE handle = NULL;
    char mask[2048];
    wchar_t wc_mask[2048];
    char mask_pboprefix[2048];
    int success = 0;

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
            if (*avoid_other_pboprefixes) {
                strcpy(mask_pboprefix, mask);
                strcat(mask_pboprefix, "$PBOPREFIX$");
                if (!file_exists(mask_pboprefix))
                    success = traverse_directory_recursive(root, mask, avoid_other_pboprefixes, callback, third_arg);
            } else {
                success = traverse_directory_recursive(root, mask, avoid_other_pboprefixes, callback, third_arg);
            }
        } else {
            success = callback(root, mask, third_arg);
        }
        if (success != 0)
            return success;
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
            success = traverse_directory_recursive(root, next, avoid_other_pboprefixes, callback, third_arg);
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


int traverse_directory(char *root, bool avoid_other_pboprefixes, int (*callback)(char *, char *, char *), char *third_arg) {
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

    return traverse_directory_recursive(root, root, &avoid_other_pboprefixes, callback, third_arg);
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
    char filename[1024];

    strcpy(filename, strrchr(source, PATHSEP) + 1);

    if (strrchr(source, '.') != NULL) {
        strcpy(fileext, strrchr(source, '.'));
        if (fileext != NULL && ((!stricmp(fileext, ".h")) || (!stricmp(fileext, ".hpp")) || (!stricmp(fileext, ".cpp"))))
            status = 0;
        else if (!stricmp(filename, "$PBOPREFIX$.txt"))
            status = 0;
    } else {
        if (!stricmp(filename, "$PBOPREFIX$"))
            status = 0;
    }
    if (status == 0) {
        //$PBOPREFIX$ is used to signify new PBO start, when packing directories recursively
        char prefixedpath[2048];
        get_prefixpath(source, source_root, prefixedpath, sizeof(prefixedpath));

        char *target = malloc(sizeof(*target)* (strlen(prefixedpath) + strlen(target_root) + 1 + 1)); // assume worst case
        target[0] = 0;
        strcat(target, target_root);
        strcat(target, PATHSEP_STR);
        strcat(target, prefixedpath);
        status = copy_file(source, target);
        free(target);
    }
    return 0;
}


int build_all_callback(char *source_root, char *source, char *target_root) {
    // Remove trailing path seperators
    if (source_root[strlen(source_root) - 1] == PATHSEP)
        source_root[strlen(source_root) - 1] = 0;
    if (target_root[strlen(target_root) - 1] == PATHSEP)
        target_root[strlen(target_root) - 1] = 0;

    if (strstr(source, source_root) != source)
        return -1;

    char filename[1024];

    strcpy(filename, strrchr(source, PATHSEP) + 1);

    if ((!stricmp(filename, "$PBOPREFIX$")) || (!stricmp(filename, "$PBOPREFIX$.txt"))) {
        // get Temp Root Directory
        char tempfolder_root[2048];
        get_temp_path(tempfolder_root, sizeof(tempfolder_root));

        // get addon prefix
        char addonprefix[512];
        char directory[2048];
        strcpy(directory, source);
        *strrchr(directory, PATHSEP) = 0;
        get_prefixpath_directory(directory, addonprefix, sizeof(addonprefix));

        char tempfolder[1024];
        if (create_temp_folder(addonprefix, tempfolder, sizeof(tempfolder))) {
            errorf("Failed to create temp folder.\n");
            return 2;
        }

        char target[2048];
        strcpy(target, target_root);
        strcat(target, PATHSEP_STR);
        strcat(target, (strrchr(directory, PATHSEP) + 1));
        strcat(target, ".pbo");
        
        if (file_exists(target) && (!args.force)) {
            infof("Skipping pbo, already exists %s\n", target);
            return 0; // Skip Building PBO if exists & force disabled
        } else {
            return build(source, tempfolder, addonprefix, tempfolder_root, target);
        }
    };
    return 0;
}


int build_all_copy_callback(char *source_root, char *source, char *target_root) {
    // Remove trailing path seperators
    if (source_root[strlen(source_root) - 1] == PATHSEP)
        source_root[strlen(source_root) - 1] = 0;
    if (target_root[strlen(target_root) - 1] == PATHSEP)
        target_root[strlen(target_root) - 1] = 0;

    if (strstr(source, source_root) != source)
        return -1;

    char filename[1024];

    strcpy(filename, strrchr(source, PATHSEP) + 1);

    if ((!stricmp(filename, "$PBOPREFIX$")) || (!stricmp(filename, "$PBOPREFIX$.txt"))) {
        // get Temp Root Directory
        char tempfolder_root[2048];
        get_temp_path(tempfolder_root, sizeof(tempfolder_root));

        // get addon prefix
        char addonprefix[512];
        char directory[2048];
        strcpy(directory, source);
        *strrchr(directory, PATHSEP) = 0;
        get_prefixpath_directory(directory, addonprefix, sizeof(addonprefix));

        infof("Preparing temp folder %s...\n", addonprefix);
        char tempfolder[1024];
        if (create_temp_folder(addonprefix, tempfolder, sizeof(tempfolder))) {
            errorf("Failed to create temp folder.\n");
            return 2;
        }
        if (copy_directory(directory, tempfolder)) {
            errorf("Failed to copy to temp folder.\n");
            return 3;
        }
    };
    return 0;
}


int get_prefixpath_directory(char *source, char* addonprefix, size_t addonprefix_size) {
    /*
    * Checks directory for $PBOPREFIX$ or $PBOPREFIX$.txt
    * Returns 0 on if found prefix path otherwise returns -1 & normal path
    */

    // Remove trailing path seperators
    if (source[strlen(source) - 1] == PATHSEP)
        source[strlen(source) - 1] = 0;

    char pboprefix_folder[1024];
    char pboprefix_file[1024];
    FILE *f_prefix;
    int success = -1;

    strcpy(pboprefix_folder, source);

    while (true) {
        strcpy(pboprefix_file, pboprefix_folder);
        strcat(pboprefix_file, PATHSEP_STR);
        strcat(pboprefix_file, "$PBOPREFIX$");
        f_prefix = fopen(pboprefix_file, "rb");  // $PBOPREFIX$
        if (f_prefix) {
            char temp[1024];
            while (fgets(temp, sizeof(temp), f_prefix)) {
                if (strchr(temp, '=') == NULL) { // Ignore PboProject cfgWorlds setting
                    strncpy(addonprefix, temp, addonprefix_size);
                    strcat(addonprefix, source + strlens(pboprefix_folder));
                    success = 0;
                    break;
                }
            }
            fclose(f_prefix);
        } else {
            strcat(pboprefix_file, ".txt");
            f_prefix = fopen(pboprefix_file, "rb"); // $PBOPREFIX$.txt
            if (f_prefix) {
                char temp[1024];
                char key[1024];
                while (fgets(temp, sizeof(temp), f_prefix)) {
                    if (strchr(temp, '=') != NULL) { // Ignore PboProject cfgWorlds setting
                        strcpy(key, temp);
                        *(strchr(key, '=')) = 0;
                        if ((!stricmp(key, "prefix")) && ((strchr(temp, '=') + 1) != NULL)) {
                            strncpy(addonprefix, (strchr(temp, '=') + 1), addonprefix_size);
                            strcat(addonprefix, source + strlens(pboprefix_folder));
                            success = 0;
                            break;
                        };
                    }
                }
                fclose(f_prefix);
            }
        }
        if (success == 0)
            break;

        if ((strchr(pboprefix_folder, PATHSEP)) != NULL) {
            *(strrchr(pboprefix_folder, PATHSEP)) = 0;
             continue;
        }

        if (strrchr(source, PATHSEP) == NULL)
            strncpy(addonprefix, source, addonprefix_size);
        else
            strncpy(addonprefix, strrchr(source, PATHSEP) + 1, addonprefix_size);

        success = -1;
        break;
    }

    if (addonprefix[strlen(addonprefix) - 1] == '\n')
        addonprefix[strlen(addonprefix) - 1] = '\0';
    if (addonprefix[strlen(addonprefix) - 1] == '\r')
        addonprefix[strlen(addonprefix) - 1] = '\0';


    // replace pathseps on linux
#ifndef _WIN32
    char tmp[512] = "";
    char *p = NULL;
    for (p = addonprefix; *p; p++) {
        if (*p == '\\' && tmp[strlen(tmp) - 1] == '/')
            continue;
        if (*p == '\\')
            tmp[strlen(tmp)] = '/';
        else
            tmp[strlen(tmp)] = *p;
        tmp[strlen(tmp) + 1] = 0;
    }
    addonprefix[0] = 0;
    strcat(addonprefix, tmp);
#endif
    return success;
}


int copy_directory_keep_prefix_path(char *source) {
    /*
    * Copy the entire directory given with source to the target folder.
    * Returns 0 on success and a non-zero integer on failure.
    */
    char addonprefix[512];
    *addonprefix = 0;
    int success = get_prefixpath_directory(source, addonprefix, sizeof(addonprefix));
    if (success) {
        nwarningf("build-missing-pboprefix","Failed to get prefix for %s using %s.\n", source, addonprefix);
    }
    char tempfolder[1024];
    if (create_temp_folder(addonprefix, tempfolder, sizeof(tempfolder))) {
        errorf("Failed to create temp folder.\n");
        return 2;
    }
    if (copy_directory(source, tempfolder)) {
        errorf("Failed to copy to temp folder.\n");
        remove_folder(tempfolder);
        return 3;
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

    return traverse_directory(source, false, copy_callback, target);
}


int copy_includes(char *source, char *target) {
    /*
     * Copy the all include files (via file extension) recursively in source to the target folder.
	 * Also copies $PBOPREFIX$ aswell
     * Returns 0 on success and a non-zero integer on failure.
     */

    // Remove trailing path seperators
    if (source[strlen(source) - 1] == PATHSEP)
        source[strlen(source) - 1] = 0;
    if (target[strlen(target) - 1] == PATHSEP)
        target[strlen(target) - 1] = 0;

    return traverse_directory(source, false, copy_includes_callback, target);
}


#ifdef _WIN32
int copy_bulk_p3ds_dependencies_callback(char *source_root, char *source, struct build_ignore_data *data) {
    char *ext = strrchr(source, '.');
    if (ext != NULL) {
        if (!stricmp(ext, ".p3d")) {
            progressf();
            if (is_mlod(source) == 0) {
                int ret = get_p3d_dependencies(source, data->tempfolder_root);
                if (ret > 0)
                    return ret;
            } else {
                build_add_ignore(source, data);
                return 0;
            }
        } else if (!stricmp(ext, ".wrp")) {
            build_add_ignore(source, data);
            return 0;
        }
    }
    return 0;
}


int copy_bulk_p3ds_dependencies(char *source, struct build_ignore_data *data) {
    /*
    * Recursively scans p3ds, checks if MLOD/OLOD.
    * Renames OLODs to filename.p3d.ignore (to prevent binarize crash)
    * Gets file dependencies for MLOD p3ds
    */
    return traverse_directory(source, false, copy_bulk_p3ds_dependencies_callback, data);
}
#endif


int build_all(char *source, char *target) {
    /*
    */

    // Remove trailing path seperators
    if (source[strlen(source) - 1] == PATHSEP)
        source[strlen(source) - 1] = 0;
    if (target[strlen(target) - 1] == PATHSEP)
        target[strlen(target) - 1] = 0;

    return traverse_directory(source, false, build_all_callback, target);
}


int build_all_copy(char *source, char *target) {
    /*
    */

    // Remove trailing path seperators
    if (source[strlen(source) - 1] == PATHSEP)
        source[strlen(source) - 1] = 0;
    if (target[strlen(target) - 1] == PATHSEP)
        target[strlen(target) - 1] = 0;

    return traverse_directory(source, false, build_all_copy_callback, target);
}
