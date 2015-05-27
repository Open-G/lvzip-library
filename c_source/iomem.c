/* iomem.c -- Memory base function header for compress/uncompress .zip
   files using zlib + zip or unzip API

   Copyright (C) 2007 Rolf Kalbermatter
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
   int number_disk,
   int mode));

uLong ZCALLBACK mem_read_file_func OF((
   voidpf opaque,
   voidpf stream,
   void* buf,
   uLong size));

uLong ZCALLBACK mem_write_file_func OF((
   voidpf opaque,
   voidpf stream,
   const void* buf,
   uLong size));

ZPOS64_T ZCALLBACK mem_tell_file_func OF((
   voidpf opaque,
   voidpf stream));

long ZCALLBACK mem_seek_file_func OF((
   voidpf opaque,
   voidpf stream,
   ZPOS64_T offset,
   int origin));

int ZCALLBACK mem_close_file_func OF((
   voidpf opaque,
   voidpf stream,
   voidpf output));

int ZCALLBACK mem_error_file_func OF((
   voidpf opaque,
   voidpf stream));

typedef struct
{
    int mode;
    ZPOS64_T pos;
    int error;
} MEMORY_IO;

voidpf ZCALLBACK mem_open_file_func (voidpf opaque, const void* filename, int mode)
{
    MEMORY_IO *memio = malloc(sizeof(MEMORY_IO));
	Unused(opaque);
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

voidpf ZCALLBACK mem_opendisk_file_func (voidpf opaque, voidpf stream, int number_disk, int mode)
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

uLong ZCALLBACK mem_read_file_func (voidpf opaque, voidpf stream, void* buf, uLong size)
{
    if (stream && opaque)
    {
        MEMORY_IO* mem = (MEMORY_IO*)stream;

        if (mem->pos + size > 0x7FFFFFFF)
          size = (uLong)(0x7FFFFFFF - mem->pos);
        if (mem->pos + size > (ZPOS64_T)LStrLen(*(LStrHandle)opaque))
          size = (uLong)(LStrLen(*(LStrHandle)opaque) - mem->pos);

        zmemcpy(buf, LStrBuf(*(LStrHandle)opaque) + mem->pos, size);
        mem->pos += size;
        return size;
	}
    return 0;
}

static MgErr ResizeStringHandle(LStrHandle handle, ZPOS64_T offset, const void *buf, ZPOS64_T *size, int32 mode)
{
    if (!handle)
        return mgArgErr;

    if (offset > 0x7FFFFFFF)
	  return mFullErr;

	if (offset + *size > 0x7FFFFFFF)
	  *size = (uLong)(0x7FFFFFFF - offset);

    if ((ZPOS64_T)DSGetHandleSize((UHandle)handle) < (offset + *size + sizeof(int32)))
    {
        MgErr err = fEOF;

        if (!(mode & ZLIB_FILEFUNC_MODE_WRITE))
            return err;

        err = NumericArrayResize(uB, 1, (UHandle*)&handle, Max(10000, (int32)(offset + *size) << 1));
		if (err)
            return err;
    }

    if (mode & ZLIB_FILEFUNC_MODE_WRITE)
    {
        if (buf)
            zmemcpy(LStrBuf(*handle) + offset, buf, (int32)*size);

        /* only grow the memory stream */
        if (offset + *size > (ZPOS64_T)LStrLen(*handle))
            LStrLen(*handle) = (int32)(offset + *size);
    }
    else if (offset + *size > (ZPOS64_T)LStrLen(*handle))
        return fEOF;

    return mgNoErr;
}

uLong ZCALLBACK mem_write_file_func (voidpf opaque, voidpf stream, const void* buf, uLong size)
{
    MEMORY_IO* mem = (MEMORY_IO*)stream;
	ZPOS64_T len = size;

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

ZPOS64_T ZCALLBACK mem_tell_file_func (voidpf opaque, voidpf stream)
{
    Unused(opaque);
    if (stream != NULL)
    {
        return ((MEMORY_IO*)stream)->pos;
    }
    return -1;
}

long ZCALLBACK mem_seek_file_func (voidpf opaque, voidpf stream, ZPOS64_T offset, int origin)
{
    if (stream != NULL)
    {
        MEMORY_IO* mem = (MEMORY_IO*)stream;
        ZPOS64_T start = 0;
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

        if (!(err = ResizeStringHandle((LStrHandle)opaque, start, NULL, &offset, mem->mode)))
        {
            mem->pos = start + offset;
            return 0;
        }
        mem->error = err;
    }
    return -1;
}

int ZCALLBACK mem_close_file_func (voidpf opaque, voidpf stream, voidpf output)
{
    int ret = -1;
    UHandle s = (UHandle)opaque;    

    if (output)
    {
        UHandle *o = (UHandle*)output;
        if (*o)
            DSDisposeHandle(*o);
        if (s)
        {
            /* Make sure the handle is not ridiculously oversized */
            int32 size = LStrLen(*(LStrHandle)s) + (int32)sizeof(int32);
            if (DSGetHandleSize(s) > size)
                DSSetHandleSize(s, size);
        }
        *o = s;
    }
    else if (s)
        DSDisposeHandle(s);

    if (stream != NULL)
    {
        free(stream);
        ret = 0;
    }
    return ret;
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

void fill_mem_filefunc (zlib_filefunc64_def* pzlib_filefunc_def, LStrHandle *memory)
{
    pzlib_filefunc_def->zopen64_file = mem_open_file_func;
    pzlib_filefunc_def->zopendisk64_file = mem_opendisk_file_func;
    pzlib_filefunc_def->zread_file = mem_read_file_func;
    pzlib_filefunc_def->zwrite_file = mem_write_file_func;
    pzlib_filefunc_def->ztell64_file = mem_tell_file_func;
    pzlib_filefunc_def->zseek64_file = mem_seek_file_func;
    pzlib_filefunc_def->zclose_file = mem_close_file_func;
    pzlib_filefunc_def->zerror_file = mem_error_file_func;
    if (*memory)
    {
        pzlib_filefunc_def->opaque = *memory;
        *memory = (LStrHandle)DSNewHClr(sizeof(int32));
    }
    else
    {
        pzlib_filefunc_def->opaque = DSNewHClr(sizeof(int32));
    }
}
