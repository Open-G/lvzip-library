/* zip.c -- IO on .zip files using zlib
   Version 1.1, February 14h, 2010
   part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications for Zip64 support
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

         Modifications for AES, PKWARE disk spanning
         Copyright (C) 2010-2014 Nathan Moinvaziri

		 For more info read MiniZip_info.txt

         Changes
   Oct-2009 - Mathias Svensson - Remove old C style function prototypes
   Oct-2009 - Mathias Svensson - Added Zip64 Support when creating new file archives
   Oct-2009 - Mathias Svensson - Did some code cleanup and refactoring to get better overview of some functions.
   Oct-2009 - Mathias Svensson - Added zipRemoveExtraInfoBlock to strip extra field data from its ZIP64 data
                                 It is used when recreting zip archive with RAW when deleting items from a zip.
                                 ZIP64 data is automaticly added to items that needs it, and existing ZIP64 data need to be removed.
   Oct-2009 - Mathias Svensson - Added support for BZIP2 as compression mode (bzip2 lib is required)
   Jan-2010 - back to unzip and minizip 1.0 name scheme, with compatibility layer

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define ZLIB_INTERNAL
#include "zlib.h"
#include "zip.h"

#ifdef STDC
#  include <stddef.h>
#  include <string.h>
#  include <stdlib.h>
#endif
#ifdef NO_ERRNO_H
    extern int errno;
#else
#   include <errno.h>
#endif

#ifdef HAVE_AES
#  define AES_METHOD          (99)
#  define AES_PWVERIFYSIZE    (2)
#  define AES_AUTHCODESIZE    (10)
#  define AES_MAXSALTLENGTH   (16)
#  define AES_VERSION         (0x0001)
#  define AES_ENCRYPTIONMODE  (0x03)

#  include "aes/aes.h"
#  include "aes/fileenc.h"
#  include "aes/prng.h"
#  include "aes/entropy.h"
#endif

#ifndef NOCRYPT
#define INCLUDECRYPTINGCODE_IFCRYPTALLOWED
#include "crypt.h"
#endif

#ifndef local
#  define local static
#endif
/* compile with -Dlocal if your debugger can't find static symbols */

#ifndef VERSIONMADEBY
# define VERSIONMADEBY   (0x0) /* platform depedent */
#endif

#ifndef Z_BUFSIZE
#define Z_BUFSIZE (64*1024) //(16384)
#endif

#ifndef Z_MAXFILENAMEINZIP
#define Z_MAXFILENAMEINZIP (256)
#endif

#ifndef ALLOC
# define ALLOC(size) (malloc(size))
#endif
#ifndef TRYFREE
# define TRYFREE(p) {if (p) free(p);}
#endif

/*
#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)
*/

/* I've found an old Unix (a SunOS 4.1.3_U1) without all SEEK_* defined.... */


// NOT sure that this work on ALL platform
#define MAKEULONG64(a, b) ((ZPOS64_T)(((unsigned long)(a)) | ((ZPOS64_T)((unsigned long)(b))) << 32))

#ifndef SEEK_CUR
#define SEEK_CUR    1
#endif

#ifndef SEEK_END
#define SEEK_END    2
#endif

#ifndef SEEK_SET
#define SEEK_SET    0
#endif

#ifndef DEF_MEM_LEVEL
#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif
#endif
const char zip_copyright[] =" zip 1.01 Copyright 1998-2004 Gilles Vollant - http://www.winimage.com/zLibDll";


#define SIZEDATA_INDATABLOCK (4096-(4*4))

#define DISKHEADERMAGIC          (0x08074b50)
#define LOCALHEADERMAGIC         (0x04034b50)
#define CENTRALHEADERMAGIC       (0x02014b50)
#define ENDHEADERMAGIC           (0x06054b50)
#define ZIP64ENDHEADERMAGIC      (0x06064b50)
#define ZIP64ENDLOCHEADERMAGIC   (0x07064b50)

#define FLAG_LOCALHEADER_OFFSET (0x06)
#define CRC_LOCALHEADER_OFFSET  (0x0e)

#define SIZECENTRALHEADER (0x2e) /* 46 */
#define SIZECENTRALHEADERLOCATOR    (0x14) /* 20 */
#define SIZECENTRALDIRITEM          (0x2e)
#define SIZEZIPLOCALHEADER          (0x1e)

typedef struct linkedlist_datablock_internal_s
{
  struct linkedlist_datablock_internal_s* next_datablock;
  uLong  avail_in_this_block;
  uLong  filled_in_this_block;
  uLong  unused; /* for future use and alignement */
  unsigned char data[SIZEDATA_INDATABLOCK];
} linkedlist_datablock_internal;

typedef struct linkedlist_data_s
{
    linkedlist_datablock_internal* first_block;
    linkedlist_datablock_internal* last_block;
} linkedlist_data;


typedef struct
{
    z_stream stream;            /* zLib stream structure for inflate */
#ifdef HAVE_BZIP2
    bz_stream bstream;          /* bzLib stream structure for bziped */
#endif
#ifdef HAVE_AES
    fcrypt_ctx aes_ctx;
    prng_ctx aes_rng[1];
#endif
    int  stream_initialised;    /* 1 is stream is initialised */
    uInt pos_in_buffered_data;  /* last written byte in buffered_data */

    ZPOS64_T pos_local_header;     /* offset of the local header of the file currenty writing */
    char* central_header;       /* central header data for the current file */
    uLong size_centralExtra;
    uLong size_centralheader;   /* size of the central header for cur file */
    uLong size_centralExtraFree; /* Extra bytes allocated to the centralheader but that are not used */
    uLong size_comment;
    uLong flag;                 /* flag of the file currently writing */

    int  method;                /* compression method written to file */
    int  compression_method;    /* compression method to use */
    int  raw;                   /* 1 for directly writing raw data */
    Byte buffered_data[Z_BUFSIZE];/* buffer contain compressed data to be writ*/
    uLong dosDate;
    uLong crc32;
    int  zip64;               /* Add ZIP64 extened information in the extra field */
    uLong number_disk;              /* number of current disk used for spanning ZIP */
    ZPOS64_T pos_zip64extrainfo;
    ZPOS64_T totalCompressedData;
    ZPOS64_T totalUncompressedData;
#ifndef NOCRYPT
    unsigned long keys[3];     /* keys defining the pseudo-random sequence */
    const z_crc_t* pcrc_32_tab;
    int crypt_header_size;
#endif
} curfile64_info;

typedef struct
{
    zlib_filefunc64_32_def z_filefunc;
    voidpf filestream;        /* io structore of the zipfile */
    voidpf filestream_with_CD;      /* io structure of the zipfile with the central dir */
    linkedlist_data central_dir;/* datablock with central dir in construction*/
    int  in_opened_file_inzip;  /* 1 if a file in the zip is currently writ.*/
    int append;                     /* append mode */
    curfile64_info ci;            /* info on the file curretly writing */

    ZPOS64_T begin_pos;            /* position of the beginning of the zipfile */
    ZPOS64_T add_position_when_writting_offset;
    ZPOS64_T number_entry;
    ZPOS64_T disk_size;             /* size of each disk */
    uLong number_disk;              /* number of the current disk, used for spanning ZIP */
    uLong number_disk_with_CD;      /* number the the disk with central dir, used for spanning ZIP */

#ifndef NO_ADDFILEINEXISTINGZIP
    char *globalcomment;
#endif

} zip64_internal;

local linkedlist_datablock_internal* allocate_new_datablock()
{
    linkedlist_datablock_internal* ldi;
    ldi = (linkedlist_datablock_internal*)
                 ALLOC(sizeof(linkedlist_datablock_internal));
    if (ldi!=NULL)
    {
        ldi->next_datablock = NULL ;
        ldi->filled_in_this_block = 0 ;
        ldi->avail_in_this_block = SIZEDATA_INDATABLOCK ;
    }
    return ldi;
}

local void free_datablock(linkedlist_datablock_internal* ldi)
{
    while (ldi!=NULL)
    {
        linkedlist_datablock_internal* ldinext = ldi->next_datablock;
        TRYFREE(ldi);
        ldi = ldinext;
    }
}

local void init_linkedlist(linkedlist_data* ll)
{
    ll->first_block = ll->last_block = NULL;
}

local void free_linkedlist(linkedlist_data* ll)
{
    free_datablock(ll->first_block);
    ll->first_block = ll->last_block = NULL;
}


local int add_data_in_datablock(linkedlist_data* ll, const void* buf, uLong len)
{
    linkedlist_datablock_internal* ldi;
    const unsigned char* from_copy;

    if (ll==NULL)
        return ZIP_INTERNALERROR;

    if (ll->last_block == NULL)
    {
        ll->first_block = ll->last_block = allocate_new_datablock();
        if (ll->first_block == NULL)
            return ZIP_INTERNALERROR;
    }

    ldi = ll->last_block;
    from_copy = (unsigned char*)buf;

    while (len>0)
    {
        uInt copy_this;
        uInt i;
        unsigned char* to_copy;

        if (ldi->avail_in_this_block==0)
        {
            ldi->next_datablock = allocate_new_datablock();
            if (ldi->next_datablock == NULL)
                return ZIP_INTERNALERROR;
            ldi = ldi->next_datablock ;
            ll->last_block = ldi;
        }

        if (ldi->avail_in_this_block < len)
            copy_this = (uInt)ldi->avail_in_this_block;
        else
            copy_this = (uInt)len;

        to_copy = &(ldi->data[ldi->filled_in_this_block]);

        for (i=0;i<copy_this;i++)
            *(to_copy+i)=*(from_copy+i);

        ldi->filled_in_this_block += copy_this;
        ldi->avail_in_this_block -= copy_this;
        from_copy += copy_this ;
        len -= copy_this;
    }
    return ZIP_OK;
}



/****************************************************************************/

#ifndef NO_ADDFILEINEXISTINGZIP
/* ===========================================================================
   Inputs a long in LSB order to the given file
   nbByte == 1, 2 ,4 or 8 (byte, short or long, ZPOS64_T)
*/

local int zip64local_putValue OF((const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, ZPOS64_T x, int nbByte));
local int zip64local_putValue (const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, ZPOS64_T x, int nbByte)
{
    unsigned char buf[8];
    int n;
    for (n = 0; n < nbByte; n++)
    {
        buf[n] = (unsigned char)(x & 0xff);
        x >>= 8;
    }
    if (x != 0)
    {     /* data overflow - hack for ZIP64 (X Roche) */
        for (n = 0; n < nbByte; n++)
        {
            buf[n] = 0xff;
        }
    }

    if (ZWRITE64(*pzlib_filefunc_def, filestream, buf, nbByte) != (uLong)nbByte)
        return ZIP_ERRNO;
    else
        return ZIP_OK;
}

