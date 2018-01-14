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
#include <errno.h>
#include <fts.h>
#endif

#include "docopt.h"
#include "filesystem.h"
#include "utils.h"
#include "rapify.h"
#include "p3d.h"
#include "binarize.h"
#include "utils.h"
#include "unistdwrapper.h"


bool warned_bi_not_found = false;


#ifdef _WIN32

int check_bis_binarize() {
    // Find binarize.exe
    long unsigned buffsize;
    long success;

    buffsize = sizeof(wc_binarize);
    success = RegGetValue(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath",
            RRF_RT_ANY, NULL, wc_binarize, &buffsize);

    if (success != ERROR_SUCCESS)
        return -2;

    wcscat(wc_binarize, L"/steamapps/common/Arma 3 Tools/Binarize/binarize.exe");
    if (!wc_file_exists(wc_binarize))
        return -3;

    return 1;
}

int get_p3d_dependencies(char *source, char *target, char *tempfolder, wchar_t *wc_temp) {
	wchar_t wc_source[2048];

    char temp[2048];
    char filename[2048];
    char *dependencies[MAXTEXTURES];
    FILE *f_source;
    struct mlod_lod *mlod_lods;

    int32_t num_lods;
    int i;
    int j;
    int k;

    // Read P3D and create a list of required files
    f_source = fopen(source, "rb");
    if (!f_source)
        return 1;

    fseek(f_source, 8, SEEK_SET);
    fread(&num_lods, 4, 1, f_source);
    mlod_lods = (struct mlod_lod *)malloc(sizeof(struct mlod_lod) * num_lods);
    num_lods = read_lods(f_source, mlod_lods, num_lods);
    fflush(stdout);
    if (num_lods < 0)
        return 2;

    fclose(f_source);

    memset(dependencies, 0, MAXTEXTURES * 2);
    for (i = 0; i < num_lods; i++) {
        for (j = 0; j < mlod_lods[i].num_faces; j++) {
            if (strlen(mlod_lods[i].faces[j].texture_name) > 0 && mlod_lods[i].faces[j].texture_name[0] != '#') {
                for (k = 0; k < MAXTEXTURES; k++) {
                    if (dependencies[k] == 0)
                        break;
                    if (stricmp(mlod_lods[i].faces[j].texture_name, dependencies[k]) == 0)
                        break;
                }
                if (k < MAXTEXTURES && dependencies[k] == 0) {
                    dependencies[k] = (char *)malloc(2048);
                    strcpy(dependencies[k], mlod_lods[i].faces[j].texture_name);
                }
            }
            if (strlen(mlod_lods[i].faces[j].material_name) > 0 && mlod_lods[i].faces[j].material_name[0] != '#') {
                for (k = 0; k < MAXTEXTURES; k++) {
                    if (dependencies[k] == 0)
                        break;
                    if (stricmp(mlod_lods[i].faces[j].material_name, dependencies[k]) == 0)
                        break;
                }
                if (k < MAXTEXTURES && dependencies[k] == 0) {
                    dependencies[k] = (char *)malloc(2048);
                    strcpy(dependencies[k], mlod_lods[i].faces[j].material_name);
                }
            }
        }

        free(mlod_lods[i].points);
        free(mlod_lods[i].facenormals);
        free(mlod_lods[i].faces);
        free(mlod_lods[i].mass);
        free(mlod_lods[i].sharp_edges);

        for (j = 0; j < mlod_lods[i].num_selections; j++) {
            free(mlod_lods[i].selections[j].points);
            free(mlod_lods[i].selections[j].faces);
        }

        free(mlod_lods[i].selections);
    }
    free(mlod_lods);

    // Create a temporary folder to isolate the target file and copy it there
    if (strchr(source, '\\') != NULL)
        strcpy(filename, strrchr(source, '\\') + 1);
    else
        strcpy(filename, source);

    if (create_temp_folder(filename, tempfolder, 2048)) {
        errorf("Failed to create temp folder.\n");
        return 1;
    }

    strcpy(temp, (strchr(target, PATHSEP) == NULL) ? target : strrchr(target, PATHSEP) + 1);
    strcpy(filename, tempfolder);
    strcat(filename, temp);

    mbstowcs(wc_source, source, 2048);
    GetFullPathName(wc_source, 2048, wc_temp, NULL);
    wcstombs(temp, wc_temp, 2048);

    if (copy_file(temp, filename)) {
        errorf("Failed to copy %s to temp folder.\n", temp);
        return 2;
    }

    // Try to find the required files and copy them there too
    strcpy(temp, source);
    strcpy(strrchr(temp, '\\') + 1, "config.cpp");
    strcpy(filename, tempfolder);
    strcat(filename, "config.cpp");
    copy_file(temp, filename);

    strcpy(temp, source);
    strcpy(strrchr(temp, '\\') + 1, "model.cfg");
    strcpy(filename, tempfolder);
    strcat(filename, "model.cfg");
    copy_file(temp, filename);

    for (i = 0; i < MAXTEXTURES; i++) {
        if (dependencies[i] == 0)
            break;

        *filename = 0;
        if (dependencies[i][0] != '\\')
            strcpy(filename, "\\");
        strcat(filename, dependencies[i]);

        if (find_file(filename, "", temp)) {
            warningf("Failed to find file %s.\n", filename);
            continue;
        }

        strcpy(filename, tempfolder);
        strcat(filename, dependencies[i]);

        if (copy_file(temp, filename)) {
            errorf("Failed to copy %s to temp folder.\n", temp);
            return 3;
        }

        free(dependencies[i]);
    }
	return 0;
}

