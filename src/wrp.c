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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wrp.h"

#include "binarize.h"
#include "filesystem.h"
#include "p3d.h"
#include "preprocess.h"
#include "rapify.h"
#include "utils.h"


int wrp_add_dependency(char *filename, char *tempfolder_root) {
    /*
    * Reverts files back to original name,
    *   that were renamed by build_add_ignore
    *
    */
    struct filelist *ptr;

    char temp[2048];
    char dest[2048];
    char dest_folder[2048];


    // Check if file already exists
    strcpy(dest, tempfolder_root);
    strcat(dest, PATHSEP_STR);
    strcat(dest, filename);


    if (!stricmp(strrchr(dest, '.'), ".rvmat")) {
        if (!getenv("DISABLE_RVMATS_CHECK")) {
            int ret = rapify_file_get_files(dest, dest, tempfolder_root);
            progressf();
        }    
        return 0;  // Don't need rvmats to the list
    }

    // Scan List again for WRPs (want p3ds etc reverted before attempting to binarize wrp)
    ptr = wrp_temp_folderlist;
    while (ptr != NULL) {
        strcpy(temp, dest_folder);
        if (!stricmp(ptr->filename, dest))
            return 0; // Already Added;

        ptr = ptr->next;
    }

    // Add to List
    ptr = wrp_temp_folderlist;
    if (ptr == NULL) {
        ptr = malloc(sizeof(struct filelist));
        ptr->next = NULL;
        wrp_temp_folderlist = ptr;
    } else {
        while ((ptr->next) != NULL) {
            ptr = ptr->next;
        }
        ptr->next = malloc(sizeof(struct filelist));
        ptr->next->next = NULL;
        ptr = ptr->next;
    }
    strcpy(ptr->filename, dest);
    
    return 0;
}


int wrp_bin_dependency(char *tempfolder_root) {
    /*
    * Copies filename parent directory to tempfolder_root
    *   Copy parent & sub directories, faster than searching for location for each p3d/rvmat
    *   Majority of the time dependencies are in parent/sub directories.
    *   Also files will be scanned for dependencies afterwards before getting binarized.
    */
    struct filelist *ptr;
    struct filelist *ptr_previous;

    char filename[2048];
    char temp[2048];
    char source_folder[2048];
    char dest_folder[2048];

    // Scan List again for WRPs (want p3ds etc reverted before attempting to binarize wrp)
    ptr = wrp_temp_folderlist;
    while (ptr != NULL) {

        // Destination Folder
        strcpy(filename, ptr->filename);
        strcpy(dest_folder, ptr->filename);
        *(strrchr(dest_folder, '\\')) = 0;

        if (!file_exists(filename)) {
            strcpy(temp, (filename + strlen(tempfolder_root)));
            if (find_file(temp, "", source_folder, true, false)) {
                warningf("Failed to find file %s.\n", filename);
                return 1;
            }
            *(strrchr(source_folder, '\\')) = 0;
            copy_directory(source_folder, dest_folder); // Only copy files if missing, still need to binarize p3ds for wrp
        }

        ptr_previous = ptr;
        ptr = ptr->next;
        //free(ptr_previous);
    }
    ptr = wrp_temp_folderlist;
    while (ptr != NULL) {

        // Destination Folder
        strcpy(filename, ptr->filename);
        strcpy(dest_folder, ptr->filename);
        *(strrchr(dest_folder, '\\')) = 0;

        if (!file_exists(filename)) {
            get_p3d_dependencies(filename, tempfolder_root);
        }

        ptr_previous = ptr;
        ptr = ptr->next;
        free(ptr_previous);
    }
    wrp_temp_folderlist = NULL;
    return 0;
}


