/* filecalls.c win32 implimentatation of the abstracted interface to accesing host
   OS file and Directory functions.
   Copyright (c) 2005 Peter Howkins, covered under the GNU GPL see file COPYING for more
   details */

#ifdef _WIN32

/* ansi includes */
#include <assert.h>
#include <stdio.h>

/* windows includes */
#include <windows.h>

/* application includes */
#include "filecalls.h"
#include "dbugsys.h"


/* The windows version of the directory struct */
struct DirEntry_s {
  WIN32_FIND_DATAA w32fd;
  HANDLE hFile;
  bool bFirstEntry;

  Directory *hDirectory;
};

struct Directory_s {
  char *sPath;
  size_t sPathLen;

  DirEntry hDirEntry;
};

Directory *Directory_Open(const char *sPath)
{
  bool bNeedsEndSlash = 0;
  Directory *hDir;
  size_t sPathLen;

  assert(sPath);
  assert(*sPath);

  if(sPath[strlen(sPath)] != '/') {
    bNeedsEndSlash = true;
  }
  sPathLen = strlen(sPath) + (bNeedsEndSlash ? 1 : 0 ) + 3; /* Path + Endslash (if needed) + *.* + terminator */

  hDir = malloc(sizeof(Directory) + sPathLen + 1);
  if(NULL == hDir) {
    warn_data("Failed to allocate memory for directory handle\n");
    return NULL;
  }

  hDir->hDirEntry.hDirectory  = hDir;
  hDir->hDirEntry.bFirstEntry = true;
  hDir->hDirEntry.hFile       = NULL;
  hDir->sPathLen              = sPathLen;
  hDir->sPath                 = (char *)(hDir + 1);

  strcpy(hDir->sPath, sPath);
  if(bNeedsEndSlash) {
    strcat(hDir->sPath, "/");
  }
  strcat(hDir->sPath, "*.*");

  return hDir;
}

void Directory_Close(Directory *hDirectory)
{
  FindClose(hDirectory->hDirEntry.hFile);
  free(hDirectory);

}

DirEntry *Directory_GetNextEntry(Directory *hDirectory)
{
  DirEntry *hDirEntry = &hDirectory->hDirEntry;

  if(hDirEntry->bFirstEntry) {
    hDirEntry->hFile = FindFirstFileA(hDirectory->sPath, &hDirEntry->w32fd);
    hDirEntry->bFirstEntry = false;

    if(INVALID_HANDLE_VALUE == hDirEntry->hFile) {
      return NULL;
    }
  } else {
    /* second or later entry */
    if(0 == FindNextFileA(hDirEntry->hFile, &hDirEntry->w32fd)) {
      return NULL;
    }
  }

  return hDirEntry;
}

static char *Directory_GetEntryPath(DirEntry *hDirEntry) {
  Directory *hDirectory = hDirEntry->hDirectory;
  const char *leaf = hDirEntry->w32fd.cFileName;
  size_t len = hDirectory->sPathLen - 3 + strlen(leaf);
  char *path = malloc(len + 1);
  if (!path) {
    return NULL;
  }

  strcpy(path, hDirectory->sPath);
  path[hDirectory->sPathLen - 3] = 0;
  strcat(path, leaf);
  return path;
}

FILE *Directory_OpenEntryFile(DirEntry *hDirEntry, const char *sMode)
{
  char *sPath;
  FILE *f;

  sPath = Directory_GetEntryPath(hDirEntry);
  if (!sPath) {
    return NULL;
  }

  f = fopen(sPath, sMode);
  if (!f) {
    warn_data("Warning: could not fopen() entry \'%s\': %s\n",
              sPath, strerror(errno));
    free(sPath);
    return NULL;
  }

  free(sPath);
  return f;
}

const char *Directory_GetEntryName(DirEntry *hDirEntry)
{
  assert(hDirEntry);
  return hDirEntry->w32fd.cFileName;
}

ObjectType Directory_GetEntryType(DirEntry *hDirEntry)
{
  assert(hDirEntry);
  if (hDirEntry->w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    return OBJECT_TYPE_DIRECTORY;
  } else {
    return OBJECT_TYPE_FILE;
  }
}

bool Directory_GetEntrySize(DirEntry *hDirEntry, Offset *ulFilesize)
{
  assert(hDirEntry);
  assert(ulFilesize);
  *ulFilesize = hDirEntry->w32fd.nFileSizeLow | ((Offset)hDirEntry->w32fd.nFileSizeHigh << 32);
  return true;
}

bool Disk_GetInfo(const char *path, DiskInfo *d)
{
	ULARGE_INTEGER free, total;

	assert(path != NULL);
	assert(d != NULL);

	if (GetDiskFreeSpaceExA(path, &free, &total, NULL) == 0) {
		return false;
	}

	d->size = (uint64_t) total.QuadPart;
	d->free = (uint64_t) free.QuadPart;

	return true;
}

#endif