int attempt_bis_binarize(char *source, char *target) {
    /*
     * Attempts to find and use the BI binarize.exe for binarization. If the
     * exe is not found, a negative integer is returned. 0 is returned on
     * success and a positive integer on failure.
     */

    extern int current_operation;
    extern char current_target[2048];
    SECURITY_ATTRIBUTES secattr = { sizeof(secattr) };
    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION processInfo;
    long unsigned buffsize;
    long success;

    wchar_t wc_command[2048];
	char temp[2048];
	wchar_t wc_temp[2048];
	char tempfolder[2048];
	wchar_t wc_tempfolder[2048];

	int i;

    current_operation = OP_P3D;
    strcpy(current_target, source);

    if (getenv("NATIVEBIN"))
        return -1;

    for (i = 0; i < strlen(source); i++)
        source[i] = (source[i] == '/') ? '\\' : source[i];

    for (i = 0; i < strlen(target); i++)
        target[i] = (target[i] == '/') ? '\\' : target[i];

    if (!wc_file_exists(wc_binarize))
        return -3;

	success = get_p3d_dependencies(source, target, tempfolder, wc_temp);
	if (success > 0)
		return success;

    // Call binarize.exe
	mbstowcs(wc_tempfolder, tempfolder, 2048);

	wchar_t wc_target[2048];
    mbstowcs(wc_target, target, 2048);
    GetFullPathName(wc_target, 2048, wc_temp, NULL);
    wcstombs(temp, wc_temp, 2048);
    *(strrchr(temp, PATHSEP)) = 0;
	mbstowcs(wc_temp, temp, 2048);

	if (wcslens(wc_binpath) <= 0) {
		swprintf(wc_command, 2048, L"\"%ls\" -norecurse -always  -maxProcesses=0 %ls %ls",
		wc_binarize, wc_tempfolder, wc_temp);
	} else {
		swprintf(wc_command, 2048, L"\"%ls\" -norecurse -always -maxProcesses=0 -binPath=%ls %ls %ls",
		wc_binarize, wc_binpath, wc_temp, wc_tempfolder, wc_temp);
	}

    if (getenv("BIOUTPUT")) {
		char command[2048];
		wcstombs(command, wc_command, 2048);
		debugf("cmdline: %s\n", command);
    }

    if (!getenv("BIOUTPUT")) {
        secattr.lpSecurityDescriptor = NULL;
        secattr.bInheritHandle = TRUE;
        info.hStdOutput = info.hStdError = CreateFile(L"NUL", GENERIC_WRITE, 0, &secattr, OPEN_EXISTING, 0, NULL);
        info.dwFlags |= STARTF_USESTDHANDLES;
    }

    if (CreateProcess(NULL, wc_command, NULL, NULL, TRUE, 0, NULL, NULL, &info, &processInfo)) {
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    } else {
        errorf("Failed to binarize %s.\n", source);
        return 3;
    }

    if (getenv("BIOUTPUT"))
        debugf("done with binarize.exe\n");

    // Clean Up
    if (remove_folder(tempfolder)) {
        errorf("Failed to remove temp folder.\n");
        return 4;
    }

    return success;
}
#endif


int binarize(char *source, char *target, bool force_p3d) {
    /*
     * Binarize the given file. If source and target are identical, the target
     * is overwritten. If the source is a P3D, it is converted to ODOL. If the
     * source is a rapifiable type (cpp, ext, etc.), it is rapified.
     *
     * If the file type is not recognized, -1 is returned. 0 is returned on
     * success and a positive integer on error.
     *
     * Force Option is only for when using BIS Tools to force to binarize a p3d (individually)
     */

    char fileext[64];
#ifdef _WIN32
    extern bool warned_bi_not_found;
#endif

    if (strchr(source, '.') == NULL)
        return -1;

    strncpy(fileext, strrchr(source, '.'), 64);

    if (!strcmp(fileext, ".cpp") ||
            !strcmp(fileext, ".rvmat") ||
            !strcmp(fileext, ".ext"))
        return rapify_file(source, target);

    if (!strcmp(fileext, ".p3d")) {
#ifdef _WIN32
        if (!force_p3d) {
            // Skip Binarize P3D, do them in Builk with BIS Tools
            if (found_bis_binarize >= 0)
                return 0;
        } else {
            // Binarize P3D (individually) i.e binarize commandline argument
            int success = attempt_bis_binarize(source, target);
            if (success >= 0)
                return success;
        }
        if (!warned_bi_not_found) {
            warningf("Failed to find BI tools, using internal binarizer.\n");
            warned_bi_not_found = true;
        }
#endif
        return mlod2odol(source, target);
    }

    return -1;
}


int cmd_binarize() {
    extern DocoptArgs args;

    // check if target already exists
    if (access(args.target, F_OK) != -1 && !args.force) {
        errorf("File %s already exists and --force was not set.\n", args.target);
        return 1;
    }

    int success = binarize(args.source, args.target, true);

    if (success == -1) {
        errorf("File is no P3D and doesn't seem rapifiable.\n");
        return 1;
    }

    return success;
}