int wrp_parse_8WVR(FILE *wrp_map, char *tempfolder_root) {
    char buffer[2048];
    char dObjName[2048];
    float cellsize, elevation, dDir[3][4];
    int len, texturegrid, terraingrid;
    long noofmaterials;
    short materialindex, materiallength;
    unsigned long dObjIndex;

    fread(&texturegrid, 4, 1, wrp_map);
    printf("texture grid x: %d\n", texturegrid);
    fread(&texturegrid, 4, 1, wrp_map);
    printf("texture grid z: %d\n", texturegrid);

    fread(&terraingrid, 4, 1, wrp_map);
    printf("terrain grid x: %d\n", terraingrid);
    fread(&terraingrid, 4, 1, wrp_map);
    printf("terrain grid z: %d\n", terraingrid);

    // cell size, but how many digits??
    fread(&cellsize, 4, 1, wrp_map);
    printf("Cellsize: %f\n", cellsize);

    // reading elevations
    for (int i = 0; i < terraingrid * terraingrid; i++)
    {
        fread(&elevation, 4, 1, wrp_map);
        //printf("Elevation: %f\n", elevation);
    }

    // credits go to Mikero for helping me figure out the RVMAT parts
    // reading rvmat index
    for (int i = 0; i < texturegrid * texturegrid; i++)
    {
        fread(&materialindex, 2, 1, wrp_map);
//		printf("materialindex: %d\n", materialindex);
    }

    // noofmaterials
    fread(&noofmaterials, 4, 1, wrp_map);
    printf("noofmaterials: %ld\n", noofmaterials);

    // reading the first NULL material...
    fread(&materiallength, 4, 1, wrp_map);
    printf("1st NULL materiallength: %d\n", materiallength);

    //////////// now, insert  the following
    noofmaterials--; // remove that 1st one. from the count

    while (noofmaterials--) //read the rest (if any)
    {
        fread(&materiallength, 4, 1, wrp_map);
        if (!materiallength)
        {
            printf("%ld materiallength has no count\n", noofmaterials);
            return -1;
        }
        fread(&buffer, materiallength, 1, wrp_map);
        buffer[materiallength]=0; // make it asciiz
        fread(&materiallength, 4, 1, wrp_map);
        if (materiallength)
        {
            printf("%ld 2nd materiallength should be zero\n", noofmaterials);
            return -1;
        }
        else {
            wrp_add_dependency(buffer, tempfolder_root);
        }
    }

    // Start reading objects...

    printf ("Reading 3d objects...");

    for(;;)
    {
        fread(&dDir, sizeof(float), 3*4, wrp_map);
        fread(&dObjIndex, sizeof(long), 1, wrp_map);
        fread(&len, sizeof(long), 1, wrp_map);

        if (!len) break;// last object has no name associated with it, and the transfrom is garbage

        fread(dObjName,sizeof(char),len, wrp_map);
        dObjName[len] = 0;
        wrp_add_dependency(dObjName, tempfolder_root);
    }
    // should now be at eof
    //printf(" Done\nNumber of P3D objects: %ld\n", dObjIndex);

    printf ("All fine, 8WVR file read, exiting. Have a nice day!\n");
    return 0;
}

/*
int wrp_parse_4WVR(FILE *wrp_map, char *tempfolder_root) {
    char dObjName[2048];
    char sig[33];
    float dDir[3][4];
    int texturegrid;
    short materialindex;
    unsigned long dObjIndex;

    fread(&texturegrid, 4, 1, wrp_map);
    printf("Cells: %d\n", texturegrid);
    fread(&texturegrid, 4, 1, wrp_map);
//	printf("texture grid z: %d\n", texturegrid);

    // reading elevations
    for (int i = 0; i < texturegrid * texturegrid; i++)
    {
        // this was elevation before (float), but it supposed to be short??
        fread(&materialindex, sizeof(short), 1, wrp_map);
    }

    printf("Reading texture indexes...");

    // read textures indexes
    for (int i = 0; i < texturegrid * texturegrid; i++)
    {
        fread(&materialindex, sizeof(short), 1, wrp_map);
    }

    printf(" Done\nReading texture names...");

    // textures 32 char length and total of 512
    for (int ix = 0; ix < 512; ix++)
    {
        sig[0] = 0;
        fread(sig, 32, 1, wrp_map);
    }

    printf(" Done\nReading 3d objects...");

    while (!feof(wrp_map))
    {
        fread(&dDir, sizeof(float), 3*4, wrp_map);
        fread(&dObjIndex, sizeof(long), 1, wrp_map);
        fread(dObjName, 76, 1, wrp_map);
        //dObjName[76] = 0; // asciiz
    }
    // should now be at eof
    printf(" Done\nNumber of P3D objects: %ld\n", dObjIndex);

    printf ("All fine, 4WVR file read, exiting. Have a nice day!\n");
    fclose(wrp_map);
    return 0;
}
*/


int wrp_parse(char *source, char *tempfolder_root) {
    FILE *wrp_map;
    int ret;
    char sig[33];

    wrp_map = fopen(source, "rb");
    if (!wrp_map) {
        //TODO ERROR
        return -1;
    }

    fread(sig, 4, 1, wrp_map);
    sig[4] = 0;

    // 8WVR, ArmA style
    if (strcmp(sig, "8WVR") == 0)
        ret = wrp_parse_8WVR(wrp_map, tempfolder_root);

    /*
    // 4WVR, OFP style
    if (strcmp(sig, "4WVR") == 0)
        return wrp_parse_4WVR(wrp_map, tempfolder_root);
    */
    if (!ret)
        wrp_bin_dependency(tempfolder_root);
    if (wrp_map)
        fclose(wrp_map);
    return ret;
}