local void zip64local_putValue_inmemory OF((void* dest, ZPOS64_T x, int nbByte));
local void zip64local_putValue_inmemory (void* dest, ZPOS64_T x, int nbByte)
{
    unsigned char* buf = (unsigned char*)dest;
    int n;
    for (n = 0; n < nbByte; n++)
	{
        buf[n] = (unsigned char)(x & 0xff);
        x >>= 8;
    }

    if (x != 0)
    {     /* data overflow - hack for ZIP64 */
        for (n = 0; n < nbByte; n++)
        {
            buf[n] = 0xff;
        }
    }
}

/****************************************************************************/


local uLong zip64local_TmzDateToDosDate(const tm_zip* ptm)
{
    uLong year = (uLong)ptm->tm_year;
#define zip64local_in_range(min, max, value) ((min) <= (value) && (value) <= (max))
    /* Years supported:
       * [00, 79] (assumed to be between 2000 and 2079)
       * [80, 207] (assumed to be between 1980 and 2107, typical output of old
         software that does 'year-1900' to get a double digit year)
       * [1980, 2107]
       Due to the date format limitations, only years between 1980 and 2107 can be stored.
    */
    if (!(zip64local_in_range(1980, 2107, ptm->tm_year) || zip64local_in_range(0, 207, ptm->tm_year)) ||
        !zip64local_in_range(0, 11, ptm->tm_mon) ||
        !zip64local_in_range(1, 31, ptm->tm_mday) ||
        !zip64local_in_range(0, 23, ptm->tm_hour) ||
        !zip64local_in_range(0, 59, ptm->tm_min) ||
        !zip64local_in_range(0, 59, ptm->tm_sec))
      return 0;
#undef zip64local_in_range

	if (year >= 1980) /* range [1980, 2107] */
        year -= 1980;
    else if (year >= 80) /* range [80, 99] */
        year -= 80;
    else /* range [00, 79] */
        year += 20;

	return
      (uLong) (((ptm->tm_mday) + (32 * (ptm->tm_mon+1)) + (512 * year)) << 16) |
        ((ptm->tm_sec/2) + (32* ptm->tm_min) + (2048 * (uLong)ptm->tm_hour));
}


/****************************************************************************/

local int zip64local_getByte OF((const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, int *pi));
local int zip64local_getByte(const zlib_filefunc64_32_def* pzlib_filefunc_def,voidpf filestream,int* pi)
{
    unsigned char c;
    int err = (int)ZREAD64(*pzlib_filefunc_def, filestream, &c,1);
    if (err == 1)
    {
        *pi = (int)c;
        return ZIP_OK;
    }
    else
    {
        if (ZERROR64(*pzlib_filefunc_def,filestream))
            return ZIP_ERRNO;
        else
            return ZIP_EOF;
    }
}


/* ===========================================================================
   Reads a long in LSB order from the given gz_stream. Sets
*/
local int zip64local_getShort OF((const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, uLong *pX));
local int zip64local_getShort (const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, uLong* pX)
{
    uLong x ;
    int i = 0;
    int err;

    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x = (uLong)i;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += ((uLong)i)<<8;

    if (err == ZIP_OK)
        *pX = x;
    else
        *pX = 0;
    return err;
}

local int zip64local_getLong OF((const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, uLong *pX));
local int zip64local_getLong (const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, uLong* pX)
{
    uLong x ;
    int i = 0;
    int err;

    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x = (uLong)i;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += ((uLong)i) << 8;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def,  filestream, &i);
    x += ((uLong)i) << 16;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += ((uLong)i) << 24;

    if (err == ZIP_OK)
        *pX = x;
    else
        *pX = 0;
    return err;
}

local int zip64local_getLong64 OF((const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, ZPOS64_T *pX));
local int zip64local_getLong64 (const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, ZPOS64_T *pX)
{
  ZPOS64_T x;
  int i = 0;
  int err;

  err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
  x = (ZPOS64_T)i;

  if (err == ZIP_OK)
    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
  x += ((ZPOS64_T)i) << 8;

  if (err == ZIP_OK)
    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
  x += ((ZPOS64_T)i) << 16;

  if (err == ZIP_OK)
    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
  x += ((ZPOS64_T)i) << 24;

  if (err == ZIP_OK)
    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
  x += ((ZPOS64_T)i) << 32;

  if (err == ZIP_OK)
    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
  x += ((ZPOS64_T)i) << 40;

  if (err == ZIP_OK)
    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
  x += ((ZPOS64_T)i) << 48;

  if (err == ZIP_OK)
    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
  x += ((ZPOS64_T)i) << 56;

  if (err == ZIP_OK)
    *pX = x;
  else
    *pX = 0;

  return err;
}

/* Gets the amount of bytes left to write to the current disk for spanning archives */
local int zipGetDiskSizeAvailable OF((zipFile file, ZPOS64_T *size_available));
local int zipGetDiskSizeAvailable(zipFile file, ZPOS64_T *size_available)
{
    zip64_internal* zi;
    ZPOS64_T current_disk_size;

    zi = (zip64_internal*)file;
    ZSEEK64(zi->z_filefunc, zi->filestream, 0, ZLIB_FILEFUNC_SEEK_END);
    current_disk_size = ZTELL64(zi->z_filefunc, zi->filestream);
    *size_available = zi->disk_size - current_disk_size;
    return ZIP_OK;
}

/* Goes to a specific disk number for spanning archives */
local int zipGoToSpecificDisk OF((zipFile file, int number_disk, int open_existing));
local int zipGoToSpecificDisk(zipFile file, int number_disk, int open_existing)
{
    zip64_internal* zi;
    int err = ZIP_OK;

    zi = (zip64_internal*)file;
    if (zi->disk_size == 0)
        return err;

    if ((zi->filestream != NULL) && (zi->filestream != zi->filestream_with_CD))
        ZCLOSE64(zi->z_filefunc, zi->filestream, NULL);

    zi->filestream = ZOPENDISK64(zi->z_filefunc, zi->filestream_with_CD, number_disk, (open_existing == 1) ?
            (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_WRITE | ZLIB_FILEFUNC_MODE_EXISTING) :
            (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_WRITE | ZLIB_FILEFUNC_MODE_CREATE));

    if (zi->filestream == NULL)
        err = ZIP_ERRNO;

    return err;
}

/* Goes to the first disk in a spanned archive */
local int zipGoToFirstDisk OF((zipFile file));
local int zipGoToFirstDisk(zipFile file)
{
    zip64_internal* zi;
    int number_disk_next;
    int err = ZIP_OK;

    zi = (zip64_internal*)file;

    if (zi->disk_size == 0)
        return err;
    number_disk_next = 0;
    if (zi->number_disk_with_CD > 0)
        number_disk_next = zi->number_disk_with_CD - 1;
    err = zipGoToSpecificDisk(file, number_disk_next, (zi->append == APPEND_STATUS_ADDINZIP));
    if ((err == ZIP_ERRNO) && (zi->append == APPEND_STATUS_ADDINZIP))
        err = zipGoToSpecificDisk(file, number_disk_next, 0);
    if (err == ZIP_OK)
        zi->number_disk = number_disk_next;
    ZSEEK64(zi->z_filefunc, zi->filestream, 0, ZLIB_FILEFUNC_SEEK_END);
    return err;
}

/* Goes to the next disk in a spanned archive */
local int zipGoToNextDisk OF((zipFile file));
local int zipGoToNextDisk(zipFile file)
{
    zip64_internal* zi;
    ZPOS64_T size_available_in_disk;
    int err = ZIP_OK;
    int number_disk_next;

    zi = (zip64_internal*)file;

    if (zi->disk_size == 0)
        return err;

    number_disk_next = zi->number_disk + 1;

    do
        {
        err = zipGoToSpecificDisk(file, number_disk_next, (zi->append == APPEND_STATUS_ADDINZIP));
        if ((err == ZIP_ERRNO) && (zi->append == APPEND_STATUS_ADDINZIP))
            err = zipGoToSpecificDisk(file, number_disk_next, 0);
        if (err != ZIP_OK)
            break;
        err = zipGetDiskSizeAvailable(file, &size_available_in_disk);
        if (err != ZIP_OK)
            break;
        zi->number_disk = number_disk_next;
        zi->number_disk_with_CD = zi->number_disk + 1;

        number_disk_next += 1;
        }
    while (size_available_in_disk <= 0);

    return err;
}

#ifndef BUFREADCOMMENT
#define BUFREADCOMMENT (0x400)
#endif
/*
  Locate the Central directory of a zipfile (at the end, just before
    the global comment)
*/
local int zip64local_SearchCentralDir OF((const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, ZPOS64_T *uPosFound));
local int zip64local_SearchCentralDir(const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, ZPOS64_T *uPosFound)
{
  unsigned char* buf;
  ZPOS64_T uSizeFile;
  ZPOS64_T uBackRead;
  ZPOS64_T uMaxBack = 0xffff + 22; /* maximum size of global comment + End of central directory record */

  *uPosFound = 0;
  if (ZSEEK64(*pzlib_filefunc_def, filestream, 0, ZLIB_FILEFUNC_SEEK_END) != 0)
    return ZIP_ERRNO;

  uSizeFile = ZTELL64(*pzlib_filefunc_def, filestream);

  if (uMaxBack > uSizeFile)
    uMaxBack = uSizeFile;

  buf = (unsigned char*)ALLOC(BUFREADCOMMENT + 4);
  if (buf == NULL)
    return ZIP_ERRNO;

  uBackRead = 4;
  while (uBackRead < uMaxBack)
  {
    uLong uReadSize;
    ZPOS64_T uReadPos ;
    int i;
    if (uBackRead + BUFREADCOMMENT > uMaxBack)
      uBackRead = uMaxBack;
    else
      uBackRead += BUFREADCOMMENT;
    uReadPos = uSizeFile - uBackRead ;

    uReadSize = ((BUFREADCOMMENT + 4) < (uSizeFile - uReadPos)) ? (BUFREADCOMMENT + 4) : (uLong)(uSizeFile - uReadPos);
    if (ZSEEK64(*pzlib_filefunc_def, filestream, uReadPos, ZLIB_FILEFUNC_SEEK_SET) != 0)
      break;

    if (ZREAD64(*pzlib_filefunc_def, filestream, buf, uReadSize) != uReadSize)
      break;

    for (i = (int)uReadSize - 3; (i--) > 0;)
      if (((*(buf + i)) == 0x50) && ((*(buf + i + 1)) == 0x4b) &&
        ((*(buf + i + 2)) == 0x05) && ((*(buf + i + 3)) == 0x06))
      {
        *uPosFound = uReadPos + i;
        break;
      }

      if (*uPosFound != 0)
        break;
  }
  TRYFREE(buf);
  return ZIP_OK;
}

