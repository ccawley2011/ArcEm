/* filecalls.c posix implimentatation of the abstracted interface to accesing host
   OS file and Directory functions.
   Copyright (c) 2005 Peter Howkins, covered under the GNU GPL see file COPYING for more
   details */

/* ansi includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* application includes */
#include "filecalls.h"

/**
 * File_OpenAppData
 *
 * Open the specified file in the application data directory
 *
 * @param sName Name of file to open
 * @param sMode Mode to open the file with
 * @returns File handle or NULL on failure
 */
FILE *File_OpenAppData(const char *sName, const char *sMode)
{
    const char *sBasePath = "rom://";
    char *sPath;
    size_t sLen;
    FILE *f;

    sLen = strlen(sBasePath) + strlen(sName) + 1;
    sPath = malloc(sLen);
    if (!sPath) {
        return NULL;
    }

    strlcpy(sPath, sBasePath, sLen);
    strlcat(sPath, sName, sLen);
    f = fopen(sPath, sMode);

    free(sPath);
    return f;
}

/**
 * Directory_OpenAppDir
 *
 * Open the specified directory in the application directory
 *
 * @param sName of directory to scan
 * @returns Directory handle or NULL on failure
 */
Directory *Directory_OpenAppDir(const char *sName)
{
    const char *sBasePath = "rom://";
    char *sPath;
    size_t sLen;
    Directory *ret;

    sLen = strlen(sBasePath) + strlen(sName) + 1;
    sPath = malloc(sLen);
    if (!sPath) {
        return NULL;
    }

    strlcpy(sPath, sBasePath, sLen);
    strlcat(sPath, sName, sLen);
    ret = Directory_Open(sPath);

    free(sPath);
    return ret;
}
