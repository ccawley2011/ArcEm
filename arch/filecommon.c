/*
  arch/filecommon.c

  (c) 2011 Jeffrey Lee <me@phlamethrower.co.uk>

  Common file handling routines

  Part of Arcem released under the GNU GPL, see file COPYING
  for details.

*/

#include <stdio.h>
#include <stddef.h>
#if defined _MSC_VER || defined __WATCOMC__
# include <io.h>
#else
# include <unistd.h>
#endif

#include "armarc.h"
#include "dbugsys.h"
#include "filecalls.h"
#include "displaydev.h"
#include "ControlPane.h"

#define USE_FILEBUFFER

/* Note: Musn't be used as a parameter to ReadEmu/WriteEmu! */
static ARMword temp_buf_word[32768/4];
static uint8_t *temp_buf = (uint8_t *) temp_buf_word;

#ifdef USE_FILEBUFFER
/* File buffering */

#define MAX_FILEBUFFER (1024*1024)
#define MIN_FILEBUFFER (32768)

static uint8_t *buffer=0;
static size_t buffer_size=0;

static void ensure_buffer_size(size_t buffer_size_needed)
{
  if (buffer_size_needed > buffer_size) {
    buffer = realloc(buffer, buffer_size_needed);
    if (!buffer) {
      warn_data("filecommon could not increase buffer size to %u bytes\n",
              (ARMword) buffer_size_needed);
    }
    buffer_size = buffer_size_needed;
  }
}

bool filebuffer_inuse = false;
FILE *filebuffer_file = NULL;
size_t filebuffer_remain = 0; /* Reads: Total amount left to buffer. Writes: Total amount the user said he was going to write */
size_t filebuffer_buffered = 0; /* Reads: How much is currently in the buffer. Writes: Total amount collected by filebuffer_write() */
size_t filebuffer_offset = 0; /* Reads/writes: Current offset within buffer */

static void filebuffer_fill(void)
{
  size_t temp = MIN(filebuffer_remain,MAX_FILEBUFFER);
  filebuffer_offset = 0;
  filebuffer_buffered = File_Read(filebuffer_file,buffer,temp);
  if(filebuffer_buffered != temp)
    filebuffer_remain = 0;
  else
    filebuffer_remain -= filebuffer_buffered;
}

static void filebuffer_initread(FILE *pFile,size_t uCount)
{
  size_t temp;
  filebuffer_inuse = false;
  filebuffer_file = pFile;
  if(uCount <= MIN_FILEBUFFER)
    return;
  filebuffer_inuse = true;
  temp = MIN(uCount,MAX_FILEBUFFER);
  ensure_buffer_size(temp);
  filebuffer_remain = uCount;
  filebuffer_fill();
}

static size_t filebuffer_read(uint8_t *pBuffer,size_t uCount,bool endian)
{
  size_t ret, avail;
  if(!filebuffer_inuse)
  {
    if(endian)
      return File_ReadEmu(filebuffer_file,pBuffer,uCount);
    else
      return File_Read(filebuffer_file,pBuffer,uCount);
  }
  ret = 0;
  while(uCount)
  {
    if(filebuffer_buffered == filebuffer_offset)
    {
      if(!filebuffer_remain)
        return ret;
      filebuffer_fill();
    }
    avail = MIN(uCount,filebuffer_buffered-filebuffer_offset);
    if(endian)
      InvByteCopy(pBuffer,buffer+filebuffer_offset,avail);
    else
      memcpy(pBuffer,buffer+filebuffer_offset,avail);
    filebuffer_offset += avail;
    ret += avail;
    pBuffer += avail;
    uCount -= avail;
  }
  return ret;
}

static void filebuffer_initwrite(FILE *pFile,size_t uCount)
{
  size_t temp;
  filebuffer_inuse = false;
  filebuffer_file = pFile;
  filebuffer_remain = uCount; /* Actually treated as total writeable */
  filebuffer_buffered = 0;
  if(uCount <= MIN_FILEBUFFER)
    return;
  filebuffer_inuse = true;
  temp = MIN(uCount,MAX_FILEBUFFER);
  ensure_buffer_size(temp);
  filebuffer_offset = 0;
}

