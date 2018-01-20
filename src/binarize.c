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

#include "build.h"
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
    /*
     * Attempts to find BI binarize.exe for binarization. If the
     * exe is not found, a negative integer is returned. 0 is returned on
     * success and a positive integer on failure.
     */

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


int get_rvmat_dependencies(char *filename, char *tempfolder) {
    /*
    * Copies filename rvmat depenencies to tempfolder location
    * i.e if rvmat is using textures/bisurf from a3/, need to copy this into temp folder for BI binarize
    */

    struct filelist *files_previous;
    struct filelist *files;
    files = NULL;
    //infof("DEBUG: %s\n", filename);
    parse_file_get_dependencies(filename, &files);
    while (files != NULL) {
        progressf();
        if ((strncmp(files->filename, "#", 1) != 0) && (strncmp(files->filename, "(", 1) != 0)) {
            char texture_path_corrected[2048] = "\\";
            strcat(texture_path_corrected, files->filename);
            char temp_filename[2048];
            strcpy(temp_filename, tempfolder);
            strcat(temp_filename, files->filename);
            if (!file_exists(temp_filename)) {  // Check if file already exists in Temp //TODO add support for checking paa instead of tga etc
                if (find_file(texture_path_corrected, "", texture_path_corrected, true, true)) {
                    warningf("Failed to find 11 file %s\n", filename);
                }
                else {
                    strcpy(strrchr(temp_filename, '.'), strrchr(texture_path_corrected, '.')); // Incase copying .paa instead of .tga
                    if (copy_file(texture_path_corrected, temp_filename)) {
                        errorf("Failed to copy 11 %s to temp folder %s.\n", texture_path_corrected, temp_filename);
                        return 3;
                    }
                }
            }
        }

        files_previous = files;
        files = files->next;
        free(files_previous);
    }
    return 0;
}


int get_p3d_dependencies(char *source, char *tempfolder, bool bulk_binarize) {
    /*
    * Copies p3d depenencies to tempfolder location
    * i.e if p3d is using textures/rvmats from a3/  need to copy this into temp folder for BI binarize
    */
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
    if (!f_source) {
        fclose(f_source);
        return 1;
    }

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


    sprintf(temp, "%s%s", tempfolder, (strrchr(source, '\\') + 1));
    if (!bulk_binarize) {
        if (copy_file(source, temp)) {
            errorf("Failed to copy p3d to temp folder.\n");
            return 1;
        }

        // Try to find the required files and copy them there too
        // Copy <config>.cpp
        strcpy(temp, source);
        strcpy(strrchr(temp, '\\') + 1, "config.cpp");
        strcpy(filename, tempfolder);
        strcat(filename, "config.cpp");
        copy_file(temp, filename);

        // Copy <model>.cfg
        strcpy(temp, source);
        strcpy(strrchr(temp, '\\') + 1, "model.cfg");
        strcpy(filename, tempfolder);
        strcat(filename, "model.cfg");
        copy_file(temp, filename);

        // Copy <model_name>.cfg
        strcpy(temp, source);
        strcpy(strrchr(temp, '.') + 1, "cfg");
        strcpy(filename, tempfolder);
        strcpy(strrchr(filename, '\\') + 1, strrchr(temp, '\\') + 1);
        copy_file(temp, filename);

        // Copy <directory_name>.cfg -> <temp_directory_name>.cfg
        char directoryname[2048];
        strcpy(temp, source);
        *strrchr(temp, '\\') = 0;
        strcpy(directoryname, strrchr(temp, '\\') + 1);
        strcat(directoryname, ".cfg");
        strcpy(temp, source);
        strcpy(strrchr(temp, '\\') + 1, directoryname);

        char tempdirectoryname[2048];
        strcpy(filename, tempfolder);
        *strrchr(filename, '\\') = 0;
        strcpy(tempdirectoryname, strrchr(filename, '\\') + 1);
        strcat(tempdirectoryname, ".cfg");
        strcpy(filename, tempfolder);
        strcpy(strrchr(filename, '\\') + 1, tempdirectoryname);
        copy_file(temp, filename);
    }


    for (i = 0; i < MAXTEXTURES; i++) {
        if (dependencies[i] == 0)
            break;

        strcpy(filename, tempfolder);
        if (dependencies[i][0] != '\\')
            strcat(filename, dependencies[i]);
        else
            strcat(filename, dependencies[i]+1);

        if (!file_exists(filename)) {
            *filename = 0;
            *temp = 0;
            if (dependencies[i][0] != '\\')
                strcpy(filename, "\\");
            strcat(filename, dependencies[i]);

            if (find_file(filename, "", temp, true, true)) {
                warningf("Failed to find file %s.\n", filename);
                continue;
            } else {
                strcpy(strrchr(dependencies[i], '.'), strrchr(temp, '.'));
            }

            strcpy(filename, tempfolder);
            strcat(filename, dependencies[i]);

            if (copy_file(temp, filename)) {
                errorf("Failed to copy %s to temp folder %s.\n", temp, filename);
                return 3;
            }
        }
        char *ext = strrchr(filename, '.');
        if ((ext != NULL) && (stricmp(ext, ".rvmat") == 0))
            get_rvmat_dependencies(filename, tempfolder);


        free(dependencies[i]);
    }
    return 0;
}


