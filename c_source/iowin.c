/* iowin32.c -- IO base function header for compress/uncompress .zip
     Version 1.1, February 14h, 2010
     part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications for Zip64 support
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

     For more info read MiniZip_info.txt

*/

#include <stdlib.h>

#include "zlib.h"
#include "ioapi.h"
#include "iowin.h"

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE (0xFFFFFFFF)
#endif

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

#define Unused(x)  x = x

#ifdef  UNICODE
#define WIDECHAR 1
#else
#define WIDECHAR 0
#endif

voidpf  ZCALLBACK win32_open_file_func  OF((voidpf opaque, const char* filename, int mode));
voidpf  ZCALLBACK win32_opendisk_file_func OF((voidpf opaque, voidpf stream, int disk_num, int mode));
uLong   ZCALLBACK win32_read_file_func  OF((voidpf opaque, voidpf stream, void* buf, uLong size));
uLong   ZCALLBACK win32_write_file_func OF((voidpf opaque, voidpf stream, const void* buf, uLong size));
ZPOS64_T ZCALLBACK win32_tell64_file_func  OF((voidpf opaque, voidpf stream));
long    ZCALLBACK win32_seek64_file_func  OF((voidpf opaque, voidpf stream, ZPOS64_T offset, int origin));
int     ZCALLBACK win32_close_file_func OF((voidpf opaque, voidpf stream,voidpf output));
int     ZCALLBACK win32_error_file_func OF((voidpf opaque, voidpf stream));

typedef struct
{
    HANDLE hf;
    int error;
	int filenameLength;
	WCHAR filename[1];
} WIN32FILE_IOWIN;


static void win32_translate_open_mode(int mode,
                                      DWORD* lpdwDesiredAccess,
                                      DWORD* lpdwCreationDisposition,
                                      DWORD* lpdwShareMode,
                                      DWORD* lpdwFlagsAndAttributes)
{
    *lpdwDesiredAccess = *lpdwShareMode = *lpdwFlagsAndAttributes = *lpdwCreationDisposition = 0;

    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
    {
        *lpdwDesiredAccess = GENERIC_READ;
        *lpdwCreationDisposition = OPEN_EXISTING;
        *lpdwShareMode = FILE_SHARE_READ;
    }
    else if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
    {
        *lpdwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
        *lpdwCreationDisposition = OPEN_EXISTING;
    }
    else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
    {
        *lpdwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
        *lpdwCreationDisposition = CREATE_ALWAYS;
    }
}

static voidpf win32_build_iowin(HANDLE hFile, LPCWSTR filename)
{
    WIN32FILE_IOWIN *w32fiow = NULL;

    if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE))
    {
		int len = wcslen(filename);
        w32fiow = (WIN32FILE_IOWIN *)malloc(sizeof(WIN32FILE_IOWIN) + len);
        if (w32fiow)
		{
			w32fiow->hf = hFile;
            w32fiow->error = 0;
			w32fiow->filenameLength = len;
			wcsncpy(w32fiow->filename, filename, len + 1);
		}
        else
		{
            CloseHandle(hFile);
		}
	}
    return w32fiow;
}

voidpf ZCALLBACK win32_open_file_funcW (voidpf opaque, LPCWSTR filename, int mode)
{
    DWORD dwDesiredAccess,dwCreationDisposition,dwShareMode,dwFlagsAndAttributes ;
    HANDLE hFile = NULL;
    Unused(opaque);
    win32_translate_open_mode(mode,&dwDesiredAccess,&dwCreationDisposition,&dwShareMode,&dwFlagsAndAttributes);

    if ((filename!=NULL) && (dwDesiredAccess != 0))
        hFile = CreateFileW((LPCWSTR)filename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwFlagsAndAttributes, NULL);

    return win32_build_iowin(hFile, filename);
}

voidpf ZCALLBACK win32_open_file_funcA (voidpf opaque, LPCSTR filename, int mode)
{
	voidpf ret = NULL;
	int len = MultiByteToWideChar(CP_ACP, 0, filename, -1, NULL, 0);
    Unused(opaque);
	if (len > 0)
	{
		LPWSTR name = malloc(len + 1);
		if (name)
		{
			len = MultiByteToWideChar(CP_ACP, 0, filename, -1, name, len + 1);
			ret = win32_open_file_funcW(opaque, name, mode);
			free(name);
		}
	}
    return ret;
}

#ifdef UNICODE
#define win32_open_file_func win32_open_file_funcW
#else
#define win32_open_file_func win32_open_file_funcA
#endif