static void filebuffer_write(uint8_t *pBuffer,size_t uCount,bool endian)
{
  if(filebuffer_remain == filebuffer_buffered)
    return;
  if(!filebuffer_inuse)
  {
    size_t temp;
    uCount = MIN(uCount,filebuffer_remain-filebuffer_buffered);
    if(endian)
      temp = File_WriteEmu(filebuffer_file,pBuffer,uCount);
    else
      temp = File_Write(filebuffer_file,pBuffer,uCount);
    filebuffer_buffered += temp;
    if(temp != uCount)
      filebuffer_remain = filebuffer_buffered;
     return;
  }
  while(uCount)
  {
    size_t temp = MIN(uCount,buffer_size-filebuffer_offset);
    if(endian)
      ByteCopy(buffer+filebuffer_offset,pBuffer,temp);
    else
      memcpy(buffer+filebuffer_offset,pBuffer,temp);
    filebuffer_offset += temp;
    uCount -= temp;
    pBuffer += temp;
    if(filebuffer_offset == buffer_size)
    {
      /* Flush */
      size_t temp2 = File_Write(filebuffer_file,buffer,filebuffer_offset);
      filebuffer_buffered += temp2;
      if(temp2 != filebuffer_offset)
      {
        filebuffer_remain = filebuffer_buffered;
        return;
      }
      filebuffer_offset = 0;
    }
  }
}

static size_t filebuffer_endwrite(void)
{
  if(filebuffer_inuse && filebuffer_offset)
  {
    /* Flush */
    size_t temp2 = File_Write(filebuffer_file,buffer,filebuffer_offset);
    filebuffer_buffered += temp2;
  }
  return filebuffer_buffered;
}
#endif

#if defined _MSC_VER || defined __WATCOMC__
# define ftello ftell
# define fseeko fseek
#endif

/* Visual Studio doesn't have ftruncate; use _chsize instead */
#if defined _MSC_VER || defined __WATCOMC__
# define ftruncate _chsize
#endif

/**
 * File_Open
 *
 * Open the specified file
 *
 * @param sName Name of file to open
 * @param sMode Mode to open the file with
 * @returns File handle or NULL on failure
 */
FILE *File_Open(const char *sName, const char *sMode)
{
  return fopen(sName, sMode);
}

/**
 * File_Close
 *
 * Close the given file handle
 *
 * @param pFile File to close
 * @returns true on success or false on failure
 */
bool File_Close(FILE *pFile)
{
  return (fclose(pFile) == 0);
}

/**
 * File_Size
 *
 * Sets the position of the given file handle.
 *
 * @param pFile File to examine
 * @param iSize The new position of the file
 * @returns true on success or false on failure
 */
bool File_Seek(FILE *pFile, off_t iOffset)
{
  return (fseeko(pFile, iOffset, SEEK_SET) == 0);
}

/**
 * File_Size
 *
 * Return the size of the given file handle.
 *
 * @param pFile File to examine
 * @returns The size of the file or -1 on failure
 */
off_t File_Size(FILE *pFile)
{
  off_t pos, size;

  pos = ftello(pFile);
  fseeko(pFile, 0, SEEK_END);
  size = ftello(pFile);
  fseeko(pFile, pos, SEEK_SET);

  return size;
}

/**
 * File_Truncate
 *
 * Extend or truncate the given file handle to the specified
 * size. Null characters are appended if the file is extended.
 *
 * @param pFile File to truncate
 * @param iSize The new size of the file
 * @returns true on success or false on failure
 */
bool File_Truncate(FILE *pFile, off_t iSize)
{
#ifndef __amigaos3__
  int fd;

  /* Flush any pending I/O before moving to low-level I/O functions */
  if (fflush(pFile)) {
    return false;
  }

  /* Obtain underlying file descriptor for this FILE* */
  fd = fileno(pFile);
  if (fd < 0) {
    return false;
  }

  /* Set file to required extent */
  return (ftruncate(fd, iSize) == 0);
#else
  /* no ftruncate in libnix - not sure if this is a critical function */
  return false;
#endif
}

/**
 * File_Read
 *
 * Reads from the given file handle into the given buffer
 *
 * @param pFile File to read from
 * @param pBuffer Buffer to write to
 * @param uCount Number of bytes to read
 * @returns Number of bytes read
 */
size_t File_Read(FILE *pFile,void *pBuffer,size_t uCount)
{
  return fread(pBuffer,1,uCount,pFile);
}

/**
 * File_Write
 *
 * Writes data to the given file handle
 *
 * @param pFile File to write to
 * @param pBuffer Buffer to read from
 * @param uCount Number of bytes to write
 * @returns Number of bytes written
 */
size_t File_Write(FILE *pFile,const void *pBuffer,size_t uCount)
{
  return fwrite(pBuffer,1,uCount,pFile);
}

/**
 * File_WriteFill
 *
 * Writes a single byte a specified number of times to
 * the given file handle
 *
 * @param pFile File to write to
 * @param uVal Byte to write
 * @param uCount Number of bytes to write
 * @returns Number of bytes written
 */