/*
Locate the End of Zip64 Central directory locator and from there find the CD of a zipfile (at the end, just before
the global comment)
*/
local int zip64local_SearchCentralDir64 OF((const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, ZPOS64_T *uPosFound));
local int zip64local_SearchCentralDir64(const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, ZPOS64_T *uPosFound)
{
  unsigned char* buf;
  ZPOS64_T uSizeFile;
  ZPOS64_T uBackRead;
  ZPOS64_T uMaxBack = 0xffff + 22 + 20; /* maximum size of global comment + End of central directory record + Zip64 end of central directory locator */
  ZPOS64_T relativeOffset = 0;
  uLong uL;

  *uPosFound = 0;

  if (ZSEEK64(*pzlib_filefunc_def, filestream, 0, ZLIB_FILEFUNC_SEEK_END) != 0)
    return ZIP_ERRNO;

  uSizeFile = ZTELL64(*pzlib_filefunc_def, filestream);

  if (uMaxBack > uSizeFile)
    uMaxBack = uSizeFile;

  buf = (unsigned char*)ALLOC(BUFREADCOMMENT + 4);
  if (buf == NULL)
    return ZIP_ERRNO;

  uBackRead = 4;
  while (uBackRead < uMaxBack)
  {
    uLong uReadSize;
    ZPOS64_T uReadPos;
    int i;
    if (uBackRead + BUFREADCOMMENT > uMaxBack)
      uBackRead = uMaxBack;
    else
      uBackRead += BUFREADCOMMENT;
    uReadPos = uSizeFile - uBackRead ;

    uReadSize = ((BUFREADCOMMENT + 4) < (uSizeFile - uReadPos)) ? (BUFREADCOMMENT + 4) : (uLong)(uSizeFile - uReadPos);
    if (ZSEEK64(*pzlib_filefunc_def, filestream, uReadPos, ZLIB_FILEFUNC_SEEK_SET) != 0)
      break;

    if (ZREAD64(*pzlib_filefunc_def, filestream, buf, uReadSize) != uReadSize)
      break;

    for (i = (int)uReadSize - 3; (i--) > 0;)
    {
      // Signature "0x07064b50" Zip64 end of central directory locater
      if (((*(buf + i)) == 0x50) && ((*(buf + i + 1)) == 0x4b) && ((*(buf + i + 2)) == 0x06) && ((*(buf + i + 3)) == 0x07))
      {
        relativeOffset = uReadPos + i;
        break;
      }
    }
    if (relativeOffset != 0)
      break;
  }

  TRYFREE(buf);
  if (relativeOffset == 0)
    return ZIP_ERRNO;

  /* Zip64 end of central directory locator */
  if (ZSEEK64(*pzlib_filefunc_def, filestream, relativeOffset, ZLIB_FILEFUNC_SEEK_SET) != 0)
    return ZIP_ERRNO;

  /* the signature, already checked */
  if (zip64local_getLong(pzlib_filefunc_def, filestream, &uL) != ZIP_OK)
    return ZIP_ERRNO;

  /* number of the disk with the start of the zip64 end of  central directory */
  if (zip64local_getLong(pzlib_filefunc_def, filestream, &uL) != ZIP_OK)
    return ZIP_ERRNO;

  /* relative offset of the zip64 end of central directory record */
  if (zip64local_getLong64(pzlib_filefunc_def, filestream, &relativeOffset) != ZIP_OK)
    return ZIP_ERRNO;

  /* total number of disks */
  if (zip64local_getLong(pzlib_filefunc_def, filestream, &uL) != ZIP_OK)
    return ZIP_ERRNO;

  /* Goto Zip64 end of central directory record */
  if (ZSEEK64(*pzlib_filefunc_def, filestream, relativeOffset, ZLIB_FILEFUNC_SEEK_SET) != 0)
    return ZIP_ERRNO;

  /* the signature */
  if (zip64local_getLong(pzlib_filefunc_def, filestream, &uL) != ZIP_OK)
    return ZIP_ERRNO;

  if (uL !=  ZIP64ENDHEADERMAGIC) // signature of 'Zip64 end of central directory'
    return ZIP_ERRNO;

  *uPosFound = relativeOffset;
  return ZIP_OK;
}

local  int LoadCentralDirectoryRecord(zip64_internal* pziinit)
{
  int err = ZIP_OK;
  ZPOS64_T byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/

  ZPOS64_T size_central_dir = 0;     /* size of the central directory  */
  ZPOS64_T offset_central_dir = 0;   /* offset of start of central directory */
  ZPOS64_T central_pos = 0;
  uLong uL;

  ZPOS64_T number_entry = 0;
  ZPOS64_T number_entry_CD = 0;      /* total number of entries in the central dir
                                        (same than number_entry on nospan) */
  uLong VersionMadeBy;
  uLong VersionNeeded;
  uLong size_comment = 0;

  err = zip64local_SearchCentralDir(&pziinit->z_filefunc, pziinit->filestream, &central_pos);
  if (err == ZIP_OK)
  {
	// Read End of central Directory info
    if (ZSEEK64(pziinit->z_filefunc, pziinit->filestream, central_pos, ZLIB_FILEFUNC_SEEK_SET) != 0)
      err = ZIP_ERRNO;

    /* the signature, already checked */
    if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream,&uL) != ZIP_OK)
      err = ZIP_ERRNO;

    /* number of this disk */
    if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream, &pziinit->number_disk) != ZIP_OK)
      err = ZIP_ERRNO;

    /* number of the disk with the start of the central directory */
    if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream, &pziinit->number_disk_with_CD) != ZIP_OK)
      err = ZIP_ERRNO;

    /* total number of entries in the central dir on this disk */
    if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream, &uL) != ZIP_OK)
      err = ZIP_ERRNO;
    else
      number_entry = uL;

    /* total number of entries in the central dir */
    if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream, &uL) != ZIP_OK)
      err = ZIP_ERRNO;
    else
      number_entry_CD = uL;

    if (number_entry_CD != number_entry)
      err = ZIP_BADZIPFILE;

    /* size of the central directory */
    if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream, &uL) != ZIP_OK)
      err = ZIP_ERRNO;
    else
      size_central_dir = uL;

    /* offset of start of central directory with respect to the starting disk number */
    if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream, &uL) != ZIP_OK)
      err = ZIP_ERRNO;
    else
      offset_central_dir = uL;

    /* zipfile global comment length */
    if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream, &size_comment) != ZIP_OK)
      err = ZIP_ERRNO;
  }

  if (err == ZIP_OK) 
  {
	err = zip64local_SearchCentralDir64(&pziinit->z_filefunc, pziinit->filestream, &central_pos);
    if (err = ZIP_OK)
    {
      ZPOS64_T sizeEndOfCentralDirectory;
      if (ZSEEK64(pziinit->z_filefunc, pziinit->filestream, central_pos, ZLIB_FILEFUNC_SEEK_SET) != 0)
        err = ZIP_ERRNO;

      /* the signature, already checked */
      if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream, &uL) != ZIP_OK)
        err = ZIP_ERRNO;

      /* size of zip64 end of central directory record */
      if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream, &sizeEndOfCentralDirectory) != ZIP_OK)
        err = ZIP_ERRNO;

      /* version made by */
      if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream, &VersionMadeBy) != ZIP_OK)
        err = ZIP_ERRNO;

      /* version needed to extract */
      if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream, &VersionNeeded) != ZIP_OK)
        err = ZIP_ERRNO;

      /* number of this disk */
      if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream, &pziinit->number_disk) != ZIP_OK)
        err = ZIP_ERRNO;

      /* number of the disk with the start of the central directory */
      if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream, &pziinit->number_disk_with_CD) != ZIP_OK)
        err = ZIP_ERRNO;

      /* total number of entries in the central directory on this disk */
      if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream, &number_entry) != ZIP_OK)
        err = ZIP_ERRNO;

      /* total number of entries in the central directory */
      if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream, &number_entry_CD) != ZIP_OK)
        err = ZIP_ERRNO;

      if (number_entry_CD != number_entry)
        err = ZIP_BADZIPFILE;

      /* size of the central directory */
      if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream, &size_central_dir) != ZIP_OK)
        err = ZIP_ERRNO;

      /* offset of start of central directory with respect to the starting disk number */
      if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream, &offset_central_dir) != ZIP_OK)
        err = ZIP_ERRNO;
    }
	else if ((number_entry_CD == 0xffff) || (offset_central_dir == 0xffffffff))
	{
	  err = ZIP_BADZIPFILE;
	}
  }

  if ((err == ZIP_OK) && (central_pos < offset_central_dir + size_central_dir))
    err = ZIP_BADZIPFILE;

  if (err != ZIP_OK)
  {
    ZCLOSE64(pziinit->z_filefunc, pziinit->filestream, NULL);
    return ZIP_ERRNO;
  }

  if (size_comment > 0)
  {
    pziinit->globalcomment = (char*)ALLOC(size_comment + 1);
    if (pziinit->globalcomment)
    {
      size_comment = ZREAD64(pziinit->z_filefunc, pziinit->filestream, pziinit->globalcomment, size_comment);
      pziinit->globalcomment[size_comment] = 0;
    }
  }

  byte_before_the_zipfile = central_pos - (offset_central_dir+size_central_dir);
  pziinit->add_position_when_writting_offset = byte_before_the_zipfile;

  {
    ZPOS64_T size_central_dir_to_read = size_central_dir;
    size_t buf_size = SIZEDATA_INDATABLOCK;
    void* buf_read = (void*)ALLOC(buf_size);
    if (ZSEEK64(pziinit->z_filefunc, pziinit->filestream, offset_central_dir + byte_before_the_zipfile, ZLIB_FILEFUNC_SEEK_SET) != 0)
      err = ZIP_ERRNO;

    while ((size_central_dir_to_read > 0) && (err == ZIP_OK))
    {
      ZPOS64_T read_this = SIZEDATA_INDATABLOCK;
      if (read_this > size_central_dir_to_read)
        read_this = size_central_dir_to_read;

      if (ZREAD64(pziinit->z_filefunc, pziinit->filestream, buf_read, (uLong)read_this) != read_this)
        err = ZIP_ERRNO;

      if (err == ZIP_OK)
        err = add_data_in_datablock(&pziinit->central_dir, buf_read, (uLong)read_this);

      size_central_dir_to_read -= read_this;
    }
    TRYFREE(buf_read);
  }
  pziinit->begin_pos = byte_before_the_zipfile;
  pziinit->number_entry = number_entry_CD;

  if (ZSEEK64(pziinit->z_filefunc, pziinit->filestream, offset_central_dir + byte_before_the_zipfile, ZLIB_FILEFUNC_SEEK_SET) != 0)
    err = ZIP_ERRNO;

  return err;
}
#endif /* !NO_ADDFILEINEXISTINGZIP*/


