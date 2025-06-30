/* filecalls.h abstracted interface to accesing host OS file and Directory functions
   Copyright (c) 2005 Peter Howkins, covered under the GNU GPL see file COPYING for more
   details */

#ifndef __FILECALLS_H
#define __FILECALLS_H

#include "../armdefs.h"
#include <stdio.h>

typedef uint64_t Offset;

typedef struct Directory_s Directory;
typedef struct DirEntry_s DirEntry;

typedef enum OBJECT_TYPE {
	OBJECT_TYPE_NOT_FOUND = 0,
	OBJECT_TYPE_FILE      = 1,
	OBJECT_TYPE_DIRECTORY = 2,
	OBJECT_TYPE_IMAGEFILE = 3
} ObjectType;

typedef struct DiskInfo
{
  uint64_t	size;		/**< Size of disk */
  uint64_t	free;		/**< Free space on disk */
} DiskInfo;

/**
 * Directory_Open
 *
 * Open a directory so that it's contents can be scanned
 *
 * @param sPath Location of directory to scan
 * @returns Directory handle or NULL on failure
 */
Directory *Directory_Open(const char *sPath);

/**
 * Directory_Close
 *
 * Close a previously opened Directory
 *
 * @param hDirectory Directory to close
 */
void Directory_Close(Directory *hDirectory);

/**
 * Directory_GetNextEntry
 *
 * Get the next entry in a directory
 *
 * @param hDirectory pointer to Directory to get entry from
 * @returns Directory entry handle or NULL on EndOfDirectory
 */
DirEntry *Directory_GetNextEntry(Directory *hDirectory);

/**
 * Directory_GetEntryName
 *
 * Get the name of the specified directory entry.
 *
 * @param hDirEntry Directory entry to get the name of
 * @returns The entry name or NULL on failure
 */
const char *Directory_GetEntryName(DirEntry *hDirEntry);

/**
 * Directory_GetEntryType
 *
 * Get the type of the specified directory entry.
 *
 * @param hDirEntry Directory entry to get the type of
 * @returns The entry type or OBJECT_TYPE_NOT_FOUND on failure
 */
ObjectType Directory_GetEntryType(DirEntry *hDirEntry);

/**
 * Directory_GetEntrySize
 *
 * Get the size of the specified directory entry.
 *
 * @param hDirEntry Directory entry to get the size of
 * @param ulFilesize An integer to be filled with the size
 * @returns true on success or false on failure
 */
bool Directory_GetEntrySize(DirEntry *hDirEntry, Offset *ulFilesize);

/**
 * Directory_OpenEntryFile
 *
 * Open the specified directory entry as a file.
 *
 * @param hDirEntry Directory entry to open
 * @param sMode Mode to open the file with
 * @returns File handle or NULL on failure
 */
FILE *Directory_OpenEntryFile(DirEntry *hDirEntry, const char *sMode);

/**
 * Return disk space information about a file system.
 *
 * @param path Pathname of object within file system
 * @param d    Pointer to disk_info structure that will be filled in
 * @return     On success 1 is returned, on error 0 is returned
 */
bool Disk_GetInfo(const char *path, DiskInfo *d);

/**
 * File_OpenAppData
 *
 * Open the specified file in the application data directory
 *
 * @param sName Name of file to open
 * @param sMode Mode to open the file with
 * @returns File handle or NULL on failure
 */
FILE *File_OpenAppData(const char *sName, const char *sMode);

/**
 * Directory_OpenAppDir
 *
 * Open the specified directory in the application directory
 *
 * @param sName of directory to scan
 * @returns Directory handle or NULL on failure
 */
Directory *Directory_OpenAppDir(const char *sName);

/* These next few are implemented in arch/filecommon.c */

/**
 * File_ReadEmu
 *
 * Reads from the given file handle into the given buffer
 * Data will be endian swapped to match the byte order of the emulated RAM
 *
 * @param pFile File to read from
 * @param pBuffer Buffer to write to
 * @param uCount Number of bytes to read
 * @returns Number of bytes read
 */
size_t File_ReadEmu(FILE *pFile,uint8_t *pBuffer,size_t uCount);

/**
 * File_WriteEmu
 *
 * Writes data to the given file handle
 * Data will be endian swapped to match the byte order of the emulated RAM
 *
 * @param pFile File to write to
 * @param pBuffer Buffer to read from
 * @param uCount Number of bytes to write
 * @returns Number of bytes written
 */
size_t File_WriteEmu(FILE *pFile,const uint8_t *pBuffer,size_t uCount);

/**
 * File_ReadRAM
 *
 * Reads from the given file handle into emulator memory
 * Data will be endian swapped to match the byte order of the emulated RAM
 *
 * @param pFile File to read from
 * @param uAddress Logical address of buffer to write to
 * @param uCount Number of bytes to read
 * @returns Number of bytes read
 */
size_t File_ReadRAM(ARMul_State *state,FILE *pFile,ARMword uAddress,size_t uCount);

/**
 * File_WriteRAM
 *
 * Writes data from emulator memory to the given file handle
 * Data will be endian swapped to match the byte order of the emulated RAM
 *
 * @param pFile File to write to
 * @param uAddress Logical address of buffer to read from
 * @param uCount Number of bytes to write
 * @returns Number of bytes written
 */
size_t File_WriteRAM(ARMul_State *state,FILE *pFile,ARMword uAddress,size_t uCount);

#endif /* __FILECALLS_H */