int attempt_bis_binarize(char *source, char *target) {
    /*
     * Attempts to use the BI binarize.exe for binarization. If the
     * exe is not found, a negative integer is returned. 0 is returned on
     * success and a positive integer on failure.
     */
    infof("  Setting up %s.\n", source);

    extern int current_operation;
    extern char current_target[2048];
    SECURITY_ATTRIBUTES secattr = { sizeof(secattr) };
    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION processInfo;
    long success;

    size_t wc_command_len = 2048 + wcslens(wc_addonpaths);
    wchar_t *wc_command = malloc(sizeof(wchar_t) * (wc_command_len + 1));
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


    // Create a temporary folder to isolate the target file and copy it there
    char temp[2048];
    char filename[2048];
    strcpy(temp, (strchr(target, PATHSEP) == NULL) ? target : strrchr(target, PATHSEP) + 1);
    strcpy(filename, tempfolder);
    strcat(filename, temp);

    if (strchr(source, '\\') != NULL)
        strcpy(filename, strrchr(source, '\\') + 1);
    else
        strcpy(filename, source);

    if (create_temp_folder(filename, tempfolder, 2048)) {
        errorf("Failed to create temp folder.\n");
        return 1;
    }

    for (i = 0; i < MAXINCLUDEFOLDERS && include_folders[i][0] != 0; i++) {
        copy_includes(include_folders[i], tempfolder);
    }
    if (tempfolder[strlen(tempfolder) - 1] != PATHSEP) {  //copy_includes removes trailing PATHSEP
        strcat(tempfolder, PATHSEP_STR);
    }


    // Copy P3D Dependencies to temporary folder
    success = get_p3d_dependencies(source, tempfolder, false);
    if (success > 0)
        return success;

    // Call binarize.exe
    mbstowcs(wc_tempfolder, tempfolder, 2048);

    wchar_t wc_target[2048];
    mbstowcs(wc_target, target, 2048);
    GetFullPathName(wc_target, 2048, wc_temp, NULL);
    *(wcsrchr(wc_temp, PATHSEP)) = 0;

    if (wcslens(wc_addonpaths) <= 0)
        swprintf(wc_command, wc_command_len, L"\"%ls\" -norecurse -always -maxProcesses=0 %ls %ls",
            wc_binarize, wc_tempfolder, wc_temp);
    else
        swprintf(wc_command, wc_command_len, L"\"%ls\" -norecurse -always -maxProcesses=0 %ls %ls %ls",
            wc_binarize, wc_addonpaths, wc_tempfolder, wc_temp);

    if (getenv("BIOUTPUT")) {
        char *command = malloc(sizeof(char) * (wc_command_len + 1));
        wcstombs(command, wc_command, wc_command_len);
        debugf("cmdline: %s\n", command);
        free(command);
    }

    if (!getenv("BIOUTPUT")) {
        secattr.lpSecurityDescriptor = NULL;
        secattr.bInheritHandle = TRUE;
        info.hStdOutput = info.hStdError = CreateFile(L"NUL", GENERIC_WRITE, 0, &secattr, OPEN_EXISTING, 0, NULL);
        info.dwFlags |= STARTF_USESTDHANDLES;
    }

    infof("  Binarize %s.\n", source);
    if (CreateProcess(NULL, wc_command, NULL, NULL, TRUE, 0, NULL, wc_tempfolder, &info, &processInfo)) {
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    } else {
        errorf("Failed to binarize %s.\n", source);
        free(wc_command);
        return 3;
    }

    if (getenv("BIOUTPUT"))
        debugf("done with binarize.exe\n");

    // Clean Up
    if (remove_folder(tempfolder)) {
        errorf("Failed to remove temp folder.\n");
        free(wc_command);
        return 4;
    }

    free(wc_command);
    return success;
}