/************************************************************/
ZEXTERN zipFile ZEXPORT zipOpen4 (const void *pathname, int append, ZPOS64_T disk_size, zipcharpc* globalcomment, zlib_filefunc64_32_def* pzlib_filefunc64_32_def)
{
    zip64_internal ziinit;
    zip64_internal* zi;
    int mode, err = ZIP_OK;

    ziinit.z_filefunc.zseek32_file = NULL;
    ziinit.z_filefunc.ztell32_file = NULL;
    if (pzlib_filefunc64_32_def==NULL)
        fill_fopen64_filefunc(&ziinit.z_filefunc.zfile_func64);
    else
        ziinit.z_filefunc = *pzlib_filefunc64_32_def;

    if (append == APPEND_STATUS_CREATE)
        mode = (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_WRITE | ZLIB_FILEFUNC_MODE_CREATE);
    else
        mode = (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_WRITE | ZLIB_FILEFUNC_MODE_EXISTING);

	ziinit.filestream = ZOPEN64(ziinit.z_filefunc, pathname, mode);

    if (ziinit.filestream == NULL)
        return NULL;

	if (append == APPEND_STATUS_CREATEAFTER)
	{
        /* Don't support spanning ZIP with APPEND_STATUS_CREATEAFTER */
        if (disk_size > 0)
            return NULL;

        ZSEEK64(ziinit.z_filefunc,ziinit.filestream,0,SEEK_END);
	}

    ziinit.filestream_with_CD = ziinit.filestream;
    ziinit.append = append;
    ziinit.number_disk = 0;
    ziinit.number_disk_with_CD = 0;
    ziinit.disk_size = disk_size;
	ziinit.begin_pos = ZTELL64(ziinit.z_filefunc,ziinit.filestream);
    ziinit.in_opened_file_inzip = 0;
    ziinit.ci.stream_initialised = 0;
    ziinit.number_entry = 0;
    ziinit.add_position_when_writting_offset = 0;
    init_linkedlist(&(ziinit.central_dir));

    zi = (zip64_internal*)ALLOC(sizeof(zip64_internal));
    if (zi == NULL)
    {
        ZCLOSE64(ziinit.z_filefunc, ziinit.filestream, NULL);
        return NULL;
    }

    /* now we add file in a zipfile */
#ifndef NO_ADDFILEINEXISTINGZIP
    ziinit.globalcomment = NULL;
    if (append == APPEND_STATUS_ADDINZIP)
    {
      // Read and Cache Central Directory Records
      err = LoadCentralDirectoryRecord(&ziinit);
    }

    if (globalcomment)
       *globalcomment = ziinit.globalcomment;
#endif /* !NO_ADDFILEINEXISTINGZIP*/

    if (err != ZIP_OK)
    {
#ifndef NO_ADDFILEINEXISTINGZIP
        TRYFREE(ziinit.globalcomment);
#endif /* !NO_ADDFILEINEXISTINGZIP*/
        TRYFREE(zi);
        return NULL;
    }
    *zi = ziinit;
    zipGoToFirstDisk((zipFile)zi);
    return (zipFile)zi;
}

ZEXTERN zipFile ZEXPORT zipOpen3(const char *pathname, int append, ZPOS64_T disk_size, zipcharpc* globalcomment, zlib_filefunc_def* pzlib_filefunc32_def)
{
    if (pzlib_filefunc32_def != NULL)
    {
        zlib_filefunc64_32_def zlib_filefunc64_32_def_fill;
        fill_zlib_filefunc64_32_def_from_filefunc32(&zlib_filefunc64_32_def_fill, pzlib_filefunc32_def);
        return zipOpen4(pathname, append, disk_size, globalcomment, &zlib_filefunc64_32_def_fill);
    }
    else
        return zipOpen4(pathname, append, disk_size, globalcomment, NULL);
}

ZEXTERN zipFile ZEXPORT zipOpen3_64 (const void *pathname, int append, ZPOS64_T disk_size, zipcharpc* globalcomment, zlib_filefunc64_def* pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
    {
        zlib_filefunc64_32_def zlib_filefunc64_32_def_fill;
        zlib_filefunc64_32_def_fill.zfile_func64 = *pzlib_filefunc_def;
        zlib_filefunc64_32_def_fill.ztell32_file = NULL;
        zlib_filefunc64_32_def_fill.zseek32_file = NULL;
        return zipOpen4(pathname, append, disk_size, globalcomment, &zlib_filefunc64_32_def_fill);
    }
    else
        return zipOpen4(pathname, append, disk_size, globalcomment, NULL);
}

ZEXTERN zipFile ZEXPORT zipOpen2(const char *pathname, int append, zipcharpc* globalcomment, zlib_filefunc_def* pzlib_filefunc32_def)
{
    if (pzlib_filefunc32_def != NULL)
    {
        zlib_filefunc64_32_def zlib_filefunc64_32_def_fill;
        fill_zlib_filefunc64_32_def_from_filefunc32(&zlib_filefunc64_32_def_fill,pzlib_filefunc32_def);
        return zipOpen4(pathname, append, 0, globalcomment, &zlib_filefunc64_32_def_fill);
    }
    else
        return zipOpen4(pathname, append, 0, globalcomment, NULL);
}

ZEXTERN zipFile ZEXPORT zipOpen2_64 (const void *pathname, int append, zipcharpc* globalcomment, zlib_filefunc64_def* pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
    {
        zlib_filefunc64_32_def zlib_filefunc64_32_def_fill;
        zlib_filefunc64_32_def_fill.zfile_func64 = *pzlib_filefunc_def;
        zlib_filefunc64_32_def_fill.ztell32_file = NULL;
        zlib_filefunc64_32_def_fill.zseek32_file = NULL;
        return zipOpen4(pathname, append, 0, globalcomment, &zlib_filefunc64_32_def_fill);
    }
    else
        return zipOpen4(pathname, append, 0, globalcomment, NULL);
}



ZEXTERN zipFile ZEXPORT zipOpen (const char* pathname, int append)
{
    return zipOpen4((const void*)pathname, append, 0, NULL, NULL);
}

ZEXTERN zipFile ZEXPORT zipOpen64 (const void* pathname, int append)
{
    return zipOpen4(pathname, append, 0, NULL, NULL);
}

local int Write_LocalFileHeader(zip64_internal* zi, const char* filename, uInt size_extrafield_local, const void* extrafield_local)
{
  /* write the local header */
  int err;
  uInt size_filename = (uInt)strlen(filename);
  uInt size_extrafield = size_extrafield_local;

  err = zip64local_putValue(&zi->z_filefunc,zi->filestream, (uLong)LOCALHEADERMAGIC, 4);

  if (err == ZIP_OK)
  {
    if (zi->ci.zip64)
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)45, 2);/* version needed to extract */
    else
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)20, 2);/* version needed to extract */
  }

  if (err == ZIP_OK)
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)zi->ci.flag, 2);

  if (err == ZIP_OK)
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)zi->ci.method, 2);

  if (err == ZIP_OK)
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)zi->ci.dosDate, 4);

  // CRC / Compressed size / Uncompressed size will be filled in later and rewritten later
  if (err == ZIP_OK)
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0, 4); /* crc 32, unknown */
  if (err == ZIP_OK)
  {
    if (zi->ci.zip64)
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0xFFFFFFFF, 4); /* compressed size, unknown */
    else
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0, 4); /* compressed size, unknown */
  }
  if (err == ZIP_OK)
  {
    if (zi->ci.zip64)
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0xFFFFFFFF, 4); /* uncompressed size, unknown */
    else
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0, 4); /* uncompressed size, unknown */
  }

  if (err == ZIP_OK)
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)size_filename, 2);

  if (err == ZIP_OK)
  {
    if (zi->ci.zip64)
    {
      size_extrafield += 20;
    }
#ifdef HAVE_AES
    if (zi->ci.method == AES_METHOD)
      size_extrafield += 11;
#endif
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)size_extrafield, 2);
  }

  if ((err == ZIP_OK) && (size_filename > 0))
  {
    if (ZWRITE64(zi->z_filefunc, zi->filestream, filename, size_filename) != size_filename)
      err = ZIP_ERRNO;
  }

  if ((err == ZIP_OK) && (size_extrafield_local > 0))
  {
    if (ZWRITE64(zi->z_filefunc, zi->filestream, extrafield_local, size_extrafield_local) != size_extrafield_local)
      err = ZIP_ERRNO;
  }


  if ((err == ZIP_OK) && (zi->ci.zip64))
  {
      // write the Zip64 extended info
      short HeaderID = 1;
      short DataSize = 16;
      ZPOS64_T CompressedSize = 0;
      ZPOS64_T UncompressedSize = 0;

      // Remember position of Zip64 extended info for the local file header. (needed when we update size after done with file)
      zi->ci.pos_zip64extrainfo = ZTELL64(zi->z_filefunc,zi->filestream);

      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (short)HeaderID, 2);
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (short)DataSize, 2);

      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (ZPOS64_T)UncompressedSize, 8);
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (ZPOS64_T)CompressedSize, 8);
  }

#ifdef HAVE_AES
  /* Write the AES extended info */
  if ((err == ZIP_OK) && (zi->ci.method == AES_METHOD))
  {
    int headerid = 0x9901;
    short datasize = 7;

    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, headerid, 2);
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, datasize, 2);

    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, AES_VERSION, 2);
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 'A', 1);
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 'E', 1);
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, AES_ENCRYPTIONMODE, 1);
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, zi->ci.compression_method, 2);
  }
#endif
  return err;
}

/*
 NOTE.
 When writing RAW the ZIP64 extended information in extrafield_local and extrafield_global needs to be stripped
 before calling this function it can be done with zipRemoveExtraInfoBlock

 It is not done here because then we need to realloc a new buffer since parameters are 'const' and I want to minimize
 unnecessary allocations.
 */
