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
   const char* filename,
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

long ZCALLBACK mem_tell_file_func OF((
   voidpf opaque,
   voidpf stream));

long ZCALLBACK mem_seek_file_func OF((
   voidpf opaque,
   voidpf stream,
   uLong offset,
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
    uLong pos;
    int error;
} MEMORY_IO;

voidpf ZCALLBACK mem_open_file_func (opaque, filename, mode)
   voidpf opaque;
   const char* filename;
   int mode;
{
    MEMORY_IO *memio = malloc(sizeof(MEMORY_IO));
    if (memio)
    {
        memio->pos = 0;
        memio->mode = mode;
        memio->error = 0;

/*        if (opaque && !(mode & ZLIB_FILEFUNC_MODE_CREATE))
             memio->pos = LStrLen(*(LStrHandle)opaque); */
    }
    return memio;
}


uLong ZCALLBACK mem_read_file_func (opaque, stream, buf, size)
   voidpf opaque;
   voidpf stream;
   void* buf;
   uLong size;
{
    if (stream && opaque)
    {
        MEMORY_IO* mem = (MEMORY_IO*)stream;

        if (mem->pos + size > (uLong)LStrLen(*(LStrHandle)opaque))
            size = LStrLen(*(LStrHandle)opaque) - mem->pos;

        zmemcpy(buf, LStrBuf(*(LStrHandle)opaque) + mem->pos, size);
        mem->pos += size;
        return size;
    }
    return 0;
}

static MgErr ResizeStringHandle(LStrHandle handle, int32 offset, const void *buf, int32 size, int32 mode)
{
    if (!handle)
        return mgArgErr;

    if (DSGetHandleSize((UHandle)handle) < (offset + size + (int32)sizeof(int32)))
    {
        MgErr err = fEOF;

        if (!(mode & ZLIB_FILEFUNC_MODE_WRITE))
            return err;

        err = NumericArrayResize(uB, 1, (UHandle*)&handle, Max(10000, (offset + size) << 1));
		if (err)
            return err;
    }

    if (mode & ZLIB_FILEFUNC_MODE_WRITE)
    {
        if (buf)
            zmemcpy(LStrBuf(*handle) + offset, buf, size);

        /* only grow the memory stream */
        if (offset + size > LStrLen(*handle))
            LStrLen(*handle) = offset + size;
    }
    else if (offset + size > LStrLen(*handle))
        return fEOF;

    return mgNoErr;
}


uLong ZCALLBACK mem_write_file_func (opaque, stream, buf, size)
   voidpf opaque;
   voidpf stream;
   const void* buf;
   uLong size;
{
    MEMORY_IO* mem = (MEMORY_IO*)stream;

    if (mem != NULL && (mem->mode & ZLIB_FILEFUNC_MODE_WRITE))
    {
        MgErr err = ResizeStringHandle((LStrHandle)opaque, mem->pos, buf, size, mem->mode); 
        if (err)
        {
            mem->error = err;
            return 0;
        }
        mem->pos += size;
        return size; 
    }
    return 0;
}

long ZCALLBACK mem_tell_file_func (opaque, stream)
   voidpf opaque;
   voidpf stream;
{
    if (stream != NULL)
    {
        return (long)((MEMORY_IO*)stream)->pos;
    }
    return -1;
}

long ZCALLBACK mem_seek_file_func (opaque, stream, offset, origin)
   voidpf opaque;
   voidpf stream;
   uLong offset;
   int origin;
{
    if (stream != NULL)
    {
        MEMORY_IO* mem = (MEMORY_IO*)stream;
        uLong size = 0;
        MgErr err;

        switch (origin)
        {
            case ZLIB_FILEFUNC_SEEK_CUR :
                size = mem->pos + offset;
                break;
            case ZLIB_FILEFUNC_SEEK_END :
                if (opaque)
                    size = LStrLen(*(LStrHandle)opaque);
                /* fall through */
            case ZLIB_FILEFUNC_SEEK_SET :
                size += offset;
                break;
            default:
                return -1;
        }

        if (!(err = ResizeStringHandle((LStrHandle)opaque, 0, NULL, size, mem->mode)))
        {
            mem->pos = size;
            return 0;
        }
        mem->error = err;
    }
    return -1;
}

int ZCALLBACK mem_close_file_func (opaque, stream, output)
   voidpf opaque;
   voidpf stream;
   voidpf output;
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

int ZCALLBACK mem_error_file_func (opaque, stream)
   voidpf opaque;
   voidpf stream;
{
    int ret=-1;
    if (stream!=NULL)
    {
        ret = ((MEMORY_IO*)stream)->error;
    }
    return ret;
}

void fill_mem_filefunc (pzlib_filefunc_def, memory)
  zlib_filefunc_def* pzlib_filefunc_def;
  LStrHandle *memory;
{
    pzlib_filefunc_def->zopen_file = mem_open_file_func;
    pzlib_filefunc_def->zread_file = mem_read_file_func;
    pzlib_filefunc_def->zwrite_file = mem_write_file_func;
    pzlib_filefunc_def->ztell_file = mem_tell_file_func;
    pzlib_filefunc_def->zseek_file = mem_seek_file_func;
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