voidpf ZCALLBACK win32_opendisk_file_func (voidpf opaque, voidpf stream, int number_disk, int mode)
{
    WIN32FILE_IOWIN *w32fiow = (WIN32FILE_IOWIN *)stream;
    LPWSTR diskFilename = NULL;
    voidpf ret = NULL;
    int i = 0;

    Unused(opaque);
    if (w32fiow)
	{
		diskFilename = (LPWSTR)malloc(w32fiow->filenameLength * sizeof(WCHAR));
		if (diskFilename)
		{
			wcsncpy(diskFilename, w32fiow->filename, w32fiow->filenameLength);
			for (i = w32fiow->filenameLength - 1; i >= 0; i -= 1)
			{
				if (diskFilename[i] != '.') 
					continue;
				swprintf(&diskFilename[i], w32fiow->filenameLength - i, L".z%02d", number_disk + 1);
				break;
			}
			if (i >= 0)
				ret = win32_open_file_funcW(opaque, diskFilename, mode);
			free(diskFilename);
		}
	}
    return ret;
}

uLong ZCALLBACK win32_read_file_func (voidpf opaque, voidpf stream, void* buf, uLong size)
{
    WIN32FILE_IOWIN *w32fiow = (WIN32FILE_IOWIN *)stream;
    uLong ret = 0;

	Unused(opaque);
	if (w32fiow && w32fiow->hf)
	{
        if (!ReadFile(w32fiow->hf, buf, size, &ret, NULL))
        {
            DWORD dwErr = GetLastError();
            if (dwErr == ERROR_HANDLE_EOF)
                dwErr = 0;
            w32fiow->error = (int)dwErr;
        }
    }
    return ret;
}

uLong ZCALLBACK win32_write_file_func (voidpf opaque, voidpf stream, const void* buf, uLong size)
{
    WIN32FILE_IOWIN *w32fiow = (WIN32FILE_IOWIN *)stream;
    uLong ret = 0;

    Unused(opaque);
	if (w32fiow && w32fiow->hf)
    {
        if (!WriteFile(w32fiow->hf, buf, size, &ret, NULL))
        {
            DWORD dwErr = GetLastError();
            if (dwErr == ERROR_HANDLE_EOF)
                dwErr = 0;
            w32fiow->error = (int)dwErr;
        }
    }
    return ret;
}

long ZCALLBACK win32_tell_file_func (voidpf opaque, voidpf stream)
{
    WIN32FILE_IOWIN *w32fiow = (WIN32FILE_IOWIN *)stream;
    long ret = -1;

	Unused(opaque);
	if (w32fiow && w32fiow->hf)
    {
        DWORD dwSet = SetFilePointer(w32fiow->hf, 0, NULL, FILE_CURRENT);
        if (dwSet == INVALID_SET_FILE_POINTER)
        {
            DWORD dwErr = GetLastError();
            w32fiow->error = (int)dwErr;
        }
        else
            ret = (long)dwSet;
    }
    return ret;
}

ZPOS64_T ZCALLBACK win32_tell64_file_func (voidpf opaque, voidpf stream)
{
    WIN32FILE_IOWIN *w32fiow = (WIN32FILE_IOWIN *)stream;
    ZPOS64_T ret = (ZPOS64_T)-1;

	Unused(opaque);
	if (w32fiow && w32fiow->hf)
    {
        LARGE_INTEGER li;
        li.QuadPart = 0;
        li.u.LowPart = SetFilePointer(w32fiow->hf, li.u.LowPart, &li.u.HighPart, FILE_CURRENT);
        if ( (li.LowPart == 0xFFFFFFFF) && (GetLastError() != NO_ERROR))
        {
            DWORD dwErr = GetLastError();
            w32fiow->error = (int)dwErr;
        }
        else
            ret = li.QuadPart;
    }
    return ret;
}


long ZCALLBACK win32_seek_file_func (voidpf opaque,voidpf stream,uLong offset,int origin)
{
    WIN32FILE_IOWIN *w32fiow = (WIN32FILE_IOWIN *)stream;
    long ret = -1;

	Unused(opaque);
	if (w32fiow && w32fiow->hf)
	{
		DWORD dwSet, dwMoveMethod = 0xFFFFFFFF;
		switch (origin)
		{
			case ZLIB_FILEFUNC_SEEK_CUR :
				dwMoveMethod = FILE_CURRENT;
				break;
			case ZLIB_FILEFUNC_SEEK_END :
				dwMoveMethod = FILE_END;
				break;
			case ZLIB_FILEFUNC_SEEK_SET :
				dwMoveMethod = FILE_BEGIN;
				break;
			default:
				return ret;
		}

        dwSet = SetFilePointer(w32fiow->hf, offset, NULL, dwMoveMethod);
        if (dwSet == INVALID_SET_FILE_POINTER)
        {
            DWORD dwErr = GetLastError();
            w32fiow->error = (int)dwErr;
        }
        else
            ret = 0;
    }
    return ret;
}