int attempt_bis_bulk_binarize(char *source) {
    /*
    * Attempts to use the BI binarize.exe to binarization source folder. If the
    * exe is not found, a negative integer is returned. 0 is returned on
    * success and a positive integer on failure.
    */

    current_operation = OP_BUILD;
    SECURITY_ATTRIBUTES secattr = { sizeof(secattr) };
    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION processInfo;

    size_t wc_command_len = 2048 + wcslens(wc_addonpaths);
    wchar_t *wc_command = malloc(sizeof(wchar_t) * (wc_command_len + 1));
    wchar_t wc_build[2048];

    mbstowcs(wc_build, source, 2048);

    char temppath[2048];
    wchar_t wc_temppath[2048];
    get_temp_path(temppath, 2048);
    strcat(temppath, PATHSEP_STR);
    mbstowcs(wc_temppath, temppath, 2048);

    infof("Checking for P3D(s) for dependencies...\n");
    copy_bulk_p3ds_dependencies(source, temppath);

    if (wcslens(wc_addonpaths) <= 0) {
        swprintf(wc_command, wc_command_len, L"\"%ls\" -always -maxProcesses=0 -textures=%ls %ls %ls",
            wc_binarize, wc_temppath, wc_build, wc_build);
    } else {
        swprintf(wc_command, wc_command_len, L"\"%ls\" -always -maxProcesses=0 -addon=%ls -textures=%ls %ls %ls",
            wc_binarize, wc_addonpaths, wc_temppath, wc_build, wc_build);
    }


    if (getenv("BIOUTPUT")) {
        char *command = malloc(sizeof(char) * (wc_command_len + 1));
        wcstombs(command, wc_command, wc_command_len);
        debugf("cmdline: %s\n", command);
        free(command);
    }

    if (getenv("BIOUTPUT")) {
        secattr.lpSecurityDescriptor = NULL;
        secattr.bInheritHandle = TRUE;
        info.hStdOutput = info.hStdError = CreateFile(L"NUL", GENERIC_WRITE, 0, &secattr, OPEN_EXISTING, 0, NULL);
        info.dwFlags |= STARTF_USESTDHANDLES;
    }

    infof("Binarzing bulk...\n");
    if (CreateProcess(NULL, wc_command, NULL, NULL, TRUE, 0, NULL, wc_temppath, &info, &processInfo)) {
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    } else {
        errorf("Failed to binarize %s.\n", source);
        free(wc_command);
        return 3;
    }
    
    infof("Restoring PreBinarized P3Ds...\n");
    build_revert_ignores();

    if (getenv("BIOUTPUT"))
        debugf("done with binarize.exe\n");

    free(wc_command);
    return 0;
}