size_t File_WriteFill(FILE *pFile,uint8_t uVal,size_t uCount)
{
  /* Split into chunks and copy into the temp buffer */
  size_t ret = 0;

  while(uCount > 0)
  {
    size_t count2 = MIN(sizeof(temp_buf),uCount);
    size_t written;
    memset(temp_buf,uVal,count2);

    written = File_Write(pFile,temp_buf,count2);
    ret += written;
    uCount -= written;
    if(written < count2)
      break;
  }
  return ret;
}

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
size_t File_ReadEmu(FILE *pFile,uint8_t *pBuffer,size_t uCount)
{
#ifdef HOST_BIGENDIAN
  /* The only way to safely read the data without running the risk of corrupting
     the destination buffer is to read into the temp buffer. (This is because
     the data will need endian swapping after reading, but we can't guarantee
     that we'll read all uCount bytes, so we can't easily pre-swap the buffer
     to avoid losing the original contents of the first/last words) */
  size_t ret = 0;
  while(uCount > 0)
  {
    int offset = ((int) pBuffer)&3;
    size_t count2 = MIN(sizeof(temp_buf)-offset,uCount);
    size_t read = File_Read(pFile,temp_buf+offset,count2);
    InvByteCopy(pBuffer,temp_buf+offset,read);
    ret += read;
    pBuffer += read;
    uCount -= read;

    if(read < count2)
      break;
  }
  return ret;
#else
  return File_Read(pFile,pBuffer,uCount);
#endif
}

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
size_t File_WriteEmu(FILE *pFile,const uint8_t *pBuffer,size_t uCount)
{
#ifdef HOST_BIGENDIAN
  /* Split into chunks and copy into the temp buffer */
  size_t ret = 0;
  while(uCount > 0)
  {
    int offset = ((int) pBuffer)&3;
    size_t count2 = MIN(sizeof(temp_buf)-offset,uCount);
    ByteCopy(temp_buf+offset,pBuffer,count2);
    size_t written = File_Write(pFile,temp_buf+offset,count2);
    ret += written;
    pBuffer += written;
    uCount -= written;
    if(written < count2)
      break;
  }
  return ret; 
#else
  return File_Write(pFile,pBuffer,uCount);
#endif
}

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
size_t File_ReadRAM(ARMul_State *state, FILE *pFile,ARMword uAddress,size_t uCount)
{
  size_t ret = 0;
#ifdef USE_FILEBUFFER
  filebuffer_initread(pFile,uCount);
#endif

  while(uCount > 0)
  {
    FastMapEntry *entry;
    FastMapRes res;
    size_t amt = MIN(4096-(uAddress&4095),uCount);

    uAddress &= UINT32_C(0x3ffffff);

    entry = FastMap_GetEntryNoWrap(state,uAddress);
    res = FastMap_DecodeWrite(entry,state->FastMapMode);
    if(FASTMAP_RESULT_DIRECT(res))
    {
      size_t temp, temp2;
      uint8_t *phy = (uint8_t *) FastMap_Log2Phy(entry,uAddress);

      /* Scan ahead to work out the size of this memory block */
      while(amt < uCount)
      {
        uint8_t *phy2;
        ARMword uAddrNext = (uAddress+amt) & UINT32_C(0x3ffffff);
        size_t amt2 = MIN(4096,uCount-amt);
        FastMapEntry *entry2 = FastMap_GetEntryNoWrap(state,uAddrNext);
        FastMapRes res2 = FastMap_DecodeRead(entry2,state->FastMapMode);
        if(!FASTMAP_RESULT_DIRECT(res2))
          break;
        phy2 = (uint8_t *) FastMap_Log2Phy(entry2,uAddrNext);
        if(phy2 != phy+amt)
          break;
        /* These fastmap pages are contiguous in physical memory */
        amt += amt2;
      }

#ifdef USE_FILEBUFFER
      temp = filebuffer_read(phy,amt,true);
#else
      temp = File_ReadEmu(pFile,phy,amt);
#endif

      /* Clobber emu funcs for that region */
      temp2 = (temp+(uAddress&3)+3)&~UINT32_C(3);
      FastMap_PhyClobberFuncRange(state,(ARMword *)(void *) (phy-(uAddress&3)),temp2);

      /* Update state */
      ret += temp;
      if(temp != amt)
        break;
      uAddress += (ARMword)temp;
      uCount -= temp;
    }
    else if(FASTMAP_RESULT_FUNC(res))
    {
      ARMword *w;
      /* Read into temp buffer */
#ifdef USE_FILEBUFFER
      size_t temp = filebuffer_read(temp_buf+(uAddress&3),amt,false);
#else
      size_t temp = File_Read(pFile,temp_buf+(uAddress&3),amt);
#endif
      size_t temp2 = temp;
      /* Deal with start bytes */
      uint8_t *c = temp_buf+(uAddress&3);
      while((uAddress&3) && temp2)
      {
        FastMap_StoreFunc(entry,state,uAddress,*c,FASTMAP_ACCESSFUNC_BYTE);
        uAddress++;
        temp2--;
        c++;
      }
      /* Deal with main body */
      w = (ARMword *)(void *) c;
      while(temp2 >= 4)
      {
        FastMap_StoreFunc(entry,state,uAddress,EndianSwap(*w),0);
        uAddress += 4;
        temp2 -= 4;
        w++;
        c += 4;
      }
      /* Deal with end bytes */
      while(temp2)
      {
        FastMap_StoreFunc(entry,state,uAddress,*c,FASTMAP_ACCESSFUNC_BYTE);
        uAddress++;
        temp2--;
        c++;
      }

      /* Update state */
      ret += temp;
      if(temp != amt)
        break;
      uCount -= temp;
    }
    else
    {
      /* Abort!
         Pretend as if the CPU accessed the memory */
      ARMul_DATAABORT(uAddress);
      break;
    }
  }
  return ret;
}

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
size_t File_WriteRAM(ARMul_State *state, FILE *pFile,ARMword uAddress,size_t uCount)
{
#ifdef USE_FILEBUFFER
  filebuffer_initwrite(pFile,uCount);
#else
  size_t ret = 0;
#endif

  while(uCount > 0)
  {
    FastMapEntry *entry;
    FastMapRes res;
    size_t amt = MIN(4096-(uAddress&4095),uCount);

    uAddress &= UINT32_C(0x3ffffff);

    entry = FastMap_GetEntryNoWrap(state,uAddress);
    res = FastMap_DecodeRead(entry,state->FastMapMode);
    if(FASTMAP_RESULT_DIRECT(res))
    {
      uint8_t *phy = (uint8_t *) FastMap_Log2Phy(entry,uAddress);

      /* Scan ahead to work out the size of this memory block */
      while(amt < uCount)
      {
        uint8_t *phy2;
        ARMword uAddrNext = (uAddress+amt) & 0x3ffffff;
        size_t amt2 = MIN(4096,uCount-amt);
        FastMapEntry *entry2 = FastMap_GetEntryNoWrap(state,uAddrNext);
        FastMapRes res2 = FastMap_DecodeRead(entry2,state->FastMapMode);
        if(!FASTMAP_RESULT_DIRECT(res2))
          break;
        phy2 = (uint8_t *) FastMap_Log2Phy(entry2,uAddrNext);
        if(phy2 != phy+amt)
          break;
        /* These fastmap pages are contiguous in physical memory */
        amt += amt2;
      }        

#ifdef USE_FILEBUFFER
      filebuffer_write(phy,amt,true);
      /* Update state */
      uAddress += (ARMword)amt;
      uCount -= amt;
#else
      {
      size_t temp = File_WriteEmu(pFile,phy,amt);

      /* Update state */
      ret += temp;
      if(temp != amt)
        break;
      uAddress += (ARMword)temp;
      uCount -= temp;
      }
#endif
    }
    else if(FASTMAP_RESULT_FUNC(res))
    {
      /* Copy into temp buffer so we can perform endian swapping */
      uint8_t *temp = temp_buf+(uAddress&3);
      size_t temp2 = (amt+(uAddress&3)+3)&~UINT32_C(3); /* How many words to read */
      
      /* Copy the data a word at a time */
      ARMword *w = (ARMword *)(void *) temp_buf;
      uAddress &= ~UINT32_C(3);
      while(temp2 >= 4)
      {
        *w = EndianSwap(FastMap_LoadFunc(entry,state,uAddress));
        uAddress += 4;
        temp2 -= 4;
        w++;
      }

#ifdef USE_FILEBUFFER
      filebuffer_write(temp,amt,false);
      /* Update state */
      uCount -= amt;
#else
      /* Write it out */
      temp2 = File_Write(pFile,temp,amt);

      /* Update state */
      ret += temp2;
      if(temp2 != amt)
        break;
      uCount -= temp2;
#endif
    }
    else
    {
      /* Abort!
         Pretend as if the CPU accessed the memory */
      ARMul_DATAABORT(uAddress);
      break;
    }
  }
#ifdef USE_FILEBUFFER
  return filebuffer_endwrite();
#else
  return ret;
#endif
}