ZEXTERN int ZEXPORT zipOpenNewFileInZip4_64 (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                         const void* extrafield_local, uInt size_extrafield_local,
                                         const void* extrafield_global, uInt size_extrafield_global,
                                         const char* comment, int method, int level, int raw,
                                         int windowBits,int memLevel, int strategy,
                                         const char* password, uLong crcForCrypting,
                                         uLong versionMadeBy, uLong flagBase, int zip64)
{
    zip64_internal* zi;
    uInt size_filename;
    uInt size_comment = 0;
    uInt i;
    int err = ZIP_OK;
    ZPOS64_T size_available;
    ZPOS64_T size_needed;

#ifdef NOCRYPT
    (crcForCrypting);
    if (password != NULL)
        return ZIP_PARAMERROR;
#endif

    if (file == NULL)
        return ZIP_PARAMERROR;

#ifdef HAVE_BZIP2
    if ((method!=0) && (method!=Z_DEFLATED) && (method!=Z_BZIP2ED))
      return ZIP_PARAMERROR;
#else
    if ((method!=0) && (method!=Z_DEFLATED))
      return ZIP_PARAMERROR;
#endif

    zi = (zip64_internal*)file;

    if (zi->in_opened_file_inzip == 1)
    {
        err = zipCloseFileInZip (file);
        if (err != ZIP_OK)
            return err;
    }

    if (filename == NULL)
        filename="-";

    if (comment != NULL)
		size_comment = (uInt)strlen(comment);

    size_filename = (uInt)strlen(filename);

    if (zipfi == NULL)
        zi->ci.dosDate = 0;
    else
    {
        if (zipfi->dosDate != 0)
            zi->ci.dosDate = zipfi->dosDate;
        else
          zi->ci.dosDate = zip64local_TmzDateToDosDate(&zipfi->tmz_date);
    }

    zi->ci.flag = flagBase;
    if ((level==8) || (level==9))
      zi->ci.flag |= 2;
    if (level==2)
      zi->ci.flag |= 4;
    if (level==1)
      zi->ci.flag |= 6;

    zi->ci.crc32 = 0;
    zi->ci.method = method;
    zi->ci.compression_method = method;
    zi->ci.stream_initialised = 0;
    zi->ci.pos_in_buffered_data = 0;
    zi->ci.raw = raw;
    zi->ci.pos_local_header = ZTELL64(zi->z_filefunc,zi->filestream);

    if (password != NULL)
    {
      zi->ci.flag |= 1;
#ifdef HAVE_AES
      zi->ci.method = AES_METHOD;
#endif
    }

    if (zi->disk_size > 0)
    {
        if ((zi->number_disk == 0) && (zi->number_entry == 0))
            err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)DISKHEADERMAGIC, 4);

        /* Make sure enough space available on current disk for local header */
        zipGetDiskSizeAvailable((zipFile)zi, &size_available);
        size_needed = 30 + size_filename + size_extrafield_local;
        if (zi->ci.zip64)
            size_needed += 20;
#ifdef HAVE_AES
        if (zi->ci.method == AES_METHOD)
            size_needed += 11;
#endif
        if (size_available < size_needed)
            zipGoToNextDisk((zipFile)zi);
    }

    zi->ci.pos_local_header = ZTELL64(zi->z_filefunc, zi->filestream);
    zi->ci.size_comment = size_comment;
	zi->ci.size_centralheader = SIZECENTRALHEADER + size_filename + size_extrafield_global + size_comment;
    zi->ci.size_centralExtra = size_extrafield_global;
    zi->ci.size_centralExtraFree = 32; // Extra space we have reserved in case we need to add ZIP64 extra info data
#ifdef HAVE_AES
    if (zi->ci.method == AES_METHOD)
        zi->ci.size_centralExtraFree += 11; /* Extra space reserved for AES extra info */
#endif

    zi->ci.central_header = (char*)ALLOC((uInt)zi->ci.size_centralheader + zi->ci.size_centralExtraFree);
    zi->ci.number_disk = zi->number_disk;

    /* Write central directory header */
    zip64local_putValue_inmemory(zi->ci.central_header, (uLong)CENTRALHEADERMAGIC, 4);
    zip64local_putValue_inmemory(zi->ci.central_header + 4, (uLong)versionMadeBy, 2);
    zip64local_putValue_inmemory(zi->ci.central_header + 6, (uLong)20, 2);
    zip64local_putValue_inmemory(zi->ci.central_header + 8, (uLong)zi->ci.flag, 2);
    zip64local_putValue_inmemory(zi->ci.central_header + 10, (uLong)zi->ci.method, 2);
    zip64local_putValue_inmemory(zi->ci.central_header + 12, (uLong)zi->ci.dosDate, 4);
    zip64local_putValue_inmemory(zi->ci.central_header + 16, (uLong)0, 4); /*crc*/
    zip64local_putValue_inmemory(zi->ci.central_header + 20, (uLong)0, 4); /*compr size*/
    zip64local_putValue_inmemory(zi->ci.central_header + 24, (uLong)0, 4); /*uncompr size*/
    zip64local_putValue_inmemory(zi->ci.central_header + 28, (uLong)size_filename, 2);
    zip64local_putValue_inmemory(zi->ci.central_header + 30, (uLong)size_extrafield_global, 2);
    zip64local_putValue_inmemory(zi->ci.central_header + 32, (uLong)size_comment, 2);
    zip64local_putValue_inmemory(zi->ci.central_header + 34, (uLong)zi->ci.number_disk, 2); /*disk nm start*/

    if (zipfi==NULL)
        zip64local_putValue_inmemory(zi->ci.central_header + 36, (uLong)0, 2);
    else
        zip64local_putValue_inmemory(zi->ci.central_header + 36, (uLong)zipfi->internal_fa, 2);

    if (zipfi==NULL)
        zip64local_putValue_inmemory(zi->ci.central_header + 38, (uLong)0, 4);
    else
        zip64local_putValue_inmemory(zi->ci.central_header + 38, (uLong)zipfi->external_fa, 4);

    if (zi->ci.pos_local_header >= 0xffffffff)
      zip64local_putValue_inmemory(zi->ci.central_header + 42, (uLong)0xffffffff, 4);
    else
      zip64local_putValue_inmemory(zi->ci.central_header + 42, (uLong)zi->ci.pos_local_header - zi->add_position_when_writting_offset, 4);

    for (i = 0; i < size_filename; i++)
        *(zi->ci.central_header + SIZECENTRALHEADER + i) = *(filename + i);

    for (i = 0; i < size_extrafield_global; i++)
        *(zi->ci.central_header + SIZECENTRALHEADER + size_filename + i) = *(((const char*)extrafield_global) + i);

    for (i = 0; i < size_comment; i++)
        *(zi->ci.central_header + SIZECENTRALHEADER + size_filename + size_extrafield_global + i) = *(comment + i);
    if (zi->ci.central_header == NULL)
        return ZIP_INTERNALERROR;

    zi->ci.zip64 = zip64;
    zi->ci.totalCompressedData = 0;
    zi->ci.totalUncompressedData = 0;
    zi->ci.pos_zip64extrainfo = 0;

    err = Write_LocalFileHeader(zi, filename, size_extrafield_local, extrafield_local);

#ifdef HAVE_BZIP2
    zi->ci.bstream.avail_in = (uInt)0;
    zi->ci.bstream.avail_out = (uInt)Z_BUFSIZE;
    zi->ci.bstream.next_out = (char*)zi->ci.buffered_data;
    zi->ci.bstream.total_in_hi32 = 0;
    zi->ci.bstream.total_in_lo32 = 0;
    zi->ci.bstream.total_out_hi32 = 0;
    zi->ci.bstream.total_out_lo32 = 0;
#endif

    zi->ci.stream.avail_in = (uInt)0;
    zi->ci.stream.avail_out = (uInt)Z_BUFSIZE;
    zi->ci.stream.next_out = zi->ci.buffered_data;
    zi->ci.stream.total_in = 0;
    zi->ci.stream.total_out = 0;
    zi->ci.stream.data_type = Z_BINARY;

    if ((err == ZIP_OK) && (!zi->ci.raw))
    {
        if(zi->ci.method == Z_DEFLATED)
        {
          zi->ci.stream.zalloc = (alloc_func)0;
          zi->ci.stream.zfree = (free_func)0;
          zi->ci.stream.opaque = (voidpf)0;

          if (windowBits > 0)
              windowBits = -windowBits;

          err = deflateInit2(&zi->ci.stream, level, Z_DEFLATED, windowBits, memLevel, strategy);

          if (err==Z_OK)
              zi->ci.stream_initialised = Z_DEFLATED;
        }
        else if(zi->ci.method == Z_BZIP2ED)
        {
#ifdef HAVE_BZIP2
          // Init BZip stuff here
          zi->ci.bstream.bzalloc = 0;
          zi->ci.bstream.bzfree = 0;
          zi->ci.bstream.opaque = (voidpf)0;

          err = BZ2_bzCompressInit(&zi->ci.bstream, level, 0,35);
          if(err == BZ_OK)
            zi->ci.stream_initialised = Z_BZIP2ED;
#endif
        }
    }

#ifndef NOCRYPT
    zi->ci.crypt_header_size = 0;
    if ((err==Z_OK) && ((zi->ci.flag & 1) != 0))
    {
#ifdef HAVE_AES
        if (zi->ci.method == AES_METHOD)
        {
            unsigned char passverify[AES_PWVERIFYSIZE];
            unsigned char saltvalue[AES_MAXSALTLENGTH];
            uInt saltlength;

            if ((AES_ENCRYPTIONMODE < 1) || (AES_ENCRYPTIONMODE > 3))
                return Z_ERRNO;

            saltlength = SALT_LENGTH(AES_ENCRYPTIONMODE);

            prng_init(entropy_fun, zi->ci.aes_rng);
            prng_rand(saltvalue, saltlength, zi->ci.aes_rng);
            prng_end(zi->ci.aes_rng);

            fcrypt_init(AES_ENCRYPTIONMODE, password, strlen(password), saltvalue, passverify, &zi->ci.aes_ctx);

            if (ZWRITE64(zi->z_filefunc, zi->filestream, saltvalue, saltlength) != saltlength)
                err = ZIP_ERRNO;
            if (ZWRITE64(zi->z_filefunc, zi->filestream, passverify, AES_PWVERIFYSIZE) != AES_PWVERIFYSIZE)
                err = ZIP_ERRNO;

            zi->ci.crypt_header_size = saltlength + AES_PWVERIFYSIZE + AES_AUTHCODESIZE;
        }
        else
#endif
        {
            unsigned char bufHead[RAND_HEAD_LEN];
            unsigned int sizeHead;
            zi->ci.pcrc_32_tab = get_crc_table();
            /*init_keys(password,zi->ci.keys,zi->ci.pcrc_32_tab);*/

            sizeHead = crypthead(password,bufHead,RAND_HEAD_LEN,zi->ci.keys,zi->ci.pcrc_32_tab,crcForCrypting);
            zi->ci.crypt_header_size = sizeHead;

            if (ZWRITE64(zi->z_filefunc,zi->filestream,bufHead,sizeHead) != sizeHead)
                err = ZIP_ERRNO;
        }
#endif
    }

    if (err==Z_OK)
        zi->in_opened_file_inzip = 1;
    return err;
}