int attempt_bis_texheader(char *target) {
    /*
    * Attempts to use the BI binarize.exe to create textheader.bin. If the
    * exe is not found, a negative integer is returned. 0 is returned on
    * success and a positive integer on failure.
    */
    SECURITY_ATTRIBUTES secattr = { sizeof(secattr) };
    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION processInfo;
    if (wcslens(wc_binarize) <= 0) {
        // Find binarize.exe
        int wc_binarize_buffsize = sizeof(wc_binarize);

        int success = RegGetValue(HKEY_CURRENT_USER, L"Software\\Bohemia Interactive\\arma 3 tools", L"path",
            RRF_RT_ANY, NULL, wc_binarize, &wc_binarize_buffsize);
        if (success != ERROR_SUCCESS)
            return -2;
        wcscat(wc_binarize, L"\\Binarize\\binarize.exe");
    }
    if (!wc_file_exists(wc_binarize))
        return -3;

    wchar_t wc_target[2048];
    mbstowcs(wc_target, target, 2048);

    wchar_t wc_command[2048];

    swprintf(wc_command, 2048, L"\"%ls\" -texheader %ls %ls",
        wc_binarize, wc_target, wc_target);

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

    if (CreateProcess(NULL, wc_command, NULL, NULL, TRUE, 0, NULL, wc_target, &info, &processInfo)) {
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    } else {
        return 3;
    }
    return 0;
}


int get_skeleton(char *skeleton_cfg, char *rtm, char *skeleton) {
    FILE *f_skeleton_cfg;
    f_skeleton_cfg = fopen(skeleton_cfg, "r");

    if (!f_skeleton_cfg) {
        strcpy(skeleton, "OFP2_ManSkeleton");
        nwarningf("binarize-rtm-configfile", "Couldn't find %s, using default skeleton.\n", skeleton_cfg);
        return -2;
    };
    char buffer[2048];
    char key[2048];
    char value[2048];
    char *temp;

    while (fgets(buffer, 2048, f_skeleton_cfg)) {
        trim(buffer, 2048);
        if ((strlens(buffer)) > 0) {
            temp = (strchr(buffer, '='));
            if (temp) {
                strcpy(key, buffer);
                *(strchr(key, '=')) = 0;
                trim(key, 2048);

                strcpy(value, (temp + 1));
                trim(value, 2048);

                if (stricmp(value, rtm)) {
                    strcpy(skeleton, value);
                    return 0;
                }
            }
        }
    }
    return -1;
}


