/* filecalls.c RISC OS implimentatation of the abstracted interface to accesing host
   OS file and Directory functions.
   Copyright (c) 2005 Peter Howkins, covered under the GNU GPL see file COPYING for more
   details */

#ifdef __riscos__

/* ansi includes */
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* OS includes */
#include "kernel.h"
#include "swis.h"

/* application includes */
#include "filecalls.h"
#include "dbugsys.h"

/* The RISC OS version of the directory struct */
struct DirEntry_s {
  Directory *hDirectory;
  int gbpb_entry;

  struct {
    ARMword load;
    ARMword exec;
    ARMword length;
    ARMword attribs;
    ARMword type;
    char name[80];
  } gbpb_buffer;
};

struct Directory_s {
  char *sPath;
  size_t sPathLen;

  DirEntry hDirEntry;
};

Directory *Directory_Open(const char *sPath)
{
  Directory *hDirectory;
  size_t sPathLen;

  assert(sPath);
  assert(*sPath);

  sPathLen = strlen(sPath);

  hDirectory = malloc(sizeof(Directory) + sPathLen + 1);
  if (!hDirectory)
    return NULL;

  memset(hDirectory, 0, sizeof(*hDirectory));
  hDirectory->hDirEntry.hDirectory = hDirectory;
  hDirectory->sPathLen = sPathLen;
  hDirectory->sPath = (char *)(hDirectory + 1);
  strcpy(hDirectory->sPath, sPath);

  return hDirectory;
}

void Directory_Close(Directory *hDirectory)
{
  free(hDirectory);
}

DirEntry *Directory_GetNextEntry(Directory *hDirectory)
{
  _kernel_oserror *err;
  DirEntry *hDirEntry;
  int found;

  assert(hDirectory);

  hDirEntry = &hDirectory->hDirEntry;
  err = _swix(OS_GBPB, _INR(0,6)|_OUTR(3,4), 10,
              hDirectory->sPath, &hDirEntry->gbpb_buffer, 1,
              hDirEntry->gbpb_entry, sizeof(hDirEntry->gbpb_buffer),
              NULL, &found, &hDirEntry->gbpb_entry);
  if(err || !found) {
    return NULL;
  }

  return hDirEntry;
}

static char *Directory_GetEntryPath(DirEntry *hDirEntry) {
  Directory *hDirectory = hDirEntry->hDirectory;
  const char *leaf = hDirEntry->gbpb_buffer.name;
  size_t len = hDirectory->sPathLen + strlen(leaf) + 1;
  char *path = malloc(len + 1);
  if (!path) {
    return NULL;
  }

  strcpy(path, hDirectory->sPath);
  strcat(path, ".");
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
  return hDirEntry->gbpb_buffer.name;
}

ObjectType Directory_GetEntryType(DirEntry *hDirEntry)
{
  assert(hDirEntry);
  return hDirEntry->gbpb_buffer.type;
}

bool Directory_GetEntrySize(DirEntry *hDirEntry, Offset *ulFilesize)
{
  assert(hDirEntry);
  assert(ulFilesize);
  *ulFilesize = hDirEntry->gbpb_buffer.length;
  return true;
}

bool Disk_GetInfo(const char *path, DiskInfo *d)
{
  _kernel_oserror *err;
  unsigned int free_lsw, free_msw, size_lsw, size_msw;

  assert(path);

  err = _swix(OS_FSControl, _INR(0,1)|_OUTR(0,1)|_OUTR(3,4), 55, path,
              &free_lsw, &free_msw, &size_lsw, &size_msw);
  if(!err) {
    d->size = size_lsw | ((uint64_t)size_msw << 32);
    d->free = free_lsw | ((uint64_t)free_msw << 32);
    return true;
  }

  err = _swix(OS_FSControl, _INR(0,1)|_OUT(0)|_OUT(2), 49, path,
              &free_lsw, &size_lsw);
  if(!err) {
    d->size = size_lsw;
    d->free = free_lsw;
    return true;
  }

  return false;
}

#endif