ZEXTERN int ZEXPORT zipOpenNewFileInZip4 (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                         const void* extrafield_local, uInt size_extrafield_local,
                                         const void* extrafield_global, uInt size_extrafield_global,
                                         const char* comment, int method, int level, int raw,
                                         int windowBits,int memLevel, int strategy,
                                         const char* password, uLong crcForCrypting,
                                         uLong versionMadeBy, uLong flagBase)
{
    return zipOpenNewFileInZip4_64 (file, filename, zipfi,
                                 extrafield_local, size_extrafield_local,
                                 extrafield_global, size_extrafield_global,
                                 comment, method, level, raw,
                                 windowBits, memLevel, strategy,
                                 password, crcForCrypting, versionMadeBy, flagBase, 0);
}

ZEXTERN int ZEXPORT zipOpenNewFileInZip3 (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                         const void* extrafield_local, uInt size_extrafield_local,
                                         const void* extrafield_global, uInt size_extrafield_global,
                                         const char* comment, int method, int level, int raw,
                                         int windowBits,int memLevel, int strategy,
                                         const char* password, uLong crcForCrypting)
{
    return zipOpenNewFileInZip4_64 (file, filename, zipfi,
                                 extrafield_local, size_extrafield_local,
                                 extrafield_global, size_extrafield_global,
                                 comment, method, level, raw,
                                 windowBits, memLevel, strategy,
                                 password, crcForCrypting, VERSIONMADEBY, 0, 0);
}

ZEXTERN int ZEXPORT zipOpenNewFileInZip3_64(zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                         const void* extrafield_local, uInt size_extrafield_local,
                                         const void* extrafield_global, uInt size_extrafield_global,
                                         const char* comment, int method, int level, int raw,
                                         int windowBits,int memLevel, int strategy,
                                         const char* password, uLong crcForCrypting, int zip64)
{
    return zipOpenNewFileInZip4_64 (file, filename, zipfi,
                                 extrafield_local, size_extrafield_local,
                                 extrafield_global, size_extrafield_global,
                                 comment, method, level, raw,
                                 windowBits, memLevel, strategy,
                                 password, crcForCrypting, VERSIONMADEBY, 0, zip64);
}

ZEXTERN int ZEXPORT zipOpenNewFileInZip2(zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                        const void* extrafield_local, uInt size_extrafield_local,
                                        const void* extrafield_global, uInt size_extrafield_global,
                                        const char* comment, int method, int level, int raw)
{
    return zipOpenNewFileInZip4_64 (file, filename, zipfi,
                                 extrafield_local, size_extrafield_local,
                                 extrafield_global, size_extrafield_global,
                                 comment, method, level, raw,
                                 -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                 NULL, 0, VERSIONMADEBY, 0, 0);
}

ZEXTERN int ZEXPORT zipOpenNewFileInZip2_64(zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                        const void* extrafield_local, uInt size_extrafield_local,
                                        const void* extrafield_global, uInt size_extrafield_global,
                                        const char* comment, int method, int level, int raw, int zip64)
{
    return zipOpenNewFileInZip4_64 (file, filename, zipfi,
                                 extrafield_local, size_extrafield_local,
                                 extrafield_global, size_extrafield_global,
                                 comment, method, level, raw,
                                 -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                 NULL, 0, VERSIONMADEBY, 0, zip64);
}

ZEXTERN int ZEXPORT zipOpenNewFileInZip64 (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                        const void* extrafield_local, uInt size_extrafield_local,
                                        const void*extrafield_global, uInt size_extrafield_global,
                                        const char* comment, int method, int level, int zip64)
{
    return zipOpenNewFileInZip4_64 (file, filename, zipfi,
                                 extrafield_local, size_extrafield_local,
                                 extrafield_global, size_extrafield_global,
                                 comment, method, level, 0,
                                 -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                 NULL, 0, VERSIONMADEBY, 0, zip64);
}

ZEXTERN int ZEXPORT zipOpenNewFileInZip (zipFile file, const char* filename, const zip_fileinfo* zipfi,
                                        const void* extrafield_local, uInt size_extrafield_local,
                                        const void*extrafield_global, uInt size_extrafield_global,
                                        const char* comment, int method, int level)
{
    return zipOpenNewFileInZip4_64 (file, filename, zipfi,
                                 extrafield_local, size_extrafield_local,
                                 extrafield_global, size_extrafield_global,
                                 comment, method, level, 0,
                                 -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                 NULL, 0, VERSIONMADEBY, 0, 0);
}

local int zip64FlushWriteBuffer(zip64_internal* zi)
{
    int err = ZIP_OK;
    uInt written = 0;
    uInt total_written = 0;
    uInt write = 0;
    uInt max_write = 0;
    ZPOS64_T size_available = 0;

    if ((zi->ci.flag & 1) != 0)
    {
#ifndef NOCRYPT
#ifdef HAVE_AES
        if (zi->ci.method == AES_METHOD)
        {
            fcrypt_encrypt(zi->ci.buffered_data, zi->ci.pos_in_buffered_data, &zi->ci.aes_ctx);
        }
        else
#endif
        {
            uInt i;
            int t;
            for (i=0;i<zi->ci.pos_in_buffered_data;i++)
                zi->ci.buffered_data[i] = zencode(zi->ci.keys, zi->ci.pcrc_32_tab, zi->ci.buffered_data[i],t);
        }
#endif
    }

    write = zi->ci.pos_in_buffered_data;

    do
    {
        max_write = write;

        if (zi->disk_size > 0)
        {
            err = zipGetDiskSizeAvailable((zipFile)zi, &size_available);
            if (err != ZIP_OK)
                return err;

            if (size_available == 0)
            {
                err = zipGoToNextDisk((zipFile)zi);
                if (err != ZIP_OK)
                    return err;
            }

            if (size_available < (ZPOS64_T)max_write)
                max_write = (uInt)size_available;
        }

        written = ZWRITE64(zi->z_filefunc, zi->filestream, zi->ci.buffered_data + total_written, max_write);

        if (ZERROR64(zi->z_filefunc, zi->filestream))
        {
            err = ZIP_ERRNO;
            break;
        }

        total_written += written;
        write -= written;
    }
    while (write > 0);

    zi->ci.totalCompressedData += zi->ci.pos_in_buffered_data;

#ifdef HAVE_BZIP2
    if(zi->ci.compression_method == Z_BZIP2ED)
    {
      zi->ci.totalUncompressedData += zi->ci.bstream.total_in_lo32;
      zi->ci.bstream.total_in_lo32 = 0;
      zi->ci.bstream.total_in_hi32 = 0;
    }
    else
#endif
    {
      zi->ci.totalUncompressedData += zi->ci.stream.total_in;
      zi->ci.stream.total_in = 0;
    }

    zi->ci.pos_in_buffered_data = 0;

    return err;
}

ZEXTERN int ZEXPORT zipWriteInFileInZip (zipFile file,const void* buf,unsigned int len)
{
    zip64_internal* zi;
    int err=ZIP_OK;

    if (file == NULL)
        return ZIP_PARAMERROR;
    zi = (zip64_internal*)file;

    if (zi->in_opened_file_inzip == 0)
        return ZIP_PARAMERROR;

    zi->ci.crc32 = crc32(zi->ci.crc32,buf,(uInt)len);

#ifdef HAVE_BZIP2
    if(zi->ci.compression_method == Z_BZIP2ED && (!zi->ci.raw))
    {
      zi->ci.bstream.next_in = (void*)buf;
      zi->ci.bstream.avail_in = len;
      err = BZ_RUN_OK;

      while ((err==BZ_RUN_OK) && (zi->ci.bstream.avail_in>0))
      {
        if (zi->ci.bstream.avail_out == 0)
        {
          if (zip64FlushWriteBuffer(zi) == ZIP_ERRNO)
            err = ZIP_ERRNO;
          zi->ci.bstream.avail_out = (uInt)Z_BUFSIZE;
          zi->ci.bstream.next_out = (char*)zi->ci.buffered_data;
        }


        if(err != BZ_RUN_OK)
          break;

        if ((zi->ci.method == Z_BZIP2ED) && (!zi->ci.raw))
        {
          uLong uTotalOutBefore_lo = zi->ci.bstream.total_out_lo32;
//          uLong uTotalOutBefore_hi = zi->ci.bstream.total_out_hi32;
          err=BZ2_bzCompress(&zi->ci.bstream,  BZ_RUN);

          zi->ci.pos_in_buffered_data += (uInt)(zi->ci.bstream.total_out_lo32 - uTotalOutBefore_lo) ;
        }
      }

      if(err == BZ_RUN_OK)
        err = ZIP_OK;
    }
    else
#endif
    {
      zi->ci.stream.next_in = (Bytef*)buf;
      zi->ci.stream.avail_in = len;

      while ((err == ZIP_OK) && (zi->ci.stream.avail_in > 0))
      {
          if (zi->ci.stream.avail_out == 0)
          {
              if (zip64FlushWriteBuffer(zi) == ZIP_ERRNO)
                  err = ZIP_ERRNO;
              zi->ci.stream.avail_out = (uInt)Z_BUFSIZE;
              zi->ci.stream.next_out = zi->ci.buffered_data;
          }


          if (err != ZIP_OK)
              break;

          if ((zi->ci.compression_method == Z_DEFLATED) && (!zi->ci.raw))
          {
              uLong uTotalOutBefore = zi->ci.stream.total_out;
              err = deflate(&zi->ci.stream,  Z_NO_FLUSH);
              if (uTotalOutBefore > zi->ci.stream.total_out)
              {
                int bBreak = 0;
                bBreak++;
              }

              zi->ci.pos_in_buffered_data += (uInt)(zi->ci.stream.total_out - uTotalOutBefore) ;
          }
          else
          {
              uInt copy_this,i;
              if (zi->ci.stream.avail_in < zi->ci.stream.avail_out)
                  copy_this = zi->ci.stream.avail_in;
              else
                  copy_this = zi->ci.stream.avail_out;

              for (i = 0; i < copy_this; i++)
                  *(((char*)zi->ci.stream.next_out)+i) =
                      *(((const char*)zi->ci.stream.next_in)+i);
              {
                  zi->ci.stream.avail_in -= copy_this;
                  zi->ci.stream.avail_out-= copy_this;
                  zi->ci.stream.next_in+= copy_this;
                  zi->ci.stream.next_out+= copy_this;
                  zi->ci.stream.total_in+= copy_this;
                  zi->ci.stream.total_out+= copy_this;
                  zi->ci.pos_in_buffered_data += copy_this;
              }
          }
      }// while(...)
    }

    return err;
}