long ZCALLBACK win32_seek64_file_func (voidpf opaque, voidpf stream,ZPOS64_T offset,int origin)
{
    WIN32FILE_IOWIN *w32fiow = (WIN32FILE_IOWIN *)stream;
    long ret = -1;

	Unused(opaque);
	if (w32fiow && w32fiow->hf)
	{
		LARGE_INTEGER* li;
		DWORD dwSet, dwMoveMethod = 0xFFFFFFFF;
		switch (origin)
		{
			case ZLIB_FILEFUNC_SEEK_CUR :
				dwMoveMethod = FILE_CURRENT;
				break;
			case ZLIB_FILEFUNC_SEEK_END :
				dwMoveMethod = FILE_END;
				break;
			case ZLIB_FILEFUNC_SEEK_SET :
				dwMoveMethod = FILE_BEGIN;
				break;
			default:
				return ret;
		}

		li = (LARGE_INTEGER*)&offset;
        dwSet = SetFilePointer(w32fiow->hf, li->u.LowPart, &li->u.HighPart, dwMoveMethod);
        if (dwSet == INVALID_SET_FILE_POINTER)
        {
            DWORD dwErr = GetLastError();
            w32fiow->error = (int)dwErr;
        }
        else
            ret = 0;
    }
    return ret;
}

int ZCALLBACK win32_close_file_func (voidpf opaque, voidpf stream, voidpf output)
{
    WIN32FILE_IOWIN *w32fiow = (WIN32FILE_IOWIN *)stream;
    int ret = -1;

	Unused(opaque);
	Unused(output);
	if (w32fiow && w32fiow->hf)
	{
        CloseHandle(w32fiow->hf);
        ret = 0;
    }
	if (stream)
        free(stream);
    return ret;
}

int ZCALLBACK win32_error_file_func (voidpf opaque, voidpf stream)
{
    int ret = -1;

	Unused(opaque);
	if (stream != NULL)
    {
        ret = ((WIN32FILE_IOWIN*)stream)->error;
    }
    return ret;
}

void fill_win32_filefunc (zlib_filefunc_def* pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen_file = win32_open_file_funcA;
    pzlib_filefunc_def->zopendisk_file = win32_opendisk_file_func;
    pzlib_filefunc_def->zread_file = win32_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32_write_file_func;
    pzlib_filefunc_def->ztell_file = win32_tell_file_func;
    pzlib_filefunc_def->zseek_file = win32_seek_file_func;
    pzlib_filefunc_def->zclose_file = win32_close_file_func;
    pzlib_filefunc_def->zerror_file = win32_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_win32_filefunc64(zlib_filefunc64_def* pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen64_file = win32_open_file_func;
    pzlib_filefunc_def->zopendisk64_file = win32_opendisk_file_func;
    pzlib_filefunc_def->zread_file = win32_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32_write_file_func;
    pzlib_filefunc_def->ztell64_file = win32_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = win32_seek64_file_func;
    pzlib_filefunc_def->zclose_file = win32_close_file_func;
    pzlib_filefunc_def->zerror_file = win32_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}


void fill_win32_filefunc64A(zlib_filefunc64_def* pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen64_file = win32_open_file_funcA;
    pzlib_filefunc_def->zopendisk64_file = win32_opendisk_file_func;
    pzlib_filefunc_def->zread_file = win32_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32_write_file_func;
    pzlib_filefunc_def->ztell64_file = win32_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = win32_seek64_file_func;
    pzlib_filefunc_def->zclose_file = win32_close_file_func;
    pzlib_filefunc_def->zerror_file = win32_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}


void fill_win32_filefunc64W(zlib_filefunc64_def* pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen64_file = win32_open_file_funcW;
    pzlib_filefunc_def->zopendisk64_file = win32_opendisk_file_func;
    pzlib_filefunc_def->zread_file = win32_read_file_func;
    pzlib_filefunc_def->zwrite_file = win32_write_file_func;
    pzlib_filefunc_def->ztell64_file = win32_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = win32_seek64_file_func;
    pzlib_filefunc_def->zclose_file = win32_close_file_func;
    pzlib_filefunc_def->zerror_file = win32_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}