int attempt_bis_binarize_rtm(char *source, char *target) {
    /*
     * Attempts to use the BI binarize.exe for binarization. If the
     * exe is not found, a negative integer is returned. 0 is returned on
     * success and a positive integer on failure.
     */

    SECURITY_ATTRIBUTES secattr = { sizeof(secattr) };
    STARTUPINFO info = { sizeof(info) };
    PROCESS_INFORMATION processInfo;
    long success = 0;

    char temp[2048];
    char tempfolder[2048];
    char rtmname[2048];
    char filename[2048];

    current_operation = OP_RTM;
    strcpy(current_target, source);

    if (getenv("NATIVEBIN"))
        return -1;

    for (int i = 0; i < strlen(source); i++)
        source[i] = (source[i] == '/') ? '\\' : source[i];

    for (int i = 0; i < strlen(target); i++)
        target[i] = (target[i] == '/') ? '\\' : target[i];


    // Create a temporary folder to isolate the target file and copy it there
    if (strchr(source, '\\') != NULL)
        strcpy(filename, strrchr(source, '\\') + 1);
    else
        strcpy(filename, source);

    if (create_temp_folder(filename, tempfolder, 2048)) {
        errorf("Failed to create temp folder.\n");
        return 1;
    }

    strcpy(rtmname, (strchr(target, PATHSEP) == NULL) ? target : strrchr(target, PATHSEP) + 1);
    strcpy(filename, tempfolder);
    strcat(filename, rtmname);

    wchar_t wc_build[2048];
    mbstowcs(wc_build, tempfolder, 2048);

    wchar_t wc_rtm_temp[2048];
    wchar_t wc_rtm_temp_full[2048];
    mbstowcs(wc_rtm_temp, target, 2048);
    GetFullPathName(wc_rtm_temp, 2048, wc_rtm_temp_full, NULL);
    wcstombs(temp, wc_rtm_temp_full, 2048);
    if (copy_file(temp, filename)) {
        errorf("Failed to copy %s to temp folder.\n", temp);
        return 2;
    }
    remove_file(temp);  // Delete target rtm if it exists, binarize won't overright rtm


                        // Try to find the required files and copy them there too
                        // Copy <config>.cpp

    // Copy <model>.cfg
    strcpy(temp, source);
    strcpy(strrchr(temp, '\\') + 1, "model.cfg");
    strcpy(filename, tempfolder);
    strcat(filename, "model.cfg");
    copy_file(temp, filename);

    // Copy <model_name>.cfg
    strcpy(temp, source);
    strcpy(strrchr(temp, '.') + 1, "cfg");
    strcpy(filename, tempfolder);
    strcpy(strrchr(filename, '\\') + 1, strrchr(temp, '\\') + 1);
    copy_file(temp, filename);

    // Copy <directory_name>.cfg -> <temp_directory_name>.cfg
    char directoryname[2048];
    strcpy(temp, source);
    *strrchr(temp, '\\') = 0;
    strcpy(directoryname, strrchr(temp, '\\') + 1);
    strcat(directoryname, ".cfg");
    strcpy(temp, source);
    strcpy(strrchr(temp, '\\') + 1, directoryname);

    char tempdirectoryname[2048];
    strcpy(filename, tempfolder);
    *strrchr(filename, '\\') = 0;
    strcpy(tempdirectoryname, strrchr(filename, '\\') + 1);
    strcat(tempdirectoryname, ".cfg");
    strcpy(filename, tempfolder);
    strcpy(strrchr(filename, '\\') + 1, tempdirectoryname);
    copy_file(temp, filename);

    // Copy rtms.armake
    strcpy(temp, source);
    strcpy(strrchr(temp, '\\') + 1, "rtms.armake");
    strcpy(filename, tempfolder);
    strcat(filename, "rtms.armake");
    copy_file(temp, filename);

    char skeleton[2048];
    wchar_t wc_skeleton[2048];
    get_skeleton(temp, rtmname, skeleton);
    mbstowcs(wc_skeleton, skeleton, 2048);

    // Temp Directory
    wchar_t wc_target[2048];
    wchar_t wc_temp[2048];
    mbstowcs(wc_temp, target, 2048);
    GetFullPathName(wc_temp, 2048, wc_target, NULL);
    *(wcsrchr(wc_target, '\\')) = 0;

    wchar_t wc_command[2048];
    swprintf(wc_command, 2048, L"\"%ls\" -always -maxProcesses=0 -skeleton=%ls %ls %ls",
        wc_binarize, wc_skeleton, wc_build, wc_target);

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

    if (CreateProcess(NULL, wc_command, NULL, NULL, TRUE, 0, NULL, wc_temp, &info, &processInfo)) {
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    } else {
        errorf("Failed to binarize %s.\n", source);
        return 3;
    }

    FILE *f_source;
    char buffer[8];
    wcstombs(temp, wc_rtm_temp, 2048);
    f_source = fopen(temp, "r");
    fseek(f_source, 0, SEEK_SET);
    fread(buffer, 4, 1, f_source);
    buffer[4] = 0;
    fclose(f_source);
    if (stricmp(buffer, "BMTR")) {
        char skeleton[2048];
        wcstombs(skeleton, wc_skeleton, 2048);
        nwarningf("binarize-rtm-skeleton", "Failed to binarize %s using skeleton %s\n", temp, skeleton);
        return -1;
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


int attempt_bis_binarize_wrp(char *source, char*target) {
    return -3;
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
     * Force Option is only for when using BIS Tools to force to binarize a file (individually)
     */

    char fileext[64];
#ifdef _WIN32
    extern bool warned_bi_not_found;
#endif

    if (strchr(source, '.') == NULL)
        return -1;

    strncpy(fileext, strrchr(source, '.'), 64);

    if (!stricmp(fileext, ".cpp") ||
            !stricmp(fileext, ".rvmat") ||
            !stricmp(fileext, ".ext"))
        return rapify_file(source, target);

    if (!stricmp(fileext, ".rtm")) {
        return attempt_bis_binarize_rtm(source, target);
    }

    if (!stricmp(fileext, ".p3d")) {
#ifdef _WIN32
        if (!force_p3d) {
            // Skip Binarize P3D, do them in bulk with BIS Tools
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