ZEXTERN int ZEXPORT zipCloseFileInZipRaw (zipFile file, uLong uncompressed_size, uLong crc32)
{
    return zipCloseFileInZipRaw64 (file, uncompressed_size, crc32);
}

ZEXTERN int ZEXPORT zipCloseFileInZipRaw64 (zipFile file, ZPOS64_T uncompressed_size, uLong crc32)
{
    zip64_internal* zi;
    ZPOS64_T compressed_size;
    uLong i, invalidValue = 0xffffffff;
    short datasize = 0;
    int err = ZIP_OK;

    if (file == NULL)
        return ZIP_PARAMERROR;
    zi = (zip64_internal*)file;

    if (zi->in_opened_file_inzip == 0)
        return ZIP_PARAMERROR;
    zi->ci.stream.avail_in = 0;

	if (!zi->ci.raw)
	{
        if (zi->ci.compression_method == Z_DEFLATED)
        {
            while (err==ZIP_OK)
            {
                uLong uTotalOutBefore;
                if (zi->ci.stream.avail_out == 0)
                {
                    if (zip64FlushWriteBuffer(zi) == ZIP_ERRNO)
                        err = ZIP_ERRNO;
                    zi->ci.stream.avail_out = (uInt)Z_BUFSIZE;
                    zi->ci.stream.next_out = zi->ci.buffered_data;
                }
                uTotalOutBefore = zi->ci.stream.total_out;
                err = deflate(&zi->ci.stream,  Z_FINISH);
                zi->ci.pos_in_buffered_data += (uInt)(zi->ci.stream.total_out - uTotalOutBefore) ;
            }
        }
        else if (zi->ci.compression_method == Z_BZIP2ED)
        {
#ifdef HAVE_BZIP2
            err = BZ_FINISH_OK;
            while (err == BZ_FINISH_OK)
            {
                uLong uTotalOutBefore;
                if (zi->ci.bstream.avail_out == 0)
                {
                    if (zip64FlushWriteBuffer(zi) == ZIP_ERRNO)
                        err = ZIP_ERRNO;
                    zi->ci.bstream.avail_out = (uInt)Z_BUFSIZE;
                    zi->ci.bstream.next_out = (char*)zi->ci.buffered_data;
                }
                uTotalOutBefore = zi->ci.bstream.total_out_lo32;
                err = BZ2_bzCompress(&zi->ci.bstream,  BZ_FINISH);
                if (err == BZ_STREAM_END)
                    err = Z_STREAM_END;

                zi->ci.pos_in_buffered_data += (uInt)(zi->ci.bstream.total_out_lo32 - uTotalOutBefore);
            }

            if (err == BZ_FINISH_OK)
                err = ZIP_OK;
#endif
        }
	}

    if (err == Z_STREAM_END)
        err=ZIP_OK; /* this is normal */

    if ((zi->ci.pos_in_buffered_data > 0) && (err == ZIP_OK))
    {
        if (zip64FlushWriteBuffer(zi) == ZIP_ERRNO)
            err = ZIP_ERRNO;
    }

#ifdef HAVE_AES
    if (zi->ci.method == AES_METHOD)
    {
        unsigned char authcode[AES_AUTHCODESIZE];

        fcrypt_end(authcode, &zi->ci.aes_ctx);

        if (ZWRITE64(zi->z_filefunc, zi->filestream, authcode, AES_AUTHCODESIZE) != AES_AUTHCODESIZE)
            err = ZIP_ERRNO;
    }
#endif
    if (!zi->ci.raw)
    {
        if ((zi->ci.compression_method == Z_DEFLATED))
        {
            int tmp_err = deflateEnd(&zi->ci.stream);
            if (err == ZIP_OK)
                err = tmp_err;
            zi->ci.stream_initialised = 0;
        }
#ifdef HAVE_BZIP2
        else if((zi->ci.compression_method == Z_BZIP2ED))
        {
            int tmperr = BZ2_bzCompressEnd(&zi->ci.bstream);
            if (err==ZIP_OK)
                err = tmperr;
            zi->ci.stream_initialised = 0;
        }
#endif
        crc32 = (uLong)zi->ci.crc32;
        uncompressed_size = zi->ci.totalUncompressedData;
    }
    compressed_size = zi->ci.totalCompressedData;

#ifndef NOCRYPT
    compressed_size += zi->ci.crypt_header_size;
#endif

    // update Current Item crc and sizes,
    if(compressed_size >= 0xffffffff || uncompressed_size >= 0xffffffff || zi->ci.pos_local_header >= 0xffffffff)
    {
      /*version Made by*/
      zip64local_putValue_inmemory(zi->ci.central_header + 4, (uLong)45, 2);
      /*version needed*/
      zip64local_putValue_inmemory(zi->ci.central_header + 6, (uLong)45, 2);
    }
    zip64local_putValue_inmemory(zi->ci.central_header + 16, crc32, 4); /*crc*/

    if (compressed_size >= 0xffffffff)
      zip64local_putValue_inmemory(zi->ci.central_header + 20, invalidValue,4); /*compr size*/
    else
      zip64local_putValue_inmemory(zi->ci.central_header + 20, compressed_size,4); /*compr size*/

    /// set internal file attributes field
    if (zi->ci.stream.data_type == Z_ASCII)
        zip64local_putValue_inmemory(zi->ci.central_header +36, (uLong)Z_ASCII, 2);

    if (uncompressed_size >= 0xffffffff)
      zip64local_putValue_inmemory(zi->ci.central_header + 24, invalidValue, 4); /*uncompr size*/
    else
      zip64local_putValue_inmemory(zi->ci.central_header + 24, uncompressed_size, 4); /*uncompr size*/

    // Add ZIP64 extra info field for uncompressed size
    if (uncompressed_size >= 0xffffffff)
      datasize += 8;

    // Add ZIP64 extra info field for compressed size
    if (compressed_size >= 0xffffffff)
      datasize += 8;

    // Add ZIP64 extra info field for relative offset to local file header of current file
    if (zi->ci.pos_local_header >= 0xffffffff)
      datasize += 8;

    if (datasize > 0)
    {
      char* p = zi->ci.central_header + zi->ci.size_centralheader;

      if ((uLong)(datasize + 4) > zi->ci.size_centralExtraFree)
        // we can not write more data to the buffer that we have room for.
        return ZIP_BADZIPFILE;

      // Add Extra Information Header for 'ZIP64 information'
      zip64local_putValue_inmemory(p, 0x0001, 2); // HeaderID
      p += 2;
      zip64local_putValue_inmemory(p, datasize, 2); // DataSize
      p += 2;

      if (uncompressed_size >= 0xffffffff)
      {
        zip64local_putValue_inmemory(p, uncompressed_size, 8);
        p += 8;
      }

      if (compressed_size >= 0xffffffff)
      {
        zip64local_putValue_inmemory(p, compressed_size, 8);
        p += 8;
      }

      if (zi->ci.pos_local_header >= 0xffffffff)
      {
        zip64local_putValue_inmemory(p, zi->ci.pos_local_header, 8);
        p += 8;
      }

      // Update how much extra free space we got in the memory buffer
      // and increase the centralheader size so the new ZIP64 fields are included
      // ( 4 below is the size of HeaderID and DataSize field )
      zi->ci.size_centralExtraFree -= datasize + 4;
      zi->ci.size_centralheader += datasize + 4;

      // Update the extra info size field
      zi->ci.size_centralExtra += datasize + 4;
      zip64local_putValue_inmemory(zi->ci.central_header + 30, (uLong)zi->ci.size_centralExtra, 2);
    }

#ifdef HAVE_AES
    /* Write the AES extended info */
    if (zi->ci.method == AES_METHOD)
    {
        char* p = zi->ci.central_header + zi->ci.size_centralheader;

        datasize = 7;

        if ((uLong)(datasize + 4) > zi->ci.size_centralExtraFree)
            return ZIP_BADZIPFILE;

        zip64local_putValue_inmemory(p, 0x9901, 2);
        p += 2;
        zip64local_putValue_inmemory(p, datasize, 2);
        p += 2;
        zip64local_putValue_inmemory(p, AES_VERSION, 2);
        p += 2;
        zip64local_putValue_inmemory(p, 'A', 1);
        p += 1;
        zip64local_putValue_inmemory(p, 'E', 1);
        p += 1;
        zip64local_putValue_inmemory(p, AES_ENCRYPTIONMODE, 1);
        p += 1;
        zip64local_putValue_inmemory(p, zi->ci.compression_method, 2);
        p += 2;

        zi->ci.size_centralExtraFree -= datasize + 4;
        zi->ci.size_centralheader += datasize + 4;
        zi->ci.size_centralExtra += datasize + 4;

        zip64local_putValue_inmemory(zi->ci.central_header + 30, (uLong)zi->ci.size_centralExtra, 2);
    }
#endif
    /* Restore comment to correct position */
    for (i = 0; i < zi->ci.size_comment; i++)
        zi->ci.central_header[zi->ci.size_centralheader + i] = zi->ci.central_header[zi->ci.size_centralheader + zi->ci.size_centralExtraFree + i];
    zi->ci.size_centralheader += zi->ci.size_comment;

    if (err == ZIP_OK)
        err = add_data_in_datablock(&zi->central_dir, zi->ci.central_header, (uLong)zi->ci.size_centralheader);

    free(zi->ci.central_header);

    if (err == ZIP_OK)
    {
        // Update the LocalFileHeader with the new values.

        ZPOS64_T cur_pos_inzip = ZTELL64(zi->z_filefunc,zi->filestream);
        uLong cur_number_disk = zi->number_disk;

        /* Local file header is stored on previous disk, switch to make edits */
        if (zi->ci.number_disk != cur_number_disk)
            err = zipGoToSpecificDisk(file, zi->ci.number_disk, 1);

		if (ZSEEK64(zi->z_filefunc, zi->filestream, zi->ci.pos_local_header + 14, ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;

        if (err == ZIP_OK)
            err = zip64local_putValue(&zi->z_filefunc, zi->filestream, crc32, 4); /* crc 32, unknown */

        if (uncompressed_size >= 0xffffffff || compressed_size >= 0xffffffff)
        {
          if (zi->ci.pos_zip64extrainfo > 0)
          {
            // Update the size in the ZIP64 extended field.
            if (ZSEEK64(zi->z_filefunc,zi->filestream, zi->ci.pos_zip64extrainfo + 4, ZLIB_FILEFUNC_SEEK_SET) != 0)
              err = ZIP_ERRNO;

            if (err == ZIP_OK) /* compressed size, unknown */
              err = zip64local_putValue(&zi->z_filefunc, zi->filestream, uncompressed_size, 8);

            if (err == ZIP_OK) /* uncompressed size, unknown */
              err = zip64local_putValue(&zi->z_filefunc, zi->filestream, compressed_size, 8);
          }
          else
            err = ZIP_BADZIPFILE; // Caller passed zip64 = 0, so no room for zip64 info -> fatal
        }
        else
        {
          if (err == ZIP_OK) /* compressed size, unknown */
              err = zip64local_putValue(&zi->z_filefunc, zi->filestream, compressed_size, 4);

          if (err == ZIP_OK) /* uncompressed size, unknown */
              err = zip64local_putValue(&zi->z_filefunc, zi->filestream, uncompressed_size, 4);
        }

        if (ZSEEK64(zi->z_filefunc,zi->filestream, cur_pos_inzip, ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;
    }

    zi->number_entry ++;
    zi->in_opened_file_inzip = 0;

    return err;
}

ZEXTERN int ZEXPORT zipCloseFileInZip (zipFile file)
{
    return zipCloseFileInZipRaw (file,0,0);
}

local int Write_Zip64EndOfCentralDirectoryLocator(zip64_internal* zi, ZPOS64_T zip64eocd_pos_inzip)
{
  int err = ZIP_OK;
  ZPOS64_T pos = zip64eocd_pos_inzip - zi->add_position_when_writting_offset;

  err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)ZIP64ENDLOCHEADERMAGIC, 4);

  /*num disks*/
    if (err == ZIP_OK) /* number of the disk with the start of the central directory */
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0, 4);

  /*relative offset*/
    if (err == ZIP_OK) /* Relative offset to the Zip64EndOfCentralDirectory */
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, pos, 8);

  /*total disks*/ /* Do not support spawning of disk so always say 1 here*/
    if (err == ZIP_OK) /* number of the disk with the start of the central directory */
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)1, 4);

    return err;
}

