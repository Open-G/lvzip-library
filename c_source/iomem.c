/*
   iomem.c -- Memory base function header for compress/uncompress .zip files
   using zlib + zip or unzip API

   Copyright (C) 2007-2018 Rolf Kalbermatter

   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
   following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the
	   following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
       following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of SciWare, James Kring, Inc., nor the names of its contributors may be used to endorse
	   or promote products derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "zutil.h"
#include "ioapi.h"
#include "lvutil.h"
#include "iomem.h"

voidpf ZCALLBACK mem_open_file_func OF((
   voidpf opaque,
   const void* filename,
   int mode));

voidpf ZCALLBACK mem_opendisk_file_func OF((
   voidpf opaque,
   voidpf stream,
   uint32_t number_disk,
   int mode));

uint32_t ZCALLBACK mem_read_file_func OF((
   voidpf opaque,
   voidpf stream,
   void* buf,
   uint32_t size));

uint32_t ZCALLBACK mem_write_file_func OF((
   voidpf opaque,
   voidpf stream,
   const void* buf,
   uint32_t size));

uint64_t ZCALLBACK mem_tell64_file_func OF((
   voidpf opaque,
   voidpf stream));

long ZCALLBACK mem_seek64_file_func OF((
   voidpf opaque,
   voidpf stream,
   uint64_t offset,
   int origin));

int ZCALLBACK mem_close_file_func OF((
   voidpf opaque,
   voidpf stream));

int ZCALLBACK mem_error_file_func OF((
   voidpf opaque,
   voidpf stream));

typedef struct
{
    int mode;
    uint32_t pos;
    int error;
} MEMORY_IO;

voidpf ZCALLBACK mem_open_file_func (voidpf opaque, const void* filename, int mode)
{
    MEMORY_IO *memio = malloc(sizeof(MEMORY_IO));
	Unused(filename);
    if (memio)
    {
    	memio->pos = 0;
        memio->mode = mode;
        memio->error = 0;

        if (opaque && !(mode & ZLIB_FILEFUNC_MODE_CREATE))
             memio->pos = LStrLen(*(LStrHandle)opaque);
    }
    return memio;
}

voidpf ZCALLBACK mem_opendisk_file_func (voidpf opaque, voidpf stream, uint32_t number_disk, int mode)
{
    Unused(opaque);
    Unused(number_disk);
    Unused(mode);
    if (stream)
    {
        MEMORY_IO* mem = (MEMORY_IO*)stream;
		mem->error = fNotEnabled;
	}
	return NULL;
}

uint32_t ZCALLBACK mem_read_file_func (voidpf opaque, voidpf stream, void* buf, uint32_t size)
{
    if (stream && opaque)
    {
        MEMORY_IO* mem = (MEMORY_IO*)stream;

        if (mem->pos + size > 0x7FFFFFFF)
          size = (uint32_t)(0x7FFFFFFF - mem->pos);
        if (mem->pos + size > (uint64_t)LStrLen(*(LStrHandle)opaque))
          size = (uint32_t)(LStrLen(*(LStrHandle)opaque) - mem->pos);

        zmemcpy(buf, LStrBuf(*(LStrHandle)opaque) + mem->pos, size);
        mem->pos += size;
        return size;
	}
    return 0;
}

static MgErr ResizeStringHandle(LStrHandle handle, uint32_t offset, const void *buf, uint32_t *size, int32 mode)
{
    if (!handle)
        return mgArgErr;

    if (offset > 0x7FFFFFFF)
	  return mFullErr;

	if (offset + *size > 0x7FFFFFFF)
	  *size = 0x7FFFFFFF - offset;

    if ((uInt32)DSGetHandleSize((UHandle)handle) < (offset + *size + sizeof(int32)))
    {
        MgErr err = fEOF;

        if (!(mode & ZLIB_FILEFUNC_MODE_WRITE))
            return err;

        err = NumericArrayResize(uB, 1, (UHandle*)&handle, Max(10000, (size_t)(offset + *size) << 1));
		if (err)
            return err;
    }

    if (mode & ZLIB_FILEFUNC_MODE_WRITE)
    {
        if (buf)
            zmemcpy(LStrBuf(*handle) + offset, buf, (size_t)*size);

        /* only grow the memory stream */
        if (offset + *size > (uint32_t)LStrLen(*handle))
            LStrLen(*handle) = (int32)(offset + *size);
    }
    else if (offset + *size > (uint32_t)LStrLen(*handle))
        return fEOF;

    return mgNoErr;
}

uint32_t ZCALLBACK mem_write_file_func (voidpf opaque, voidpf stream, const void* buf, uint32_t size)
{
    MEMORY_IO* mem = (MEMORY_IO*)stream;
	uint32_t len = size;

    if (mem != NULL && (mem->mode & ZLIB_FILEFUNC_MODE_WRITE))
    { 
		MgErr err = ResizeStringHandle((LStrHandle)opaque, mem->pos, buf, &len, mem->mode); 
        if (err)
        {
            mem->error = err;
            return 0;
        }
        mem->pos += len;
        return size; 
    }
    return 0;
}

uint64_t ZCALLBACK mem_tell64_file_func (voidpf opaque, voidpf stream)
{
    Unused(opaque);
    if (stream != NULL)
    {
        return (uint64_t)((MEMORY_IO*)stream)->pos;
    }
    return (uint64_t)-1L;
}

long ZCALLBACK mem_seek64_file_func (voidpf opaque, voidpf stream, uint64_t offset, int origin)
{
    if (stream != NULL)
    {
        MEMORY_IO* mem = (MEMORY_IO*)stream;
        uint32_t start = 0, len = (uint32_t)offset;
        MgErr err;

        switch (origin)
        {
            case ZLIB_FILEFUNC_SEEK_CUR :
                start = mem->pos;
                break;
            case ZLIB_FILEFUNC_SEEK_END :
                if (opaque)
                    start = LStrLen(*(LStrHandle)opaque);
                /* fall through */
            case ZLIB_FILEFUNC_SEEK_SET :
                break;
            default:
                return -1;
        }
		err = ResizeStringHandle((LStrHandle)opaque, start, NULL, &len, mem->mode);
        if (!err)
        {
            mem->pos = start + len;
            return 0;
        }
        mem->error = err;
    }
    return -1;
}

int ZCALLBACK mem_close_file_func (voidpf opaque, voidpf stream)
{
    Unused(opaque);
    if (stream != NULL)
		free(stream);
    return 0;
}

int ZCALLBACK mem_error_file_func (voidpf opaque, voidpf stream)
{
    int ret = -1;
    Unused(opaque);
    if (stream != NULL)
    {
        ret = ((MEMORY_IO*)stream)->error;
    }
    return ret;
}

void fill_mem_filefunc(zlib_filefunc64_def* pzlib_filefunc_def, LStrHandle *memory)
{
    pzlib_filefunc_def->zopen64_file = mem_open_file_func;
    pzlib_filefunc_def->zopendisk64_file = mem_opendisk_file_func;
    pzlib_filefunc_def->zread_file = mem_read_file_func;
    pzlib_filefunc_def->zwrite_file = mem_write_file_func;
    pzlib_filefunc_def->ztell64_file = mem_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = mem_seek64_file_func;
    pzlib_filefunc_def->zclose_file = mem_close_file_func;
    pzlib_filefunc_def->zerror_file = mem_error_file_func;
    if (*memory)
    {
        pzlib_filefunc_def->opaque = *memory;
        *memory = NULL;
    }
    else
    {
        pzlib_filefunc_def->opaque = DSNewHClr(sizeof(int32));
    }
}
