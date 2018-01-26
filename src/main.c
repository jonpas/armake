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

#include "docopt.h"
#include "img2paa.h"
#include "paa2img.h"
#include "binarize.h"
#include "build.h"
#include "unpack.h"
#include "derapify.h"
#include "filesystem.h"
#include "keygen.h"
#include "sign.h"
#include "utils.h"
#include "version.h"

#ifdef _WIN32
#include <windows.h>
#endif


int main(int argc, char *argv[]) {
#ifdef _WIN32
    // Disable QuickEdit on Console, to prevent user accidentally freezing program
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    DWORD dwPreviousMode = 0;
    DWORD dwNewMode = dwPreviousMode | ENABLE_MOUSE_INPUT;
    dwNewMode &= ~ENABLE_QUICK_EDIT_MODE;
    SetConsoleMode(hConsole, dwNewMode | ENABLE_EXTENDED_FLAGS);
#endif

    extern DocoptArgs args;
    extern char exclude_files[MAXEXCLUDEFILES][512];
    extern char include_folders[MAXINCLUDEFOLDERS][512];
    extern char muted_warnings[MAXWARNINGS][512];
    int i;
    int j;
    char *halp[] = {argv[0], "-h"};

    args = docopt(argc, argv, 1, VERSION);

    // Docopt doesn't yet support positional arguments
    if (!args.derapify && argc < ((args.keygen || args.inspect) ? 3 : 4))
        docopt(2, halp, 1, VERSION);

    if (!args.keygen && !args.inspect) {
        args.source = argv[argc - 2];
        if (args.source[strlen(args.source) - 1] == PATHSEP)
            args.source[strlen(args.source) - 1] = 0;

        if (args.source[0] == '-' && strlen(args.source) > 1)
            docopt(2, halp, 1, VERSION);
    }

    args.target = argv[argc - 1];
    if (args.target[strlen(args.target) - 1] == PATHSEP)
        args.target[strlen(args.target) - 1] = 0;

    if (args.target[0] == '-' && strlen(args.target) > 1)
        docopt(2, halp, 1, VERSION);

    if (args.derapify) {
        if (args.target[0] == '-' || argc <= 2) {
            strcpy(args.source, "-");
            strcpy(args.target, "-");
        } else if (args.source[0] == '-' || argc <= 3) {
            strcpy(args.source, args.target);
            strcpy(args.target, "-");
        }
    }

    // @todo
    strcpy(include_folders[0], ".");
    for (i = 0; i < argc - 1; i++) {
        if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--exclude") == 0) {
            for (j = 0; j < MAXEXCLUDEFILES && exclude_files[j][0] != 0; j++);
                strncpy(exclude_files[j], argv[i + 1], sizeof(exclude_files[j]));
        }
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--include") == 0) {
            for (j = 0; j < MAXINCLUDEFOLDERS && include_folders[j][0] != 0; j++);
                strncpy(include_folders[j], argv[i + 1], sizeof(include_folders[j]));
            if (include_folders[j][strlen(include_folders[j]) - 1] == PATHSEP)
                include_folders[j][strlen(include_folders[j]) - 1] = 0;
        }
        if (strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--includeforce") == 0) {
            for (j = 0; j < MAXINCLUDEFOLDERS && includeforce_folders[j][0] != 0; j++);
            strncpy(includeforce_folders[j], argv[i + 1], sizeof(includeforce_folders[j]));
            if (includeforce_folders[j][strlen(includeforce_folders[j]) - 1] == PATHSEP)
                includeforce_folders[j][strlen(includeforce_folders[j]) - 1] = 0;
        }
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--warning") == 0) {
            for (j = 0; j < MAXWARNINGS && muted_warnings[j][0] != 0; j++);
                strncpy(muted_warnings[j], argv[i + 1], sizeof(muted_warnings[j]));
        }
        if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--key") == 0)
            args.privatekey = argv[i + 1];
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--type") == 0)
            args.paatype = argv[i + 1];
        if (strcmp(argv[i], "-T") == 0 || strcmp(argv[i], "--temppath") == 0) {
            args.temppath = argv[i + 1];
            if ((strlens(args.temppath) == 0) || 
                (strchr(args.temppath, PATHSEP) == NULL) ||
                (strcmp(strchr(args.temppath, PATHSEP), PATHSEP_STR) == 0)) {
                    errorf("Bad Temp Path: %s", args.temppath);
                    return 1;
            }
        };
    }


    #ifdef _WIN32
        found_bis_binarize = check_bis_binarize();
        if (found_bis_binarize) {

            wchar_t wc_include_folder[512];
            for (i = 1; i < MAXINCLUDEFOLDERS && include_folders[i][0] != 0; i++) { //Skip Current Directory
                mbstowcs(wc_include_folder, include_folders[i], 512);
                wcscat(wc_addonpaths, L" -addon=");
                wcscat(wc_addonpaths, wc_include_folder);
            }
        };
    #endif

    if (args.binarize)
        return cmd_binarize();
    if (args.build)
        return cmd_build();
    if (args.inspect)
        return cmd_inspect();
    if (args.unpack)
        return cmd_unpack();
    if (args.cat)
        return cmd_cat();
    if (args.derapify)
        return cmd_derapify();
    if (args.keygen)
        return cmd_keygen();
    if (args.sign)
        return cmd_sign();
    if (args.paa2img)
        return cmd_paa2img();
    if (args.img2paa)
        return cmd_img2paa();


    docopt(2, halp, 1, VERSION);
    return 1;
}