local int Write_Zip64EndOfCentralDirectoryRecord(zip64_internal* zi, uLong size_centraldir, ZPOS64_T centraldir_pos_inzip)
{
  int err = ZIP_OK;

  uLong Zip64DataSize = 44;

  err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)ZIP64ENDHEADERMAGIC, 4);

  if (err == ZIP_OK) /* size of this 'zip64 end of central directory' */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (ZPOS64_T)Zip64DataSize, 8); // why ZPOS64_T of this ?

  if (err == ZIP_OK) /* version made by */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)45,2);

  if (err == ZIP_OK) /* version needed */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)45,2);

  if (err == ZIP_OK) /* number of this disk */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0, 4);

  if (err == ZIP_OK) /* number of the disk with the start of the central directory */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0, 4);

  if (err == ZIP_OK) /* total number of entries in the central dir on this disk */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, zi->number_entry, 8);

  if (err == ZIP_OK) /* total number of entries in the central dir */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, zi->number_entry, 8);

  if (err == ZIP_OK) /* size of the central directory */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (ZPOS64_T)size_centraldir, 8);

  if (err == ZIP_OK) /* offset of start of central directory with respect to the starting disk number */
  {
    ZPOS64_T pos = centraldir_pos_inzip - zi->add_position_when_writting_offset;
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (ZPOS64_T)pos, 8);
  }
  return err;
}

local int Write_EndOfCentralDirectoryRecord(zip64_internal* zi, uLong size_centraldir, ZPOS64_T centraldir_pos_inzip)
{
  int err = ZIP_OK;

  /*signature*/
  err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)ENDHEADERMAGIC, 4);

  if (err == ZIP_OK) /* number of this disk */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)zi->number_disk_with_CD, 2);

  if (err == ZIP_OK) /* number of the disk with the start of the central directory */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)zi->number_disk_with_CD, 2);

  if (err == ZIP_OK) /* total number of entries in the central dir on this disk */
  {
    {
      if (zi->number_entry >= 0xFFFF)
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0xffff, 2); // use value in ZIP64 record
      else
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)zi->number_entry, 2);
    }
  }

  if (err == ZIP_OK) /* total number of entries in the central dir */
  {
    if (zi->number_entry >= 0xFFFF)
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0xffff, 2); // use value in ZIP64 record
    else
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)zi->number_entry, 2);
  }

  if (err == ZIP_OK) /* size of the central directory */
    err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)size_centraldir, 4);

  if (err == ZIP_OK) /* offset of start of central directory with respect to the starting disk number */
  {
    ZPOS64_T pos = centraldir_pos_inzip - zi->add_position_when_writting_offset;
    if (pos >= 0xffffffff)
    {
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)0xffffffff, 4);
    }
    else
      err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)(pos), 4);
  }
  return err;
}

local int Write_GlobalComment(zip64_internal* zi, const char* global_comment)
{
  int err = ZIP_OK;
  uInt size_global_comment = 0;

  if (global_comment != NULL)
    size_global_comment = (uInt)strlen(global_comment);

  err = zip64local_putValue(&zi->z_filefunc, zi->filestream, (uLong)size_global_comment, 2);

  if (err == ZIP_OK && size_global_comment > 0)
  {
    if (ZWRITE64(zi->z_filefunc, zi->filestream, global_comment, size_global_comment) != size_global_comment)
      err = ZIP_ERRNO;
  }
  return err;
}

ZEXTERN int ZEXPORT zipClose2 (zipFile file, const char* global_comment, voidpf output)
{
    zip64_internal* zi;
    int err = 0;
    uLong size_centraldir = 0;
    ZPOS64_T centraldir_pos_inzip;
    ZPOS64_T pos;

    if (file == NULL)
        return ZIP_PARAMERROR;

    zi = (zip64_internal*)file;

    if (zi->in_opened_file_inzip == 1)
    {
        err = zipCloseFileInZip (file);
    }

#ifndef NO_ADDFILEINEXISTINGZIP
    if (global_comment == NULL)
        global_comment = zi->globalcomment;
#endif

    if (zi->filestream != zi->filestream_with_CD)
    {
        if (ZCLOSE64(zi->z_filefunc, zi->filestream, NULL) != 0)
            if (err == ZIP_OK)
                err = ZIP_ERRNO;
        if (zi->disk_size > 0)
            zi->number_disk_with_CD = zi->number_disk + 1;
        zi->filestream = zi->filestream_with_CD;
    }
    centraldir_pos_inzip = ZTELL64(zi->z_filefunc,zi->filestream);

    if (err == ZIP_OK)
    {
        linkedlist_datablock_internal* ldi = zi->central_dir.first_block;
        while (ldi != NULL)
        {
            if ((err == ZIP_OK) && (ldi->filled_in_this_block>0))
            {
                if (ZWRITE64(zi->z_filefunc, zi->filestream, ldi->data, ldi->filled_in_this_block) != ldi->filled_in_this_block)
                    err = ZIP_ERRNO;
            }

            size_centraldir += ldi->filled_in_this_block;
            ldi = ldi->next_datablock;
        }
    }
    free_linkedlist(&(zi->central_dir));

    pos = centraldir_pos_inzip - zi->add_position_when_writting_offset;
    if (pos >= 0xffffffff || zi->number_entry > 0xFFFF)
    {
      ZPOS64_T Zip64EOCDpos = ZTELL64(zi->z_filefunc, zi->filestream);
      Write_Zip64EndOfCentralDirectoryRecord(zi, size_centraldir, centraldir_pos_inzip);

      Write_Zip64EndOfCentralDirectoryLocator(zi, Zip64EOCDpos);
    }

    if (err == ZIP_OK)
      err = Write_EndOfCentralDirectoryRecord(zi, size_centraldir, centraldir_pos_inzip);

    if (err == ZIP_OK)
      err = Write_GlobalComment(zi, global_comment);

    if ((ZCLOSE64(zi->z_filefunc, zi->filestream, output) != 0) && (err == ZIP_OK))
      err = ZIP_ERRNO;

#ifndef NO_ADDFILEINEXISTINGZIP
    TRYFREE(zi->globalcomment);
#endif
    TRYFREE(zi);

    return err;
}

ZEXTERN int ZEXPORT zipClose (zipFile file, const char* global_comment)
{
	return zipClose2(file, global_comment, NULL);
}

ZEXTERN int ZEXPORT zipRemoveExtraInfoBlock (char* pData, int* dataLen, short sHeader)
{
  char* p = pData;
  int size = 0;
  char* pNewHeader;
  char* pTmp;
  short header;
  short dataSize;

  int retVal = ZIP_OK;

  if(pData == NULL || *dataLen < 4)
    return ZIP_PARAMERROR;

  pNewHeader = (char*)ALLOC(*dataLen);
  pTmp = pNewHeader;

  while(p < (pData + *dataLen))
  {
    header = *(short*)p;
    dataSize = *(((short*)p)+1);

    if( header == sHeader ) // Header found.
    {
      p += dataSize + 4; // skip it. do not copy to temp buffer
    }
    else
    {
      // Extra Info block should not be removed, So copy it to the temp buffer.
      memcpy(pTmp, p, dataSize + 4);
      p += dataSize + 4;
      size += dataSize + 4;
    }

  }

  if(size < *dataLen)
  {
    // clean old extra info block.
    memset(pData,0, *dataLen);

    // copy the new extra info block over the old
    if(size > 0)
      memcpy(pData, pNewHeader, size);

    // set the new extra info size
    *dataLen = size;

    retVal = ZIP_OK;
  }
  else
    retVal = ZIP_ERRNO;

  TRYFREE(pNewHeader);

  return retVal;
}
