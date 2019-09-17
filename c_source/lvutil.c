/* 
   lvutil.c -- support functions for LabVIEW ZIP library

   Version 1.25, Dec 07, 2018

   Copyright (C) 2002-2018 Rolf Kalbermatter

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

#define ZLIB_INTERNAL
#include "lvutil.h"
#include "zlib.h"
#include "ioapi.h"
#include "iomem.h"
#include "utf.h"
#if Unix
 #ifndef __USE_FILE_OFFSET64
  #define __USE_FILE_OFFSET64
 #endif
 #ifndef __USE_LARGEFILE64
  #define __USE_LARGEFILE64
 #endif
 #ifndef _LARGEFILE64_SOURCE
  #define _LARGEFILE64_SOURCE
 #endif
 #ifndef _FILE_OFFSET_BIT
  #define _FILE_OFFSET_BIT 64
 #endif
#endif
#include <stdio.h>
#include <string.h>
#if Win32
 #define COBJMACROS
 #include "iowin.h"
 #include <ole2.h>
 #include <shlobj.h>
 #ifndef INVALID_FILE_ATTRIBUTES
  #define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
 #endif
 #define SYMLINK_FLAG_RELATIVE 1
 typedef struct _REPARSE_DATA_BUFFER
 {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union
    {
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;                    // contains SYMLINK_FLAG_RELATIVE(1) or 0
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct
        {
            UCHAR  DataBuffer[1];
        } GenericReparseBuffer;
    } DUMMYUNIONNAME;
 } REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

#define REPARSE_DATA_BUFFER_HEADER_SIZE   FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer)

#elif Unix
 #include <errno.h>
 #include <dirent.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/stat.h>
 #define ftruncate64 ftruncate
 #ifdef HAVE_ICONV
  #include <iconv.h>
 #endif
 #include <wchar.h>
 #if VxWorks
  #include <sys/types.h>
  #include <string.h>
  #include <utime.h>
//  #include <ioLib.h>
  #ifdef __GNUC__
   #define ___unused __attribute__((unused))
  #else
   #define ___unused
  #endif

  #ifndef S_ISLNK
   #define	S_ISLNK(mode)	((mode & S_IFMT) == S_IFLNK)	/* symlink special */
  #endif

  #if _WRS_VXWORKS_MAJOR < 6 || _WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 2
  inline int link(const char* path1 ___unused, const char* path2 ___unused)
  {
	// VxWorks < 6.2 has no link() support
    errno = ENOTSUP;
    return -1;
  }

  inline int chmod(const char* _path ___unused, mode_t _mode ___unused)
  {
	// VxWorks < 6.2 has no chmod() support
    errno = ENOTSUP;
    return -1;
  }
  #endif

  // Fake symlink handling by dummy functions:
  inline int symlink(const char* path1 ___unused, const char* path2 ___unused)
  {
    // vxWorks has no symlinks -> always return an error!
    errno = ENOTSUP;
    return -1;
  }

  inline ssize_t readlink(const char* path1 ___unused, char* path2 ___unused, size_t size ___unused)
  {
    // vxWorks has no symlinks -> always return an error!
    errno = EACCES;
    return -1;
  }
  #define lstat(path, statbuf) stat(path, statbuf) 
  #define FCNTL_PARAM3_CAST(param)  (int32)(param)
 #else
  #define FCNTL_PARAM3_CAST(param)  (param)
 #endif
#elif MacOSX
 #include <CoreFoundation/CoreFoundation.h>
 #include <CoreServices/CoreServices.h>
 #include <sys/stat.h>
 #include <sys/xattr.h>
 #define ftruncate64 ftruncate
 #if ProcessorType!=kX64
  #define MacSpec	FSRef
  #define MacIsInvisible(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible)
  #define MacIsInvFolder(cpb) ((cpb).dirInfo.ioDrUsrWds.frFlags & kIsInvisible)
  #define MacIsDir(cpb)   ((cpb).nodeFlags & ioDirMask)
  #define MacIsStationery(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsStationery)
  #define MacIsAlias(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsAlias)
  #define kFileChanged    (1L<<7)
  static MgErr OSErrToLVErr(OSErr err);
 #else
  #include <dirent.h>
 #endif
#endif

#define usesHFSPath      MacOSX && ProcessorType != kX64
#define usesPosixPath    Unix || (MacOSX && ProcessorType == kX64)
#define usesWinPath      Win32

#define kMaxFileExtLength   10

#define kPosixPathSeperator  '/'

#if usesHFSPath
#define kPathSeperator ':'
typedef FSIORefNum FileRefNum;
#elif usesPosixPath
#define kPathSeperator '/'
typedef FILE* FileRefNum;
#elif usesWinPath
#define kPathSeperator '\\'
typedef HANDLE FileRefNum;
#endif

#if usesWinPath && !defined(EMBEDDED)
#define LWStrPtr  WStrPtr
#define LWStrLen  WStrLen
#define LWStrBuf  WStrBuf

#define LWStrNCat  WStrNCat
#define lwslen     wcslen
#define lwsrchr    wcsrchr

static const wchar_t strWildcardPattern[] = L"*.*";
#else
#define LWStrPtr  LStrPtr
#define LWStrLen  LStrLen
#define LWStrBuf  LStrBuf

#define LWStrNCat  LStrNCat
#define lwslen     strlen
#define lwsrchr    strrchr

static char strWildcardPattern[] = "*.*";

#define SStrBuf(s)  (char*)LStrBuf(s)
#endif

#ifdef HAVE_BZIP2
void bz_internal_error(int errcode);
void bz_internal_error(int errcode)
{
	// if we have a debug build then print the error in the LabVIEW debug console
#if DEBUG
	DbgPrintf((CStr)"BZIP2 internal error %ld occurred!!", errcode);
#else
    Unused(errcode);
#endif
}
#endif

LibAPI(void) DLLVersion(uChar* version)
{
    sprintf((char*)version, "lvzlib V 4.2B2, date: %s, time: %s",__DATE__,__TIME__);
}

#if usesHFSPath
static MgErr FSMakePathRef(Path path, FSRef *ref)
{
	LStrHandle str = NULL;
    MgErr err = LVPath_ToText(path, &str);
    DEBUGPRINTF(((CStr)"FPathToText1: path = %z", path));
	if (!err)
	{
	    err = OSErrToLVErr(FSPathMakeRef(LStrBuf(*str), ref, NULL));
		if (err)
		{
			DEBUGPRINTF(((CStr)"FSPathMakeRef: err = %ld, len = %ld, path = %s", err, LStrLen(*str), LStrBuf(*str)));
		}
		DSDisposeHandle((UHandle)str);
    }
    return err;
}

static MgErr OSErrToLVErr(OSErr err)
{
    switch(err)
    {
      case mgNoErr:
        return mgNoErr;
      case rfNumErr:
      case paramErr:
      case nsvErr:
      case fnOpnErr:
      case bdNamErr:
        return mgArgErr;
      case vLckdErr:
      case wrPermErr:
      case wPrErr:
      case fLckdErr:
      case afpAccessDenied:
      case permErr:
        return fNoPerm;
      case fBsyErr:
      case opWrErr:
        return fIsOpen;
      case posErr:
      case eofErr:
        return fEOF;
      case dirNFErr:
      case fnfErr:
        return fNotFound;
      case dskFulErr:
        return fDiskFull;
      case dupFNErr:
        return fDupPath;
      case tmfoErr:
        return fTMFOpen;
      case memFullErr:
        return mFullErr;
      case afpObjectTypeErr:
      case afpContainsSharedErr:
      case afpInsideSharedErr:
      return fNotEnabled;
    }
    return fIOErr; /* fIOErr generally signifies some unknown file error */
}
#endif

#if Win32
/* int64 100ns intervals from Jan 1 1601 GMT to Jan 1 1904 GMT */
#define LV1904_FILETIME_OFFSET  0x0153b281e0fb4000
#define SECS_TO_FT_MULT			10000000

static void ATimeToFileTime(ATime128 *pt, FILETIME *pft)
{
	LARGE_INTEGER li;
	li.QuadPart = pt->u.f.val * SECS_TO_FT_MULT;
	li.QuadPart += (pt->u.f.fract >> 32) * SECS_TO_FT_MULT / 0x100000000L;
	li.QuadPart += LV1904_FILETIME_OFFSET;
	pft->dwHighDateTime = li.HighPart;
	pft->dwLowDateTime = li.LowPart;
}

static void FileTimeToATime(FILETIME *pft, ATime128 *pt)
{
	LARGE_INTEGER li;    
	li.LowPart = pft->dwLowDateTime;
	li.HighPart = pft->dwHighDateTime;
	li.QuadPart -= LV1904_FILETIME_OFFSET;
	pt->u.f.val = li.QuadPart / SECS_TO_FT_MULT;
	pt->u.f.fract = ((li.QuadPart - pt->u.f.val) * 0x100000000 / SECS_TO_FT_MULT) << 32;
}

static MgErr Win32ToLVFileErr(DWORD winErr)
{
    switch (winErr)
    {
		case ERROR_NOT_ENOUGH_MEMORY: return mFullErr;
	    case ERROR_NOT_LOCKED:
		case ERROR_INVALID_NAME:
		case ERROR_INVALID_PARAMETER: return mgArgErr;
		case ERROR_PATH_NOT_FOUND:
		case ERROR_DIRECTORY:
		case ERROR_FILE_NOT_FOUND:    return fNotFound;
		case ERROR_LOCK_VIOLATION:
		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION: return fNoPerm;
		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS:       return fDupPath;
		case ERROR_NOT_SUPPORTED:     return mgNotSupported;
		case ERROR_NO_MORE_FILES:     return mgNoErr;
    }
    return fIOErr;   /* fIOErr generally signifies some unknown file error */
}

static MgErr HRESULTToLVErr(HRESULT winErr)
{
	switch (winErr)
	{
		case S_OK:                    return mgNoErr;
		case E_FAIL:
		case E_NOINTERFACE:
	    case E_POINTER:               return mgArgErr;
		case E_OUTOFMEMORY:           return mFullErr;
		case REGDB_E_CLASSNOTREG:
		case CLASS_E_NOAGGREGATION:   return mgNotSupported;
	}
    return fIOErr;   /* fIOErr generally signifies some unknown file error */
}

static MgErr Win32GetLVFileErr(void)
{
	return Win32ToLVFileErr(GetLastError());
}

static MgErr NormalizePath(LWStrPtr lwstr)
{
#if EMBEDDED
	LPSTR a, s, e, t = LWStrBuf(lwstr);
#else
	LPWSTR a, s, e, t = LWStrBuf(lwstr);
#endif
	int32 unc = FALSE, len = LWStrLen(lwstr);

	a = s = t;
	e = s + len;
	if (!*s || !len)
		return noErr;
  
/*
	\\?\UNC\server\share
	\\?\C:\
	\\.\UNC\server\share
	\\.\C:\
	\\server\share
	C:\
*/

	if (s[0] == kPathSeperator)
	{
		int32 n = 1;
		s++;
		if (s[0] == kPathSeperator)
		{
			n++;
			s++;
			if ((s[0] == '?' || s[0] == '.') && s[1] == kPathSeperator)
			{
				n += 2;
				s += 2;
				if ((s[0] == 'U' || s[0] == 'u') && (s[1] == 'N' || s[1] == 'n') && (s[2] == 'C' || s[2] == 'C') && s[2] == kPathSeperator)
				{
					unc = TRUE;
					n += 4;
					s += 4;
				}
			}
			else if (isalpha(s[0]))
			{
				unc = TRUE;
			}
			else
			{
				return mgArgErr;
			}
		}
		t += n;
		a = s;
	}
	if (isalpha(s[0]) && s[1] == ':')
	{
		/* X:\ */
		s += 2;
		t += 2;
		if (s[0] == kPathSeperator)
		{
			s++; t++;
		}
	}

	/* Canonicalize the rest of the path */
	while (s < e)
	{
		if (s[0] == '.')
		{
			if (s[1] == kPathSeperator && (s == a || s[-1] == kPathSeperator || s[-1] == ':'))
			{
				s += 2; /* Skip .\ */
			}
			else if (s[1] == '.' && (t == a || t[-1] == kPathSeperator))
			{
				/* \.. backs up a directory, over the root if it has no \ following X:.
				 * .. is ignored if it would remove a UNC server name or initial \\
				 */
				if (t != a)
				{
					if (t > a + 1 && t[-1] == kPathSeperator &&
					    (t[-2] != kPathSeperator || t > a + 2))
					{
						if (t[-2] == ':' && (t > a + 3 || t[-3] == ':'))
						{
							t -= 2;
							while (t > a && *t != kPathSeperator)
								t--;
							if (*t == kPathSeperator)
								t++; /* Reset to last '\' */
							else
								t = a; /* Start path again from new root */
						}
						else if (t[-2] != ':' && !unc)
							t -= 2;
					}
					while (t > a && *t != kPathSeperator)
						t--;
					if (t == a)
					{
						*t = kPathSeperator;
						s++;
					}
				}
				s += 2; /* Skip .. in src path */
			}
			else
				*t++ = *s++;
		}
		else
			*t++ = *s++;
	}
	/* Append \ to naked drive spec */
	if (t - a == 2 && t[-1] == ':')
		*t++ = kPathSeperator;
	*t++ = '\0';
	return noErr;
}

#undef CreateFile
#undef GetFileAttributes
#undef SetFileAttributes
#undef FindFirstFile
#undef FindNextFile
#if EMBEDDED
#define CreateFile(path, fl, sh, ds, op, se, ov) CreateFileA(LStrBuf(path), fl, sh, ds, op, se, ov)
#define GetFileAttributes(path)         GetFileAttributesA(LStrBuf(path))
#define SetFileAttributes(path, attr)   SetFileAttributesA(LStrBuf(path), attr)
#define FindFirstFile(path, findFiles)  FindFirstFileA(LStrBuf(path), findFiles)
#define FindNextFile(handle, findFiles) FindNextFileA(handle, findFiles)
#else
#define CreateFile(path, fl, sh, ds, op, se, ov) CreateFileW(WStrBuf(path), fl, sh, ds, op, se, ov)
#define GetFileAttributes(path)         GetFileAttributesW(WStrBuf(path))
#define SetFileAttributes(path, attr)   SetFileAttributesW(WStrBuf(path), attr)
#define FindFirstFile(path, findFiles)  FindFirstFileW(WStrBuf(path), findFiles)
#define FindNextFile(handle, findFiles) FindNextFileW(handle, findFiles)
#endif

#elif Unix || MacOSX
/* seconds between Jan 1 1904 GMT and Jan 1 1970 GMT */
#define dt1970re1904    2082844800L

#if VxWorks
// on VxWorks the stat time values are unsigned long integer
static void VxWorksConvertFromATime(ATime128 *time, unsigned long *sTime)
{
	if (time->u.f.val > dt1970re1904)
		*sTime = (unsigned long)(time->u.f.val - dt1970re1904);
	else
		*sTime = 0;
} 

static void VxWorksConvertToATime(unsigned long sTime, ATime128 *time)
{
	time->u.f.val = (int64_t)sTime + dt1970re1904;
	time->u.f.fract = 0;
}
#else
// on Mac/Linux kernel 2.6 and newer the stat time values are struct timespec values
static void UnixConvertFromATime(ATime128 *time, struct timespec *sTime)
{
	sTime->tv_sec = (time_t)(time->u.f.val - dt1970re1904);
	sTime->tv_nsec = (int32_t)(time->u.p.fractHi / 4.294967296);
}

static void UnixConvertToATime(struct timespec *sTime, ATime128 *time)
{
	time->u.f.val = (int64_t)(sTime->tv_sec + dt1970re1904);
	time->u.p.fractHi = (uint32_t)sTime->tv_nsec * 4.294967296;
	time->u.p.fractLo = 0;
}
#endif

static MgErr UnixToLVFileErr(void)
{
    switch (errno)
    {
      case 0:           return mgNoErr;
      case ESPIPE:      return fEOF;
      case EINVAL:
      case EBADF:       return mgArgErr;
      case ETXTBSY:     return fIsOpen;
      case ENOENT:      return fNotFound;
#ifdef EAGAIN
      case EAGAIN:  /* SVR4, file is locked */
#endif
#ifdef EDEADLK
      case EDEADLK: /* deadlock would occur */
#endif
#ifdef ENOLCK
      case ENOLCK:  /* NFS, lock not avail */
#endif
      case EPERM:
      case EACCES:      return fNoPerm;
      case ENOSPC:      return fDiskFull;
      case EEXIST:      return fDupPath;
      case ENFILE:
      case EMFILE:      return fTMFOpen;
      case ENOMEM:      return mFullErr;
      case EIO:         return fIOErr;
    }
    return fIOErr;   /* fIOErr generally signifies some unknown file error */
}

static MgErr NormalizePath(LWStrPtr lwstr)
{
	char *a, s, e, t = LWStrBuf(lwstr);
	int32 len = LWStrLen(lwstr);

	a = s = t;
	e = s + len;
	if (!*s || !len)
		return noErr;
  
	/* Canonicalize the path */
	while (s < e)
	{
		if (s[0] == kPosixPathSeperator)
		{
			if (s[1] == kPosixPathSeperator)
				s++; /* Reduce // to / */ 
		}
		else if (s[0] == '.')
		{
			if (s[1] == kPosixPathSeperator && (s == a || s[-1] == kPosixPathSeperator))
			{
				s += 2; /* Skip ./ */
			}
			else if (s[1] == '.' && (t == a || t[-1] == kPosixPathSeperator))
			{
				/* .. backs up a directory
				 */
				if (t != a)
				{
					if (t > a + 1 && t[-1] == kPosixPathSeperator &&
					    (t[-2] != kPosixPathSeperator || t > a + 2))
					{
						if (t[-2] == ':' && (t > a + 3 || t[-3] == ':'))
						{
							t -= 2;
							while (t > a && *t != kPosixPathSeperator)
								t--;
							if (*t == kPosixPathSeperator)
								t++; /* Reset to last '\' */
							else
								t = a; /* Start path again from new root */
						}
						else if (t[-2] != ':' && !unc)
							t -= 2;
					}
					while (t > a && *t != kPosixPathSeperator)
						t--;
					if (t == a)
					{
						*t = kPosixPathSeperator;
						s++;
					}
				}
				s += 2; /* Skip .. in src path */
			}
			else
				*t++ = *s++;
		}
		else
			*t++ = *s++;
	}
	*t++ = '\0';
	return noErr;
}
#endif

#if Unix || MacOSX || defined(EMBEDDED)
static MgErr makeFileDSString(LStrHandle string, uInt32 codePage, LWStrPtr *lwstr, size_t *reserve)
{
	MgErr err = noErr;
	LStrHandle dest = NULL;
	int32 bufLen = 0;
	if (LStrLenH(string))
	{
#if usesHFSPath
		err = ConvertToPosixPath(string, codePage, &dest, CP_ACP, '?', NULL, FALSE);
#else
		err = ConvertLString(string, codePage, &dest, CP_ACP, '?', NULL);
#endif
	}
#if Unix || MacOSX
	else
	{
		bufLen = 1;
	}
#endif
	if (!err)
	{
		bufLen += LStrLenH(dest) + 1 + (reserve ? *reserve : 0);
		if (*lwstr && LStrLenH(lwstr) + 1 < bufLen)
		{
			DSDisposePtr((UPtr)*lwstr);
			*lwstr = NULL;
		}
		if (!*lwstr)
		{
			*lwstr = (LWStrPtr)DSNewPtr(sizeof(int32) + bufLen);
			if (!*lwstr)
			{
				DSDisposeHandle((UHandle)dest);
				return mFullErr;
			}
		}
		if (LStrLenH(string))
		{
			MoveBlock((UPtr)*dest, (UPtr)*lwstr, sizeof(int32) + LStrLenH(dest));
		}
#if Unix || MacOSX
		else
		{
			LStrBuf(*lwstr)[0] = kPathSeparator;
			LStrLen(*lwstr) = 1;
		}
#endif
		LStrBuf(*lwstr)[LStrLen(*lwstr)] = 0;
		DSDisposeHandle((UHandle)dest);
		if (reserve)
			*reserve = bufLen;
	}
	return NormalizePath(*lwstr);
}

static MgErr LStrNCat(LStrPtr lstr, int32 off, int32 bufLen, const char *str, int32 strLen)
{
	if (strLen == -1)
	    strLen = strlen(str);
	if (off == -1)
		off = LStrLen(lstr);
	if (off + strLen < bufLen)
	{
		strncpy(LStrBuf(lstr) + off, str, strLen + 1);
		LStrLen(lstr) = off + strLen;
		return noErr;
	}
	return mgArgErr;
}

#else /* Windows and not Pharlap */
static MgErr makeFileDSString(LStrHandle string, uInt32 codePage, LWStrPtr *wstr, int32 *reserve)
{
	LPCSTR buf = (LPCSTR)LStrBufH(string);
	int32 bufLen = 0, off = 0, len = 0;
	
	if (LStrLenH(string))
	{
		bufLen = MultiByteToWideChar(codePage, 0, buf, LStrLenH(string), NULL, 0);
		if (bufLen <= 0)
			return Win32GetLVFileErr();

		if (isalpha(buf[0]) && buf[1] == ':')
		{
			/* Absolute path with drive letter */
			len = 4;
		}
		else if (buf[0] == kPathSeperator && buf[1] == kPathSeperator && isalpha(buf[2]))
		{
			/* Absolute UNC path */
			len = 7;
			off = 1;
		}
	}
	bufLen += len + 1 + (reserve ? *reserve : 0);
	if (*wstr && WStrLen(*wstr) + 1 < bufLen)
	{
		DSDisposePtr((UPtr)*wstr);
		*wstr = NULL;
	}
	if (!*wstr)
	{
		*wstr = (WStrPtr)DSNewPtr(sizeof(int32) + sizeof(wchar_t) * bufLen);
		if (!*wstr)
			return mFullErr;
	}
	if (LStrLenH(string))
	{
		if (isalpha(buf[0]) && buf[1] == ':')
		{
			wcscpy(WStrBuf(*wstr), L"\\\\?\\");
		}
		else if (buf[0] == kPathSeperator && buf[1] == kPathSeperator && isalpha(buf[2]))
		{
			wcscpy(WStrBuf(*wstr), L"\\\\?\\UNC");
		}
		len += MultiByteToWideChar(CP_ACP, 0, buf + off, LStrLenH(string) - off, WStrBuf(*wstr) + len, bufLen - len);
	}
	WStrLenSet(*wstr, len);
	WStrBuf(*wstr)[len] = 0;

	if (reserve)
		*reserve = bufLen;

	return NormalizePath(*wstr);
}

static MgErr WStrNCat(WStrPtr wstr, int32 off, int32 bufLength, const wchar_t *str, int32 strLen)
{
	if (strLen == -1)
	    strLen = (int32)wcslen(str);
	if (off == -1)
		off = WStrLen(wstr);
	if (off + strLen < bufLength)
	{
		wcsncpy(WStrBuf(wstr) + off, str, strLen + 1);
		WStrLenSet(wstr, off + strLen);
		return noErr;
	}
	return mgArgErr;
}

#define Win32HasDOSDevicePrefix(str) (str[0] == kPathSeperator && str[1] == kPathSeperator && (str[2] == '?' || str[2] == '.') && str[3] == kPathSeperator)

LibAPI(MgErr) Win32ResolveShortCut(WStrPtr wStr, WStrPtr *wTgt, int32 *bufLen, Bool32 recursive, DWORD *dwAttrs)
{
	HRESULT err = noErr;
	IShellLinkW* psl;
	IPersistFile* ppf;
	WIN32_FIND_DATAW fileData;
	WCHAR *extStr = L".LNK";
	WCHAR tempPath[MAX_PATH], *srcPath = WStrBuf(wStr);
	int32 len = WStrLen(wStr), extLen = (int32)wcslen(extStr);

	// Don't bother trying to resolve shortcut if it doesn't have a ".lnk" extension.
	if (len < extLen)
		return cancelError;
	if (_wcsicmp(srcPath + len - extLen, extStr))
		return cancelError;

	// Get a pointer to the IShellLink interface.
	err = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void**)&psl);
	if (SUCCEEDED(err))
	{
		// Get a pointer to the IPersistFile interface.
		err = IShellLinkW_QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
		while (SUCCEEDED(err))
		{
			// Load the shortcut.
			err = IPersistFile_Load(ppf, srcPath, STGM_READ);
			if (SUCCEEDED(err))
			{
				// Resolve the shortcut.
				err = IShellLinkW_Resolve(psl, NULL, SLR_NO_UI);
				if (SUCCEEDED(err))
				{
					fileData.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
					err = IShellLinkW_GetPath(psl, tempPath, MAX_PATH, &fileData, SLGP_SHORTPATH); 
					if (SUCCEEDED(err))
					{
						*dwAttrs = fileData.dwFileAttributes;
						if (err == S_OK)
						{
							len = (int32)wcslen(tempPath);
							if (recursive && len >= extLen && !_wcsicmp(tempPath + len - extLen, extStr))
							{
								srcPath = tempPath;
								continue;
							}
							if (*dwAttrs == INVALID_FILE_ATTRIBUTES)
								*dwAttrs = GetFileAttributesW(tempPath);
							
							if (wTgt)
							{
								if (*wTgt && *bufLen <= len)
								{
									DSDisposePtr((UPtr)*wTgt);
									*wTgt = NULL;
								}
								if (!*wTgt)
								{
									*bufLen = len + 10;
									*wTgt = (WStrPtr)DSNewPClr(sizeof(int32) + (*bufLen) * sizeof(WCHAR));
								}
								if (*wTgt)
								{
									wcscpy(WStrBuf(*wTgt), tempPath);
									WStrLenSet(*wTgt, len);
								}
								else
								{
									err = E_OUTOFMEMORY;
								}
							}
						}
						else
						{
							/* Path couldn't be retrieved, is there any other way we can get the info? */
							fileData.cFileName;
						}
					}
				}
			}
			// Release pointer to IPersistFile interface.
			IPersistFile_Release(ppf);
			break;
		}
		// Release pointer to IShellLink interface.
		IShellLinkA_Release(psl);
	}
	return HRESULTToLVErr(err);
}

static BOOL Win32ModifyBackupPrivilege(BOOL fEnable)
{
	HANDLE handle;
	BOOL success = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &handle);
	if (success)
	{
		TOKEN_PRIVILEGES tokenPrivileges;
		// NULL for local system
		success = LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &tokenPrivileges.Privileges[0].Luid);
		if (success)
		{
			tokenPrivileges.PrivilegeCount = 1;
			tokenPrivileges.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;
			success = AdjustTokenPrivileges(handle, FALSE, &tokenPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
		}
		CloseHandle(handle);
	}
	return success;
}

LibAPI(MgErr) Win32ResolveLink(WStrPtr wSrc, WStrPtr *wTgt, int32 *bufLen, Bool32 recursive, DWORD *dwAttrs)
{
	HANDLE handle;
	MgErr err = noErr;
	int32 length, parentLen = 0, offset;
	DWORD bytes = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
	PREPARSE_DATA_BUFFER buffer = NULL;
	WStrPtr tempPath = NULL, wIntermediate = wSrc;

	*dwAttrs = GetFileAttributes(wSrc);
	if (*dwAttrs == INVALID_FILE_ATTRIBUTES)
	{
		return Win32GetLVFileErr();
	}

	// Need to acquire backup privileges in order to be able to retrieve a handle to a directory below or call symlink kernel entry points
	Win32ModifyBackupPrivilege(TRUE);
	while (!err && *dwAttrs & FILE_ATTRIBUTE_REPARSE_POINT)
	{
		// Open the link file or directory
		handle = CreateFile(wIntermediate, GENERIC_READ, 0 /*FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE */, 
			                 NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (handle == INVALID_HANDLE_VALUE)
		{
			err = Win32GetLVFileErr();
		}

		if (!err)
		{
			if (!buffer)
				buffer = (PREPARSE_DATA_BUFFER)DSNewPtr(bytes);
			if (!DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0, buffer, bytes, &bytes, NULL))
				err = Win32GetLVFileErr();
			else if (bytes < 9)
				err = fEOF;

			// Close the handle to our file so we're not locking it anymore.
			CloseHandle(handle);
		}

		if (!err)
		{
			BOOL relative = FALSE;
			LPWSTR start = NULL;

			switch (buffer->ReparseTag)
			{
				case IO_REPARSE_TAG_SYMLINK:
					start = (LPWSTR)((char*)buffer->SymbolicLinkReparseBuffer.PathBuffer + buffer->SymbolicLinkReparseBuffer.SubstituteNameOffset);
					length = buffer->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
					relative = buffer->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE;
					break;
				case IO_REPARSE_TAG_MOUNT_POINT:
					start = (LPWSTR)((char*)buffer->MountPointReparseBuffer.PathBuffer + buffer->MountPointReparseBuffer.SubstituteNameOffset);
					length = buffer->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
					break;
				default:
					length = 0;
					err = mgArgErr;
					break;
			}
			if (length)
			{
				parentLen = 0;
				if (length > 4 && Win32HasDOSDevicePrefix(start))
				{
					relative = FALSE;
					start += 4;
				}
				else if (!relative)
				{
					length += 4;
				}

				if (relative)
				{
					/* Shorten the path to the parent directory */
					wchar_t *ptr = WStrBuf(wIntermediate);
					parentLen = WStrLen(wIntermediate);
					while (parentLen && ptr[--parentLen] != kPathSeperator);
					parentLen++;
					length += parentLen;
				}
					
				tempPath = (WStrPtr)DSNewPtr(sizeof(int32) + sizeof(WCHAR) * ++length);
				if (tempPath)
				{
					if (relative)
					{
						WStrNCat(tempPath, 0, parentLen, WStrBuf(wIntermediate), WStrLen(wIntermediate));
						offset = parentLen;
					}
					else
					{
						WStrNCat(tempPath, 0, length, L"\\\\?\\", 4);
						offset = 4;
					}
					if (wIntermediate != wSrc)
						DSDisposePtr((UPtr)wIntermediate);

					WStrNCat(tempPath, offset, length - offset, start, length - offset);

					*dwAttrs = GetFileAttributes(tempPath);
					if (!recursive)
						break;

					if (*dwAttrs == INVALID_FILE_ATTRIBUTES)
						err = Win32GetLVFileErr();

					wIntermediate = tempPath;
				}
				else
				{
					err = mFullErr;
				}
			}
		}
	}
	Win32ModifyBackupPrivilege(FALSE);
	DSDisposePtr((UPtr)buffer);
	if (!err && wTgt)
	{
		DSDisposePtr((UPtr)*wTgt);
		*wTgt = tempPath;
		return noErr;
	}
	else
	{
		DSDisposePtr((UPtr)tempPath);
	}
	return err;
}
#endif

#if Win32 && !EMBEDDED
static uInt32 PtrHasRezExt(WCHAR *ptr, int32 len)
#else
static uInt32 PtrHasRezExt(char *ptr, int32 len)
#endif
{
	int32 k = 1;
#if Win32 && !EMBEDDED
	if (len < 0)
		len = (int32)wcslen(ptr);
#else
	if (len < 0)
		len = strlen(ptr);
#endif
	/* look backwards	*/
	for ( ; k < len && k < kMaxFileExtLength; --ptr, k++)
	{
		if (*ptr == '.')
		{
			uChar str[16];
            PStrLen(str) = (uChar)k;
            for (len = 1; k; k--)
				str[len++] = (uChar)*ptr++;
			return PStrHasRezExt(str);
		}
	}
	return 0;
}

static MgErr LWAppendPathSeparator(LWStrPtr pathName, int32 bufLen)
{
	if (LWStrBuf(pathName)[LWStrLen(pathName) - 1] != kPathSeperator)
	{
		if (LWStrLen(pathName) + 1 >= bufLen)
		{
			return mgArgErr;
		}
		LWStrBuf(pathName)[LWStrLen(pathName)] = kPathSeperator;
		LWStrBuf(pathName)[LWStrLen(pathName) + 1] = 0;
	}
	return noErr;
}

static uInt32 LWHasRezExt(LWStrPtr pathName)
{
	return PtrHasRezExt(LWStrBuf(pathName), LWStrLen(pathName));
}

/* Windows: path is an UTF8 encoded path string and lwstr is filled with a Windows UTF16LE string from the path
   MacOSX, Unix, VxWorks, Pharlap: path is an UTF8 encoded path string and lwstr is filled with a local encoded MBC
   string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
static MgErr MakeFileDSString(LStrHandle path, LWStrPtr *lwstr, int32 *reserve)
{
	return makeFileDSString(path, CP_UTF8, lwstr, reserve);
}

/* Windows: path is a LabVIEW path and lwstr is filled with a Windows UTF16LE string from this path
   MacOSX, Unix, VxWorks, Pharlap: path is a LabVIEW pathand lwstr is filled with a local encoded MBC
   string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
static int32 MakePathDSString(Path path, LWStrPtr *lwstr, int32 *reserve)
{
	LStrPtr lstr;
    int32 bufLen = -1;
    MgErr err = FPathToText(path, (LStrPtr)&bufLen);
    if (!err)
    {
		bufLen += 1 + (reserve ? *reserve : 0);
        lstr = (LStrPtr)DSNewPClr(sizeof(int32) + bufLen);
        if (!lstr)
            return mFullErr;
        LStrLen(lstr) = bufLen;
        err = FPathToText(path, lstr);
        if (!err)
			err = makeFileDSString(&lstr, CP_ACP, lwstr, reserve);

		DSDisposePtr((UPtr)lstr);
    }
    return err;
}

/* Internal list directory function
   On Windows the pathName is a wide char Long String pointer and the function returns UTF8 encoded filenames in the name array
   On other platforms it uses whatever is the default encoding for both pathName and the filenames, which could be UTF8 (Linux and Mac)
 */
static MgErr lvFile_ListDirectory(LWStrPtr pathName, int32 bufLen, LStrArrHdl *nameArr, FileInfoArrHdl *typeArr, int32 flags)
{
	MgErr err;
	int32 rootLength, type, index = 0, size = 8;
#if Win32
	HANDLE dirp = INVALID_HANDLE_VALUE;
	DWORD dwAttrs;
#if EMBEDDED
	LPSTR fileName = NULL;
	WIN32_FIND_DATAA fileData;
#else
	PVOID oldRedirection = NULL;
	WStrPtr wTgt = NULL;
	LPWSTR fileName = NULL;
	WIN32_FIND_DATAW fileData;
#endif
#else // Unix, VxWorks, and MacOSX
	char *path;
    struct stat statbuf;
	DIR *dirp;
	struct dirent *dp;
#if !VxWorks    /* no links */
    struct stat tmpstatbuf;
#endif
#endif

#if Win32
	err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, size);
	if (err)
		return err;

	err = NumericArrayResize(uQ, 1, (UHandle*)typeArr, size);
	if (err)
		return err;

	if (!pathName || !LWStrLen(pathName))
	{
		DWORD drives = GetLogicalDrives();
		int drive = 0;
	    for (; !err && drive < 26; drive++)
		{
            if (drives & 01)
		    {
				if (index >= size)
				{
					size *= 2;
					err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, size);
					if (err)
						return err;

					err = NumericArrayResize(uQ, 1, (UHandle*)typeArr, size);
					if (err)
						return err;
				}
				(**typeArr)->elm[index].flags = 0;
				(**typeArr)->elm[index].type = 0;

				err = NumericArrayResize(uB, 1, (UHandle*)((**nameArr)->elm + index), 1);
				if (!err)
				{
					*LStrBuf(*((**nameArr)->elm[index])) = (uChar)('A' + drive);
					LStrLen(*((**nameArr)->elm[index])) = 1;
				}
				index++;
				(**nameArr)->numItems = index;
				(**typeArr)->numItems = index;
		    }
	        drives >>= 1;
		}
		return err;
	}

	/* Check that we have actually a folder */
	dwAttrs = GetFileAttributes(pathName);
	if (dwAttrs == INVALID_FILE_ATTRIBUTES)
		return Win32GetLVFileErr();

	if (!(dwAttrs & FILE_ATTRIBUTE_DIRECTORY))
		return mgArgErr;

	err = LWAppendPathSeparator(pathName, bufLen);
	if (err)
		goto FListDirOut;

	rootLength = LWStrLen(pathName);
	err = LWStrNCat(pathName, rootLength, bufLen, strWildcardPattern, 3);
	if (err)
		goto FListDirOut;

#if !EMBEDDED
	if (!Wow64DisableWow64FsRedirection(&oldRedirection))
		/* Failed but lets go on anyhow, risking strange results for virtualized paths */
		oldRedirection = NULL;
#endif
	dirp = FindFirstFile(pathName, &fileData);
	if (dirp == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			(**typeArr)->numItems = 0;
		}
		else
		{
			err = fNotFound;
		}
		goto FListDirOut;
	}

	do
	{
		/* Skip the current dir, and parent dir entries */
		if (fileData.cFileName[0] != '.' || (fileData.cFileName[1] != 0 && (fileData.cFileName[1] != '.' || fileData.cFileName[2] != 0)))
		{
			/* Make sure our arrays are resized to allow the new values */
			if (index >= size)
			{
				size *= 2;
				err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, size);
				if (err)
					goto FListDirOut;

				err = NumericArrayResize(uQ, 1, (UHandle*)typeArr, size);
				if (err)
					goto FListDirOut;
			}

			(**typeArr)->elm[index].flags = 0;
			(**typeArr)->elm[index].type = 0;
			fileName = fileData.cFileName;

			/* Create the path to the file for intermediate operations */
			err = LWStrNCat(pathName, rootLength, bufLen, fileName, -1);
			if (err)
				goto FListDirOut;

			dwAttrs = fileData.dwFileAttributes;
			if (dwAttrs != INVALID_FILE_ATTRIBUTES)
			{
#if !EMBEDDED
				if (dwAttrs & FILE_ATTRIBUTE_REPARSE_POINT)
				{
					(**typeArr)->elm[index].flags |= kIsLink;
				}
				else
				{
					if (!Win32ResolveShortCut(pathName, NULL, NULL, FALSE, &dwAttrs))
						(**typeArr)->elm[index].flags |= (kIsLink | kIsFile);
				}
#endif
				if (!(dwAttrs & FILE_ATTRIBUTE_DIRECTORY))
				{
					(**typeArr)->elm[index].flags |= kIsFile;
#if EMBEDDED
					type = PtrHasRezExt(fileName, -1);
#else
					type = PtrHasRezExt(wTgt ? LWStrBuf(wTgt) : fileName, -1);
#endif
					if (type)
					{
						(**typeArr)->elm[index].flags |= kRecognizedType;
						(**typeArr)->elm[index].type = type;
					}
				}
				if (dwAttrs & FILE_ATTRIBUTE_HIDDEN)
				{
					(**typeArr)->elm[index].flags |= kFIsInvisible;
				}
			}
			else
			{
				/* can happen if file disappeared since we did FindNextFile */
				(**typeArr)->elm[index].flags |= kErrGettingType | kIsFile;
			}
#if EMBEDDED
			err = ConvertCString(fileName, -1, CP_ACP, (**nameArr)->elm + index, CP_ACP, 0, NULL);
#else
			err = WideCStrToMultiByte(fileName, -1, (**nameArr)->elm + index, CP_UTF8, 0, NULL);
#endif
			index++;
			(**nameArr)->numItems = index;
			(**typeArr)->numItems = index;
		}
	}
	while (FindNextFile(dirp, &fileData));

	if (!err)
		err = Win32GetLVFileErr();

FListDirOut:
#if !EMBEDDED
	DSDisposePtr((UPtr)wTgt);
	if (oldRedirection)
		Wow64RevertWow64FsRedirection(&oldRedirection);
#endif
	if (dirp != INVALID_HANDLE_VALUE)
		FindClose(dirp);
	if (index < size)
	{
		if (*nameArr || index > 0)
			NumericArrayResize(uPtr, 1, (UHandle*)nameArr, index);
		if (*typeArr || index > 0)
			NumericArrayResize(uQ, 1, (UHandle*)typeArr, index);
	}
#else
	if (!pathName || !LWStrLen(pathName))
		path = "/";
	else
		path = SStrBuf(pathName):

	/* Check that we have actually a folder */
    if (lstat(path, &statbuf))
		return UnixToLVFileErr();

	if (!S_ISDIR(statbuf.st_mode))
		return mgArgErr;

	err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, size);
	if (err)
		return err;

	err = NumericArrayResize(uQ, 1, (UHandle*)typeArr, size);
	if (err)
		return err;

	if (!(dirp = opendir(path))))
		return UnixToLVFileErr();

	/* Skip . and .. directory entries, they're always the first two. */
	(void)readdir(dirp);
	(void)readdir(dirp);

	err = LWAppendPathSeparator(pathName, bufLen);
	if (err)
		goto FListDirOut;

	rootLength = LWStrLen(pathName);

	for (dp = readdir(dirp), index = 1; dp; dp = readdir(dirp), index++)
	{
		/* Skip the current dir, and parent dir entries */
		if (dp->d_name[0] != '.' || (dp->d_name[1] != 0 && (dp->d_name[1] != '.' || dp->d_name[2] != 0)))
		{
			if (index >= size)
			{
				size *= 2;
				err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, size);
				if (err)
			        goto FListDirOut;

				err = NumericArrayResize(uQ, 1, (UHandle*)typeArr, size);
				if (err)
			        goto FListDirOut;
			}

			err = ConvertCString(dp->d_name, -1, CP_ACP, (**nameArr)->elm + index, CP_ACP, 0, NULL);
			if (err)
			    goto FListDirOut;
	
			err = LStrNCat(pathName, rootLength, bufLen, dp->d_name);
			if (err)
			    goto FListDirOut;

			if (lstat(SStrBuf(pathName), &statbuf))
			{
				(**typeArr)->elm[index].flags |= kErrGettingType;
			}
			else
			{
#if !VxWorks    /* no links */
				if (S_ISLNK(statbuf.st_mode))
				{
					(**typeArr)->elm[index].flags |= kIsLink;
					if (stat(thePath, &tmpstatbuf) == 0)	/* If link points to something */
						statbuf = tmpstatbuf;				/* return info about it not link. */
				}
#endif
				if (!S_ISDIR(statbuf.st_mode))
				{
					(**typeArr)->elm[index].flags |= kIsFile;
                    if (type = PtrHasRezExt(SStrBuf(pathName, LStrLen(pathName))))
					{
						(**typeArr)->elm[index].flags |= kRecognizedType;
						(**typeArr)->elm[index].type = type;
					}
				}
			}
			index++;
			(**nameArr)->numItems = index;
			(**typeArr)->numItems = index;
		}
	}
FListDirOut:
	closedir(dirp);
#endif
	return err;
}

/* On Windows, folderPath is a UTF8 encoded string, on other platforms it is locally encoded */
LibAPI(MgErr) LVFile_ListDirectory(LStrHandle folderPath, LStrArrHdl *nameArr, FileInfoArrHdl *typeArr, int32 flags)
{
	MgErr err = mgArgErr;
	LWStrPtr pathName = NULL;
	int32 bufLen = 0;
#if Win32
	if (LStrLenH(folderPath))
#endif
	{
		bufLen = 260;
		err = MakeFileDSString(folderPath, &pathName, &bufLen);
		if (err)
			return err;
	}
	err = lvFile_ListDirectory(pathName, bufLen, nameArr, typeArr, flags);
	DSDisposePtr((UPtr)pathName);
	return err;
}

LibAPI(MgErr) LVPath_ListDirectory(Path folderPath, LStrArrHdl *nameArr, FileInfoArrHdl *typeArr, int32 flags)
{
	MgErr err = mgArgErr;
	
	if (FIsAPath(folderPath))
	{
		LWStrPtr pathName = NULL;
		int32 bufLen = 0;
#if Win32
		if (FDepth(folderPath) > 0)
#endif
		{
			bufLen = 260;
			err = MakePathDSString(folderPath, &pathName, &bufLen);
			if (err)
				return err;
		}
		err = lvFile_ListDirectory(pathName, bufLen, nameArr, typeArr, flags);
		DSDisposePtr((UPtr)pathName);
	}
	return err;
}

LibAPI(MgErr) LVPath_HasResourceFork(Path path, LVBoolean *hasResFork, uInt32 *sizeLow, uInt32 *sizeHigh)
{
    MgErr  err = mgNoErr;
#if MacOSX
    LStrHandle lstr = NULL;
#else
    Unused(path);
#endif
    if (hasResFork)
	    *hasResFork = 0;
	if (sizeLow)
		*sizeLow = 0;
	if (sizeHigh)
		*sizeHigh = 0;

#if MacOSX
    err = MakePathDSString(path, &lstr, NULL);
    if (!err)
    {
        ssize_t len = getxattr((const char)LStrBuf(*lstr), XATTR_RESOURCEFORK_NAME, NULL, 0, 0, O_NOFOLLOW);
        if (len != -1)
        {
            if (hasResFork)
                *hasResFork = LV_TRUE;
            if (sizeLow)
                *sizeLow = Lo32(len);
            if (sizeHigh)
                *sizeHigh = Hi32(len);
           
        }
        DSDisposeHandle((UHandle)lstr);
    }
#endif
    return err;
}

#define kSetableWinFileFlags kWinFileInfoArchive | kWinFileInfoHidden | kWinFileInfoNotIndexed | \
				             kWinFileInfoOffline | kWinFileInfoReadOnly | kWinFileInfoSystem | kWinFileInfoTemporary

static uInt16 FlagsFromWindows(uInt16 attr)
{
	uInt16 flags = attr & kWinFileInfoReadOnly ?  0444 : 0666;
	if (attr & kWinFileInfoDirectory)
	{
		flags |= 040000;
	}
	else if (attr & kWinFileInfoDevice)
	{
		flags |= 060000;
	}
	else if (attr & kWinFileInfoReparsePoint)
	{
		flags |= 0120000;
	}
	else
	{
		flags |= 010000; 
	}
	return flags;
}

static uInt16 FlagsFromUnix(uInt32 mode)
{
	uInt16 flags = mode & 0222 ? 0 : kWinFileInfoReadOnly;
	switch (mode & 0170000)
	{
	     case 040000:
			flags |= kWinFileInfoDirectory;
			break;
	     case 020000:
	     case 060000:
			flags |= kWinFileInfoDevice;
			break;
	     case 0120000:
			flags |= kWinFileInfoReparsePoint;
			break;
	}
	if (!flags)
		flags = kWinFileInfoNormal;
	return flags;
}

static MgErr lvFile_FileInfo(LWStrPtr pathName, int32 bufLen, uInt8 write, LVFileInfo *fileInfo)
{
	MgErr err = noErr;
#if Win32 // Windows
    HANDLE handle = NULL;
	uInt64 count = 0;
#if EMBEDDED
	WIN32_FIND_DATAA fi = {0};
#else
	WIN32_FIND_DATAW fi = {0};
#endif
#else // Unix, VxWorks, and MacOSX
    struct stat statbuf;
	uInt64 count = 0;
#if VxWorks
    struct utimbuf times;
#else
	struct timespec times[2];
#endif
#endif

#if Win32
	/* FindFirstFile fails with empty path (desktop) or volume letter alone */
    if (!pathName || LWStrLen(pathName) <= 3)
    {
		fi.ftCreationTime.dwLowDateTime = fi.ftCreationTime.dwHighDateTime = 0;
		fi.ftLastWriteTime.dwLowDateTime = fi.ftLastWriteTime.dwHighDateTime = 0;
		fi.ftLastAccessTime.dwLowDateTime = fi.ftLastAccessTime.dwHighDateTime = 0;
		fi.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    }
    else
    {
		handle = FindFirstFile(pathName, &fi);
		if (handle == INVALID_HANDLE_VALUE)
			err = Win32GetLVFileErr();
		else
		{
			if (!FindClose(handle)) 
				err = Win32GetLVFileErr();
		}
    }

    if (!err)
    {
		if (write)
		{
			ATimeToFileTime(&fileInfo->cDate, &fi.ftCreationTime);
			ATimeToFileTime(&fileInfo->mDate, &fi.ftLastWriteTime);
			ATimeToFileTime(&fileInfo->aDate, &fi.ftLastAccessTime);
			handle = CreateFile(pathName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
			if (handle != INVALID_HANDLE_VALUE)
			{
			    if (!SetFileTime(handle, &fi.ftCreationTime, &fi.ftLastAccessTime, &fi.ftLastWriteTime))
				    err = Win32GetLVFileErr();
				CloseHandle(handle);
			}
			else
				err = Win32GetLVFileErr();

			if (!err)
			{
				fi.dwFileAttributes = GetFileAttributes(pathName);
				if (fi.dwFileAttributes != INVALID_FILE_ATTRIBUTES)
			    {
					if (!fileInfo->winFlags && fileInfo->unixFlags)
							fileInfo->winFlags = FlagsFromUnix(fileInfo->unixFlags);

					fi.dwFileAttributes = (fi.dwFileAttributes & ~kSetableWinFileFlags) | (fileInfo->winFlags & kSetableWinFileFlags);
                    if (!fi.dwFileAttributes)
						fi.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

				    SetFileAttributes(pathName, fi.dwFileAttributes);
			    }
			}
		}
		else
		{
			fileInfo->type = kUnknownFileType;
			fileInfo->creator = kUnknownCreator;
			fileInfo->uid = 0xFFFFFFFF;
			fileInfo->gid = 0xFFFFFFFF;
			FileTimeToATime(&fi.ftCreationTime, &fileInfo->cDate);
			FileTimeToATime(&fi.ftLastWriteTime, &fileInfo->mDate);
			FileTimeToATime(&fi.ftLastAccessTime, &fileInfo->aDate);
			fileInfo->rfSize = 0;
			fileInfo->winFlags = Lo16(fi.dwFileAttributes);
			fileInfo->unixFlags = FlagsFromWindows(fileInfo->winFlags);
			fileInfo->xtraFlags = 0;
			if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!pathName || !LWStrLen(pathName))
				{
					DWORD drives = GetLogicalDrives();
					int drive;

	                count = 0;
	                for (drive = 1; drive <= 26; drive++)
					{
                        if (drives & 01)
		                {
                            count++;
			            }
	                    drives >>= 1;
		            }
				}
				else
				{
				    count = 1;

				    err = LWStrNCat(pathName, 0, bufLen, strWildcardPattern, 3);
				    if (!err)
				    {
					    handle = FindFirstFile(pathName, &fi);
						if (handle == INVALID_HANDLE_VALUE)
						    count = 0;
					    else
						    while (FindNextFile(handle, &fi))
								count++;
					    FindClose(handle);
					}
				}
	            /* FindFirstFile doesn't enumerate . and .. entries in a volume root */
				if (!pathName || LWStrLen(pathName) < 3)
				    fileInfo->size = count;
				else
				    fileInfo->size = count - 2;
			}
			else
			{
				uInt32 type = LWHasRezExt(pathName);
				if (type)
				{
					fileInfo->type = type;
			        fileInfo->creator = kLVCreatorType;
				}
				else
				{
					fileInfo->type = kUnknownFileType;
					fileInfo->creator = kUnknownCreator;
				}
				fileInfo->size = Quad(fi.nFileSizeHigh, fi.nFileSizeLow);
			}
		}
    }
#else
    if (lstat(SStrBuf(pathName), &statbuf))
		return UnixToLVFileErr();

	if (write)
	{
		if (!fileInfo->unixFlags && fileInfo->winFlags)
			fileInfo->unixFlags = FlagsFromWindows(fileInfo->winFlags);
#if VxWorks
		VxWorksConvertFromATime(&fileInfo->cDate, &statbuf.st_ctime);
		VxWorksConvertFromATime(&fileInfo->aDate, &times.actime);
		VxWorksConvertFromATime(&fileInfo->mDate, &times.modtime);
		if (utime(SStrBuf(pathName), &times))
#else
		UnixConvertFromATime(&fileInfo->cDate, &statbuf.st_ctim);
		UnixConvertFromATime(&fileInfo->aDate, &times[0]);
		UnixConvertFromATime(&fileInfo->mDate, &times[1]);
		if (utimensat(0, SStrBuf(pathName), times, AT_SYMLINK_NOFOLLOW))
#endif
			err = UnixToLVFileErr();
#if !VxWorks
		/*
		 * Changing the ownership probably won't succeed, unless we're root
         * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
         * the mode; current BSD behavior is to remove all setuid bits on
         * chown. If chown fails, lose setuid/setgid bits.
         */
        else if (chown(SStrBuf(pathName), fileInfo->uid, fileInfo->gid))
		{
	        if (errno != EPERM && errno != ENOTSUP)
				err = UnixToLVFileErr();
	        fileInfo->unixFlags &= ~(S_ISUID | S_ISGID);
        }
#endif
        if (!err && chmod(SStrBuf(pathName), (statbuf.st_mode & 0170000) | (fileInfo->unixFlags & 07777)) && errno != ENOTSUP)
			err = UnixToLVFileErr();
#if MacOSX
		else if (chflags(SStrBuf(pathName), fileInfo->xtraFlags)
			err = UnixToLVFileErr();			
#endif
	}
	else
	{
		/* Try to determine LabVIEW file types based on file ending? */
		int32 type = LWHasRezExt(pathName);
		if (type)
		{
			fileInfo->type = type;
		    fileInfo->creator = kLVCreatorType;
		}
		else
		{
			fileInfo->type = kUnknownFileType;
			fileInfo->creator = kUnknownCreator;
		}
		fileInfo->uid = statbuf.st_uid;
		fileInfo->gid = statbuf.st_gid;
#if VxWorks
		VxWorksConvertToATime(statbuf.st_ctime, &fileInfo->cDate);
		VxWorksConvertToATime(statbuf.st_atime, &fileInfo->aDate);
		VxWorksConvertToATime(statbuf.st_mtime, &fileInfo->mDate);
#else
		UnixConvertToATime(&statbuf.st_ctim, &fileInfo->cDate);
		UnixConvertToATime(&statbuf.st_mtim, &fileInfo->mDate);
		UnixConvertToATime(&statbuf.st_atim, &fileInfo->aDate);
#endif
		if (S_ISDIR(statbuf.st_mode))
		{
			DIR *dirp;
			struct dirent *dp;

			if (!(dirp = opendir(SStrBuf(pathName))))
				return UnixToLVFileErr();

			for (dp = readdir(dirp); dp; dp = readdir(dirp))
				count++;
			closedir(dirp);
			fileInfo->size = count - 2;
			fileInfo->rfSize = 0;
		}
		else
		{
			fileInfo->size = statbuf.st_size;
#if MacOSX
			ssize_t size = getxattr(SStrBuf(pathName), XATTR_RESOURCEFORK_NAME, NULL, 0, 0, O_NOFOLLOW);
			if (size > 0)
			    fileInfo->rfSize = (uInt64)size;
			else
#else
				fileInfo->rfSize = 0;
#endif
		}
		fileInfo->unixFlags = Lo16(statbuf.st_mode);
		fileInfo->winFlags = FlagsFromUnix(fileInfo->unixFlags);
#if MacOSX
		fileInfo->xtraFlags = statbuf.st_flags;
#else
		fileInfo->xtraFlags = 0;
#endif
	}
#endif
    return err;
}

static Bool32 LStrIsAbsPath(LStrHandle filePath)
{
	int32 len = LStrLenH(filePath);
#if usesWinPath
	UPtr ptr = LStrBufH(filePath);
	if (!len || (len >= 3 && isalpha(ptr[0]) && ptr[1] == ':' && ptr[2] == kPathSeperator) 
#if !EMBEDDED
		|| (len >= 3 && ptr[0] == kPathSeperator && ptr[1] == kPathSeperator && isalpha(ptr[2]))
		|| (len > 4 && Win32HasDOSDevicePrefix(ptr))
#endif
		)
		return LV_TRUE;
#else
	if (!len || LStrBuf(*filePath)[0] == '/')
		return LV_TRUE;
#endif
	return LV_FALSE;
}

LibAPI(MgErr) LVFile_FileInfo(LStrHandle filePath, uInt8 write, LVFileInfo *fileInfo)
{
    MgErr err = mgNoErr;
    LWStrPtr pathName = NULL;
	int32 bufLen = 0;

    if (!LStrIsAbsPath(filePath))
        return mgArgErr;

	if (LStrLenH(filePath))
	{
		bufLen = 4;
		err = MakeFileDSString(filePath, &pathName, &bufLen);
		if (err)
			return err;
	}
    if (!err)
	{
		err = lvFile_FileInfo(pathName, bufLen, write, fileInfo);
		DSDisposePtr((UPtr)pathName);
	}
	return err;
}

LibAPI(MgErr) LVPath_FileInfo(Path filePath, uInt8 write, LVFileInfo *fileInfo)
{
    MgErr err = mgNoErr;
    LWStrPtr pathName = NULL;
	int32 bufLen = 4;

	if (!FIsAbsPath(filePath))
      return mgArgErr;

    err = MakePathDSString(filePath, &pathName, &bufLen);
    if (!err)
	{
		err = lvFile_FileInfo(pathName, bufLen, write, fileInfo);
		DSDisposePtr((UPtr)pathName);
	}
	return err;
}

/* 
   These two APIs will use the platform specific path syntax except for the
   MacOSX 32-bit plaform where it will use posix format
*/
LibAPI(MgErr) LVPath_ToText(Path path, LStrHandle *str)
{
	int32 pathLen = -1;
	MgErr err = FPathToText(path, (LStrPtr)&pathLen);
	if (!err)
	{
		err = NumericArrayResize(uB, 1, (UHandle*)str, pathLen + 1);
		if (!err)
		{
			err = FPathToText(path, **str);
			LStrLen(**str) = pathLen;
#if usesHFSPath
			if (!err)
				err = ConvertToPosixPath(*str, CP_ACP, str, CP_ACP, '?', NULL, false);
#endif
            if (LStrBuf(**str)[LStrLen(**str) - 1] == kPathSeperator)
            {
                LStrLen(**str)--;
            }
		}
	}
	return err;
}

LibAPI(MgErr) LVPath_FromText(CStr str, int32 len, Path *path, LVBoolean isDir)
{
	MgErr err = mgNoErr;
#if usesHFSPath
	LStrHandle hfsPath = NULL;
	/* Convert the posix path to an HFS path */
	err = ConvertFromPosixPath(str, len, CP_ACP, &hfsPath, CP_ACP, '?', NULL, isDir);
	if (!err && hfsPath)
	{
		err = FTextToPath(LStrBuf(*hfsPath), LStrLen(*hfsPath), path);
	}
#else
	Unused(isDir);
	err = FTextToPath(str, len, path);
#endif
	return err;
}

#if Win32 && !defined(EMBEDDED)
typedef BOOL (WINAPI *tCreateHardLinkW)(LPCWSTR lpFileName, LPCWSTR lpExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
MgErr Win32CreateHardLinkW(WStrPtr lwFileName, WStrPtr lwExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	MgErr err = mgNotSupported;
	static tCreateHardLinkW pCreateHardLinkW = NULL;
	if (!pCreateHardLinkW)
	{
		HMODULE hLib = LoadLibrary("kernel32.dll");
		if (hLib)
		{
			pCreateHardLinkW = (tCreateHardLinkW)GetProcAddress(hLib, "CreateHardLinkW");
		}
	}
	if (pCreateHardLinkW)
	{
		if (!pCreateHardLinkW(WStrBuf(lwFileName), WStrBuf(lwExistingFileName), lpSecurityAttributes))
			err = Win32GetLVFileErr();
		else
			err = noErr;
	}
	return err;
}

#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY
#define SYMBOLIC_LINK_FLAG_DIRECTORY (0x1)
#endif

static BOOL AcquireSymlinkPriv(void)
{
	HANDLE hToken;
	TOKEN_PRIVILEGES TokenPriv;
	BOOL result;

	if (!LookupPrivilegeValue(NULL, SE_CREATE_SYMBOLIC_LINK_NAME, &TokenPriv.Privileges[0].Luid))
		// This privilege does not exist before Windows XP
		return TRUE;

	TokenPriv.PrivilegeCount = 1;
	TokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
		return FALSE;

	result = AdjustTokenPrivileges(hToken, FALSE, &TokenPriv, 0, NULL, NULL) && GetLastError() == ERROR_SUCCESS;
	CloseHandle(hToken);

	return result;
}

typedef BOOL (WINAPI *tCreateSymbolicLinkW)(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);

MgErr Win32CreateSymbolicLinkW(WStrPtr lwSymlinkFileName, WStrPtr lwTargetFileName, LPSECURITY_ATTRIBUTES lpsa, DWORD dwFlags)
{
	MgErr err = mgNotSupported;
	static tCreateSymbolicLinkW pCreateSymbolicLinkW = NULL;
	if (!pCreateSymbolicLinkW)
	{
		HMODULE hLib = LoadLibrary("kernel32.dll");
		if (hLib)
		{
			pCreateSymbolicLinkW = (tCreateSymbolicLinkW)GetProcAddress(hLib, "CreateSymbolicLinkW");
		}
	}
	if (!(dwFlags & 0x8000) && pCreateSymbolicLinkW)
	{
		if (!pCreateSymbolicLinkW(WStrBuf(lwSymlinkFileName), WStrBuf(lwTargetFileName), dwFlags))
			err = Win32GetLVFileErr();
		else
			err = noErr;
	}
	else
	{
		BOOL isRelative = FALSE, isDirectory = FALSE;
		HANDLE hFile;
		BOOL (WINAPI *deletefunc)();
		DWORD attr = GetFileAttributes(lwTargetFileName);
		if (attr != INVALID_FILE_ATTRIBUTES)
			isDirectory = (attr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
		else
			isDirectory = (dwFlags & SYMBOLIC_LINK_FLAG_DIRECTORY) == SYMBOLIC_LINK_FLAG_DIRECTORY;

	    if (isDirectory)
		{
		    if (!CreateDirectoryW(WStrBuf(lwSymlinkFileName), lpsa))
			{
				return Win32GetLVFileErr();
			}
			hFile = CreateFile(lwSymlinkFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
			deletefunc = RemoveDirectoryW;
		}
		else
		{
			hFile = CreateFile(lwSymlinkFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, lpsa, CREATE_NEW, 0, NULL);
			deletefunc = DeleteFileW;
	    }
		if (hFile != INVALID_HANDLE_VALUE)
		{
			int32 length = WStrLen(lwTargetFileName) + 1, offset = 0;
			LPWSTR lpTargetFileName = WStrBuf(lwTargetFileName);
			WCHAR namebuf[MAX_PATH + 6];
			DWORD bytes = (DWORD)(REPARSE_DATA_BUFFER_HEADER_SIZE + length * sizeof(WCHAR) * 2 + 20);
			PREPARSE_DATA_BUFFER buffer = (PREPARSE_DATA_BUFFER)DSNewPClr(bytes);
			if (buffer)
			{
				wcscpy(namebuf, L"\\\\?\\");
				if (length > 4 && length < MAX_PATH + 4 && Win32HasDOSDevicePrefix(lpTargetFileName))
				{
					offset = 4;
				}
				if (isalpha(lpTargetFileName[offset]) && lpTargetFileName[offset + 1] == ':')
				{
					if (!GetFullPathNameW(lpTargetFileName + offset, sizeof(namebuf) / sizeof(namebuf[0]) - 4, namebuf + 4, NULL))
					{
						err = Win32GetLVFileErr();
						CloseHandle(hFile);
						return err;
					}
				}
				else if (!offset && lpTargetFileName[0] == kPathSeperator && lpTargetFileName[1] == kPathSeperator && isalpha(lpTargetFileName[2]))
				{
					if (!GetFullPathNameW(lpTargetFileName, sizeof(namebuf) / sizeof(namebuf[0]) - 6, namebuf + 6, NULL))
					{
						err = Win32GetLVFileErr();
						CloseHandle(hFile);
						return err;
					}
					wcscpy(namebuf + 4, L"UNC\\");
				}
				else if (offset == 4 && (CompareStringW(LOCALE_SYSTEM_DEFAULT, 0, lpTargetFileName + offset, 4, L"UNC\\", 4) == CSTR_EQUAL))
				{
					if (!GetFullPathNameW(lpTargetFileName, sizeof(namebuf) / sizeof(namebuf[0]), namebuf, NULL))
					{
						err = Win32GetLVFileErr();
						CloseHandle(hFile);
						return err;
					}
				}
				else
				{
					size_t rem = wcslen(lpTargetFileName);
					LPCWSTR src = lpTargetFileName;
					LPWSTR tgt = namebuf, root = namebuf;
					while (rem)
					{
						do
						{
							if (rem >= 3 && src[0] == '.' && src[1] == '.' && src[2] == kPathSeperator)
							{
								if (tgt > root)
								{
									while (tgt > root && *--tgt != kPathSeperator);
									tgt++;
								}
								else
								{
									memcpy(tgt, src, 3 * sizeof(WCHAR));
									tgt += 3;
									root += 3;
								}
								src += 3;
								rem -= 3;
							}
							else if (rem >= 2 && src[0] == '.' && src[1] == kPathSeperator)
							{
								src += 2;
								rem -= 2;
							}
						}
						while (rem-- && (*tgt++ = *src++) != kPathSeperator);
					}
					*tgt = 0;
					isRelative = TRUE;
				}
				buffer->ReparseTag = IO_REPARSE_TAG_SYMLINK;
				buffer->Reserved = 0;
				buffer->SymbolicLinkReparseBuffer.PrintNameOffset = 0;
				buffer->SymbolicLinkReparseBuffer.PrintNameLength = (USHORT)(wcslen(lpTargetFileName) * sizeof(WCHAR));
	            
				memcpy((char *)buffer->SymbolicLinkReparseBuffer.PathBuffer + buffer->SymbolicLinkReparseBuffer.PrintNameOffset,
		               lpTargetFileName, buffer->SymbolicLinkReparseBuffer.PrintNameLength);

				buffer->SymbolicLinkReparseBuffer.SubstituteNameOffset = buffer->SymbolicLinkReparseBuffer.PrintNameOffset + buffer->SymbolicLinkReparseBuffer.PrintNameLength;
				buffer->SymbolicLinkReparseBuffer.SubstituteNameLength = (USHORT)(wcslen(namebuf) * sizeof(WCHAR));
	
				memcpy((char *)buffer->SymbolicLinkReparseBuffer.PathBuffer + buffer->SymbolicLinkReparseBuffer.SubstituteNameOffset,
		                namebuf, buffer->SymbolicLinkReparseBuffer.SubstituteNameLength);

				buffer->SymbolicLinkReparseBuffer.Flags = isRelative ? 1 : 0;
				buffer->ReparseDataLength = 12 + buffer->SymbolicLinkReparseBuffer.SubstituteNameOffset + buffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
				
				bytes = 8 + buffer->ReparseDataLength;
				if (!DeviceIoControl(hFile, FSCTL_SET_REPARSE_POINT, NULL, 0, buffer, bytes, &bytes, NULL))
				{
				    err = Win32GetLVFileErr();
					deletefunc(WStrBuf(lwSymlinkFileName));
				}
				DSDisposePtr((UPtr)buffer);
			}
			CloseHandle(hFile);
		}
	}
	return err;
}
#endif

LibAPI(MgErr) LVPath_CreateLink(Path path, Path target, uInt32 flags)
{
    MgErr err = mgNoErr;
    LWStrPtr src = NULL;
    LWStrPtr tgt = NULL;

    if (!FIsAbsPath(path))
        return mgArgErr;

#if Win32 && EMBEDDED
	return mgNotSupported;
#else
	err =  MakePathDSString(path, &src, NULL);
    if (!err)
    {
        err = MakePathDSString(target, &tgt, NULL);
        if (!err)
        {
#if MacOSX || Unix
            if (flags & kLinkHard)
            {
                if (link((const char*)LStrBuf(src), (const char*)LStrBuf(tgt)))
                    err = UnixToLVFileErr();
            }
            else
            {
                if (symlink((const char*)LStrBuf(src), (const char*)LStrBuf(tgt)))
                    err = UnixToLVFileErr();
            }
#elif Win32
            if (!err)
            {
				// Need to acquire backup privileges in order to be able to call symlink kernel entry points
				Win32ModifyBackupPrivilege(TRUE);
                if (flags & kLinkHard)
                {
					err = Win32CreateHardLinkW(src, tgt, NULL);
                }
				else 
				{
					err = Win32CreateSymbolicLinkW(src, tgt, NULL, flags & kLinkDir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
                }
 				Win32ModifyBackupPrivilege(FALSE);
           }
#endif
            DSDisposePtr((UPtr)tgt);
        }
        DSDisposePtr((UPtr)src);
    }
    return err;
#endif
}

LibAPI(MgErr) LVPath_ReadLink(Path path, Path *target, Bool32 recursive, uInt32 *fileType)
{
    MgErr err = mgNoErr;
    LWStrPtr src = NULL;

    if (!FIsAbsPath(path))
        return mgArgErr;

#if Win32 && EMBEDDED
	return mgNotSupported;
#else
    err = MakePathDSString(path, &src, NULL);
    if (!err)
    {
#if MacOSX || Unix
        struct stat st;
        char *buf = NULL, *ptr = NULL;
        int len = 0;

        if (lstat((const char*)LStrBuf(src), &st))
        {
            err = UnixToLVFileErr();
        }
        else if (S_ISLNK(st.st_mode))
        {
            len = st.st_size + 1;
            buf = malloc(len);
            if (!buf)
            {
                err = mFullErr;
            }
        }
        else
        {
            err = mgArgErr;
        }
        
        while (!err)
        {
            ssize_t retval = readlink((const char*)LStrBuf(src), buf, len);
            if (retval < 0)
            {
                err = UnixToLVFileErr();
            }
            else if (retval < len)
            {
                /* Is the link target a relative path */
                if (buf[0] != '/')
                {
                    ptr = realloc(buf, retval + LStrLen(src) + 1);
                    if (ptr)
                    {
                        buf = ptr;
                        memmove(buf + LStrLen(src) + 1, buf, retval);
                        memmove(buf, LStrBuf(src), LStrLen(src));
                        buf[LStrLen(src)] = '/';
                        retval += LStrLen(src) + 1;
                    }
                    else
                    {
                        err = mFullErr;
                    }
                }
                if (!err)
                {
                    err = LVPath_FromText((CStr)buf, retval, target, LV_FALSE);
                    if (!err && fileType)
                    {
                        if (lstat(buf, &st))
                        {
                            err = UnixToLVFileErr();
                        }
                        else if (S_ISLNK(st.st_mode))
                        {
                            if (stat(buf, &st))
                            {
                                err = UnixToLVFileErr();
                            }
                            else
                            {
                                *fileType = S_ISDIR(st.st_mode) ? kIsLink : kIsLink | kIsFile;
                            }
                        }
                        else
                        {
                            if (!S_ISDIR(st.st_mode))
                            {
                                *fileType = kIsFile;
                            }
                        }
                    }
                    break;
                }
            }
            else // retval >= len
            {
                len += len;
                ptr = realloc(buf, len);
                if (!ptr)
                {
                    err = mFullErr;
                }
                else
                {
                    buf = ptr;
                }
            }
        }
        if (buf)
        {
            free(buf);
        }
#else
		WStrPtr wTgt = NULL;
		int32 tgtLen = 0;
		DWORD dwAttr;

		err = Win32ResolveLink(src, &wTgt, &tgtLen, recursive, &dwAttr);
		*fileType = 0;
		if (!err && wTgt)
		{
			int32 offset = 0;
			LStrHandle handle = NULL;
			if (WStrLen(wTgt) > 4 && Win32HasDOSDevicePrefix(WStrBuf(wTgt)))
					offset += 4;

			err = WideCStrToMultiByte(WStrBuf(wTgt) + offset, WStrLen(wTgt) - offset, &handle, CP_ACP, 0, NULL);
			if (!err)
				err = FTextToPath(LStrBuf(*handle), LStrLen(*handle), target);
			if (handle)
				DSDisposeHandle((UHandle)handle);
		}
		if (!err)
		{
            if (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				*fileType = kIsLink;
			}
			else if (!(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
			{
				*fileType = kIsFile;
 			}
		}
        DSDisposePtr((UPtr)wTgt);
#endif
        DSDisposePtr((UPtr)src);
    }
    return err;
#endif
}

/*
   The lvfile internal functions work on the same file object as the LabVIEW internal functions.
   This is the file handle or file descriptor that a LabVIEW File Refnum wraps.
   MacOS and MacOSX 32-Bit: FSIORefNum (int)
   Windows: HANDLE
   Unix: FILE*
*/
static MgErr lvfile_CloseFile(FileRefNum ioRefNum)
{
	MgErr err = mgNoErr;
#if usesHFSPath
	err = OSErrToLVErr(FSCloseFork(ioRefNum));
#elif usesPosixPath
	if (fclose(ioRefNum))
	{
		err = UnixToLVFileErr();
	}
#elif usesWinPath
	if (!CloseHandle(ioRefNum))
	{
		err = Win32GetLVFileErr();
	}
#endif
	return err;
}

static MgErr lvfile_GetSize(FileRefNum ioRefNum, FileOffset *size)
{
#if usesPosixPath || usesWinPath
	FileOffset tell;
#if usesWinPath
    MgErr err;
#endif
#endif
	if (0 == ioRefNum)
		return mgArgErr;
	size->q = 0;
#if usesHFSPath
	return OSErrToLVErr(FSGetForkSize(ioRefNum, &(size->q)));
#elif usesPosixPath
	errno = 0;
	tell.q = ftello64(ioRefNum);
	if (tell.q == - 1)
	{
		return UnixToLVFileErr();
	}
	else if (fseeko64(ioRefNum, 0L, SEEK_END) == -1)
	{
		return UnixToLVFileErr();
	}
	size->q = ftello64(ioRefNum);
	if (size->q == - 1)
	{
		return UnixToLVFileErr();
	}
	if (fseeko64(ioRefNum, tell.q, SEEK_SET) == -1)
	{
		return UnixToLVFileErr();
	}
#elif usesWinPath
	tell.q = 0;
	tell.l.lo = SetFilePointer(ioRefNum, 0, (PLONG)&tell.l.hi, FILE_CURRENT);
	if (tell.l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32GetLVFileErr();
		if (err)
			return err;
	}
	size->l.lo = SetFilePointer(ioRefNum, 0, (PLONG)&size->l.hi, FILE_END);
	if (size->l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32GetLVFileErr();
		if (err)
			return err;
	}
	tell.l.lo = SetFilePointer(ioRefNum, tell.l.lo, (PLONG)&tell.l.hi, FILE_BEGIN);
	if (tell.l.lo == INVALID_SET_FILE_POINTER /* && GetLastError() != NO_ERROR */)
	{
		err = Win32GetLVFileErr();
		if (err)
			return err;
	}
#endif
	return mgNoErr;
}

static MgErr lvfile_SetSize(FileRefNum ioRefNum, FileOffset *size)
{
#if usesPosixPath || usesWinPath
#if usesWinPath
	MgErr err = mgNoErr;
#endif
	FileOffset tell;
#endif
	if (0 == ioRefNum)
		return mgArgErr;
	if (size->q < 0)
		return mgArgErr;
#if usesHFSPath
	return OSErrToLVErr(FSSetForkSize(ioRefNum, fsFromStart, size->q));
#elif usesPosixPath
	errno = 0;
	if (fflush(ioRefNum) != 0)
	{
		return fIOErr;
	}
	if (ftruncate64(fileno(ioRefNum), size->q) != 0)
	{
		return UnixToLVFileErr();
	}
	tell.q = ftello64(ioRefNum);
	if (tell.q == -1)
	{
		return UnixToLVFileErr();
	}
	if ((tell.q > size->q) && (fseeko64(ioRefNum, size->q, SEEK_SET) != 0))
	{
		return UnixToLVFileErr();
	}
#elif usesWinPath
    tell.q = 0;
	tell.l.lo = SetFilePointer(ioRefNum, 0, (PLONG)&tell.l.hi, FILE_CURRENT);
	if (tell.l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32GetLVFileErr();
		if (err)
			return err;
	}
	size->l.lo = SetFilePointer(ioRefNum, size->l.lo, (PLONG)&size->l.hi, FILE_BEGIN);
	if (size->l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32GetLVFileErr();
		if (err)
			return err;
	}
	if (SetEndOfFile(ioRefNum))
	{
		if (tell.q < size->q)
		{
			tell.l.lo = SetFilePointer(ioRefNum, tell.l.lo, (PLONG)&tell.l.hi, FILE_BEGIN);
			if (tell.l.lo == INVALID_SET_FILE_POINTER /* && GetLastError() != NO_ERROR */)
			{
				return Win32GetLVFileErr();
			}
		}
	}
	else
	{
		return Win32GetLVFileErr();
	}
#endif
	return mgNoErr;
}

static MgErr lvfile_SetFilePos(FileRefNum ioRefNum, FileOffset *offs, uInt16 mode)
{
#if usesHFSPath
	OSErr ret;
#elif usesPosixPath || usesWinPath
	FileOffset size, sought, tell;
#if usesWinPath
	MgErr err = mgNoErr;
#endif
#endif
	if (0 == ioRefNum)
		return mgArgErr;

	if ((offs->q == 0) && (mode == fCurrent))
		return noErr;
#if usesHFSPath
	ret = FSSetForkPosition(ioRefNum, mode, offs->q);
	if (ret == posErr)
	{
		ret = FSSetForkPosition(ioRefNum, fsFromStart, 0);
	}
	if (ret == eofErr)
	{
		ret = FSSetForkPosition(ioRefNum, fsFromLEOF, 0);
	}
	return OSErrToLVErr(ret);
#elif usesPosixPath
	errno = 0;
	if (mode == fCurrent)
	{
		tell.q = ftello64(ioRefNum);
		if (tell.q == -1)
		{
			return UnixToLVFileErr();
		}
		sought.q = tell.q + offs->q;
	}
	if (fseeko64(ioRefNum, 0L, SEEK_END) != 0)
	{
		return UnixToLVFileErr();
	}
	size.q = ftello64(ioRefNum);
	if (size.q == -1)
	{
		return UnixToLVFileErr();
	}
	if (mode == fStart)
	{
		sought.q = offs->q;
	}
	else /* fEnd */
	{
		sought.q = size.q + offs->q;
	}

	if (sought.q > size.q)
	{
		/* already moved to actual end of file above */
		return fEOF;
	}
	else if (sought.q < 0)
	{
		fseeko64(ioRefNum, 0L, SEEK_SET);
		return fEOF;
	}
	else if (fseeko64(ioRefNum, sought.q, SEEK_SET) != 0)
	{
		return UnixToLVFileErr();
	}
#elif usesWinPath
	size.l.lo = GetFileSize(ioRefNum, (LPDWORD)&size.l.hi);
	if (size.l.lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
	{
		err = Win32GetLVFileErr();
		if (err)
			return err;
	}

	if (mode == fStart)
	{
		sought.q = offs->q;
	}
	else if (mode == fCurrent)
	{
		tell.l.hi = 0;
		tell.l.lo = SetFilePointer(ioRefNum, 0, (PLONG)&tell.l.hi, FILE_CURRENT);
		if (tell.l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		{	
			err = Win32GetLVFileErr();
			if (err)
				return err;
		}
		sought.q = tell.q + offs->q;
	}
	else /* fEnd */
	{
		sought.q = size.q + offs->q;
	}
	if (sought.q > size.q)
	{
		SetFilePointer(ioRefNum, 0L, 0L, FILE_END);
		return fEOF;
	}
	else if (sought.q < 0)
	{
		SetFilePointer(ioRefNum, 0L, 0L, FILE_BEGIN);
		return fEOF;
	}
	else
	{
		sought.l.lo = SetFilePointer(ioRefNum, sought.l.lo, (PLONG)&sought.l.hi, FILE_BEGIN);
		if (sought.l.lo == INVALID_SET_FILE_POINTER /* && GetLastError() != NO_ERROR */)
		{
			return Win32GetLVFileErr();
		}
	}
#endif
	return mgNoErr;
}

static MgErr lvfile_GetFilePos(FileRefNum ioRefNum, FileOffset *tell)
{
	if (0 == ioRefNum)
		return mgArgErr;
#if usesHFSPath
	return OSErrToLVErr(FSGetForkPosition(ioRefNum, &tell->q));
#elif usesPosixPath
	errno = 0;
	tell->q = ftello64(ioRefNum);
	if (tell->q == -1)
	{
		return UnixToLVFileErr();
	}
#elif usesWinPath
	tell->l.hi = 0;
	tell->l.lo = SetFilePointer(ioRefNum, 0, (PLONG)&tell->l.hi, FILE_CURRENT);
	if (tell->l.lo == INVALID_SET_FILE_POINTER /* && GetLastError() != NO_ERROR */)
	{
		return Win32GetLVFileErr();
	}
#endif
	return mgNoErr;
}

static MgErr lvfile_Read(FileRefNum ioRefNum, uInt32 inCount, uInt32 *outCount, UPtr buffer)
{
    MgErr	err = mgNoErr;
#if usesPosixPath
	int actCount;
#elif usesWinPath
	DWORD actCount;
#endif

    if (0 == ioRefNum)
		return mgArgErr;
    if (outCount)
        *outCount = 0;

#if usesHFSPath
	err = OSErrToLVErr(FSRead(ioRefNum, (SInt32*)&inCount, buffer));
    if (!err && outCount)
        *outCount = inCount;
#elif usesPosixPath
	errno = 0;
	actCount = fread((char *)buffer, 1, inCount, ioRefNum);
	if (ferror(ioRefNum))
	{
		clearerr(ioRefNum);
        return fIOErr;
	}
	if (feof(ioRefNum))
	{
		clearerr(ioRefNum);
        err = fEOF;
	}
    if (outCount)
        *outCount = actCount;
#elif usesWinPath
	if (!ReadFile(ioRefNum, buffer, inCount, &actCount, NULL))
	{
		return Win32GetLVFileErr();
	}
    if (outCount)
        *outCount = actCount;
	if (actCount != inCount)
	{
		err = fEOF;
	}
#endif
    return err;
}

static MgErr lvfile_Write(FileRefNum ioRefNum, uInt32 inCount, uInt32 *outCount, UPtr buffer)
{
    MgErr err = mgNoErr;
#if usesPosixPath
	int actCount;
#elif usesWinPath
	DWORD actCount;
#endif

	if (0 == ioRefNum)
		return mgArgErr;
    if (outCount)
        *outCount = 0;

#if usesHFSPath
    err = OSErrToLVErr(FSWrite(ioRefNum, (SInt32*)outCount, buffer));
    if (!err && outCount)
        *outCount = inCount;
#elif usesPosixPath
    errno = 0;
    actCount = fwrite((char *)buffer, 1, inCount, ioRefNum);
    if (ferror(ioRefNum))
    {
        clearerr(ioRefNum);
        return fIOErr;
    }
    if (outCount)
        *outCount = actCount;
#elif usesWinPath
	if (!WriteFile(ioRefNum, buffer, inCount, &actCount, NULL))
    {
		return Win32GetLVFileErr();
    }
	if (outCount)
		*outCount = actCount;
#endif
    return err;
}

#if MacOSX
static char *namedResourceFork = "/..namedfork/rsrc";
#endif

static MgErr lvFile_OpenFile(FileRefNum *ioRefNum, LWStrPtr lwstr, int32 bufLen, uInt32 rsrc, uInt32 openMode, uInt32 denyMode)
{
    MgErr err = noErr;
	int32 type;
#if usesPosixPath
    struct stat statbuf;
    char theMode[4];
#elif usesWinPath
    DWORD shareAcc, openAcc;
    DWORD createMode = OPEN_EXISTING;
    int32 attempts = 3;
 #if !EMBEDDED
	wchar_t *rsrcPostfix = NULL;
 #endif
#endif
    
#if usesWinPath
 #if EMBEDDED
	if (rsrc)
		return mgArgErr;
 #else
	/* Try to access the possible NTFS alternate streams from Services For Macintosh (SFM) */
	switch (rsrc)
	{
		case kOpenFileRsrcData:
			break;
		case kOpenFileRsrcResource:
			rsrcPostfix = L":AFP_Resource";
			break;
		case kOpenFileRsrcInfo:
			rsrcPostfix = L":AFP_AfpInfo";
			break;
		case kOpenFileRsrcDesktop:
			rsrcPostfix = L":AFP_DeskTop";
			break;
		case kOpenFileRsrcIndex:
			rsrcPostfix = L":AFP_IdIndex";
			break;
		case kOpenFileRsrcComment:
			rsrcPostfix = L":Comments";
			break;
		default:
			return mgArgErr;
	}
 #endif

    switch (openMode)
    {
		case openReadOnly:
			openAcc = GENERIC_READ;
			break;
		case openWriteOnlyTruncate:
			createMode = TRUNCATE_EXISTING;
			/* Intentionally falling through */
		case openWriteOnly:
			openAcc = GENERIC_WRITE;
			break;
		case openReadWrite:
			openAcc = GENERIC_READ | GENERIC_WRITE;
			break;
		default:
			return mgArgErr;
    }

    switch (denyMode)
    {
		case denyReadWrite:
			shareAcc = 0;
			break;
		case denyWriteOnly:
			shareAcc = FILE_SHARE_READ;
			break;
		case denyNeither:
			shareAcc = FILE_SHARE_READ | FILE_SHARE_WRITE ;
			break;
		default:
			return mgArgErr;
    }

 #if !EMBEDDED
	if (rsrcPostfix)
		err = WStrNCat(lwstr, LWStrLen(lwstr), bufLen, rsrcPostfix, -1);
 #endif
	/* Open the specified file. */
    while (!err && attempts)
	{
		*ioRefNum = CreateFile(lwstr, openAcc, shareAcc, 0, createMode, FILE_ATTRIBUTE_NORMAL, 0);
		if (*ioRefNum == INVALID_HANDLE_VALUE)
		{
			DWORD error = GetLastError();
			if (error == ERROR_SHARING_VIOLATION)
		    {
			    if (--attempts > 0)
				    Sleep(50);
		    }
		    else
			{
				err = Win32ToLVFileErr(error);
				break;
			}
		}
		else
		{
			break;
		}
	}
	if (err)
		return err;

#elif usesPosixPath
 #if MacOSX
	if (rsrc > kOpenFileRsrcResource)
 #else
	if (rsrc)
 #endif
		return mgArgErr;

	switch (openMode)
	{
		case openWriteOnly:
			/* Treat write-only as read-write, since you can't open a file for write-only
			   using buffered i/o functions without truncating the file. */
		case openReadWrite:
			strcpy(theMode, "r+");
			break;
		case openReadOnly:
			strcpy(theMode, "r");
			break;
		case openWriteOnlyTruncate:
			strcpy(theMode, "w");
			break;
		default:
			return mgArgErr;
	}

	switch (denyMode)
	{
		case denyReadWrite:
		case denyWriteOnly:
		case denyNeither:
			break;
		default:
			return mgArgErr;
	}

 #if MacOSX
	if (rsrc == kOpenFileRsrcResource)
	{
		err = LStrNCat(lwstr), LStrLen(lwstr)), bufLen, namedResourceFork, -1);
		if (err)
			return err;
	}
 #endif

	/* Test for file existence first to avoid creating file with mode "w". */
	if (openMode == openWriteOnlyTruncate && lstat(SStrBuf(lwstr), &statbuf))
	{
		return fNotFound;
	}

	errno = 0;
	*ioRefNum = fopen(SStrBuf(lwstr), (char *)theMode);
	if (!*ioRefNum)
		return UnixToLVFileErr();

 #ifdef HAVE_FCNTL
	/* Implement deny mode by range locking whole file */
	if (denyMode == denyReadWrite || denyMode == denyWriteOnly)
	{
		struct flock lockInfo;

		lockInfo.l_type = (openMode == openReadOnly) ? F_RDLCK : F_WRLCK;
		lockInfo.l_whence = SEEK_SET;
		lockInfo.l_start = 0;
		lockInfo.l_len = 0;
		if (fcntl(fileno(*ioRefNum), F_SETLK, FCNTL_PARAM3_CAST(&lockInfo)) == -1)
		{
			err = UnixToLVFileErr();
		}
	}
 #endif
#elif !usesHFSPath
 #error No supported code path
#endif
	if (err)
	{
		lvfile_CloseFile(*ioRefNum);
		DEBUGPRINTF(((CStr)"OpenFile: err = %ld, rsrc = %d", err, (int16)rsrc));
	}
	return err;
}

LibAPI(MgErr) LVFile_OpenFile(LVRefNum *refnum, LStrHandle filePath, uInt32 rsrc, uInt32 openMode, uInt32 denyMode)
{
#if usesWinPath && !EMBEDDED
	int32 bufLen = rsrc ? 14 : 0;
#elif MacOSX
	int32 bufLen = rsrc ? strlen(namedResourceFork) : 0;
#else
	int32 bufLen = 0;
#endif
	LWStrPtr pathName = NULL;
	FileRefNum ioRefNum = NULL;
	MgErr err;

	if (!LStrIsAbsPath(filePath))
        return mgArgErr;

#if usesWinPath
	if (!LStrLenH(filePath))
		return mgArgErr;
#endif

	err = MakeFileDSString(filePath, &pathName, &bufLen);
	if (!err)
	{
		err = lvFile_OpenFile(&ioRefNum, pathName, bufLen, rsrc, openMode, denyMode);
		DSDisposePtr((UPtr)pathName);
		if (!err)
		{
			err = FNewRefNum(NULL, (File)(ioRefNum), refnum);
		}
	}
	return err;
}

#if usesHFSPath
static MgErr lvFile_OpenFileHFS(FileRefNum *ioRefNum, Path path, uInt32 rsrc, uInt32 openMode, uInt32 denyMode)
{
    FSRef ref;
	HFSUniStr255 forkName;
    int8 perm;
    OSErr ret;

	if (rsrc > kOpenFileRsrcResource)
		return mgArgErr;

	switch (openMode)
    {
	    case openReadWrite:
		    perm = fsRdWrPerm;
		    break;
	    case openReadOnly:
		    perm = fsRdPerm;
		    break;
    	case openWriteOnly:
	    case openWriteOnlyTruncate:
		    perm = fsWrPerm;
		    break;
	    default:
		    return mgArgErr;
    }

    switch (denyMode)
    {
	    case denyReadWrite:
		    perm |= fsRdDenyPerm | fsWrDenyPerm;
		    break;
		case denyWriteOnly:
			perm |= fsWrDenyPerm;
			break;
		case denyNeither:
			/* leave all deny mode bits clear */
			break;
		default:
			return mgArgErr;
	}

    err = FSMakePathRef(path, &ref);
    if (err)
	{
		DEBUGPRINTF(((CStr)"FSMakePathRef: err = %ld", err));
		return err;
	}

    if (rsrc)
	{
		ret = FSGetResourceForkName(&forkName);
	}
	else
	{
		ret = FSGetDataForkName(&forkName);
	}
	if (ret == noErr)
	{
		ret = FSOpenFork(&ref, forkName.length, forkName.unicode, perm, ioRefNum);
	}
	if (ret)
		return OSErrToLVErr(ret);

	if (openMode == openWriteOnlyTruncate)
	{
		FileOffset size;
		size.q = 0;
		err = lvfile_SetSize(ioRefNum, &size);
	}
	if (err)
	{
		lvfile_CloseFile(*ioRefNum);
		DEBUGPRINTF(((CStr)"OpenFile: err = %ld, rsrc = %d", err, (int16)rsrc));
	}
	return err;
}
#endif

LibAPI(MgErr) LVPath_OpenFile(LVRefNum *refnum, Path path, uInt32 rsrc, uInt32 openMode, uInt32 denyMode)
{
#if usesWinPath && !EMBEDDED
	int32 bufLen = rsrc ? 14 : 0;
#elif MacOSX
	int32 bufLen = rsrc ? strlen(namedResourceFork) : 0;
#else
	int32 bufLen = 0;
#endif
	int32 type;
	LWStrPtr pathName = NULL;
	FileRefNum ioRefNum = NULL;
	MgErr err;
	
	if (!FIsAbsPath(path))
		return mgArgErr;

#if usesHFSPath
	err = lvFile_OpenFileHFS(refnum, path, rsrc, openMode, denyMode);
#else
	err = MakePathDSString(path, &pathName, &bufLen);
	if (!err)
	{
		err = lvFile_OpenFile(&ioRefNum, pathName, bufLen, rsrc, openMode, denyMode);
		DSDisposePtr((UPtr)pathName);
	}
#endif
	if (!err)
		err = FNewRefNum(path, (File)(ioRefNum), refnum);
	return err;
}

LibAPI(MgErr) LVFile_CloseFile(LVRefNum *refnum)
{
	FileRefNum ioRefNum;
	MgErr err = FRefNumToFD(*refnum, (File*)&ioRefNum);
	if (!err)
	{
		err = lvfile_CloseFile(ioRefNum);
		FDisposeRefNum(*refnum);
	}
	return err;
}

LibAPI(MgErr) LVFile_GetSize(LVRefNum *refnum, FileOffset *size)
{
	FileRefNum ioRefNum;
	MgErr err = FRefNumToFD(*refnum, (File*)&ioRefNum);
	if (!err)
	{
		err = lvfile_GetSize(ioRefNum, size);
	}
	return err;
}

LibAPI(MgErr) LVFile_SetSize(LVRefNum *refnum, FileOffset *size)
{
	FileRefNum ioRefNum;
	MgErr err = FRefNumToFD(*refnum, (File*)&ioRefNum);
	if (!err)
	{
		err = lvfile_SetSize(ioRefNum, size);
	}
	return err;
}

LibAPI(MgErr) LVFile_SetFilePos(LVRefNum *refnum, FileOffset *offs, uInt16 mode)
{
	FileRefNum ioRefNum;
	MgErr err = FRefNumToFD(*refnum, (File*)&ioRefNum);
	if (!err)
	{
		err = lvfile_SetFilePos(ioRefNum, offs, mode);
	}
	return err;
}

LibAPI(MgErr) LVFile_GetFilePos(LVRefNum *refnum, FileOffset *offs)
{
	FileRefNum ioRefNum;
	MgErr err = FRefNumToFD(*refnum, (File*)&ioRefNum);
	if (!err)
	{
		err = lvfile_GetFilePos(ioRefNum, offs);
	}
	return err;
}

LibAPI(MgErr) LVFile_Read(LVRefNum *refnum, uInt32 inCount, uInt32 *outCount, UPtr buffer)
{
	FileRefNum ioRefNum;
	MgErr err = FRefNumToFD(*refnum, (File*)&ioRefNum);
	if (!err)
	{
		err = lvfile_Read(ioRefNum, inCount, outCount, buffer);
	}
	return err;
}

LibAPI(MgErr) LVFile_Write(LVRefNum *refnum, uInt32 inCount, uInt32 *outCount, UPtr buffer)
{
	FileRefNum ioRefNum;
	MgErr err = FRefNumToFD(*refnum, (File*)&ioRefNum);
	if (!err)
	{
		err = lvfile_Write(ioRefNum, inCount, outCount, buffer);
	}
	return err;
}

LibAPI(MgErr) InitializeFileFuncs(LStrHandle filefunc_def)
{
	MgErr err = NumericArrayResize(uB, 1, (UHandle*)&filefunc_def, sizeof(zlib_filefunc64_def));
	if (!err)
	{
		zlib_filefunc64_def* pzlib_filefunc_def = (zlib_filefunc64_def*)LStrBuf(*filefunc_def);
		LStrLen(*filefunc_def) = sizeof(zlib_filefunc64_def);
#if Win32
		fill_win32_filefunc64A(pzlib_filefunc_def);
#else
		fill_fopen64_filefunc(pzlib_filefunc_def);
#endif
	}
	return err;
}

LibAPI(MgErr) InitializeStreamFuncs(LStrHandle filefunc_def, LStrHandle *memory)
{
	MgErr err = NumericArrayResize(uB, 1, (UHandle*)&filefunc_def, sizeof(zlib_filefunc64_def));
	if (!err)
	{
		zlib_filefunc64_def* pzlib_filefunc_def = (zlib_filefunc64_def*)LStrBuf(*filefunc_def);
		LStrLen(*filefunc_def) = sizeof(zlib_filefunc64_def);

		fill_mem_filefunc(pzlib_filefunc_def, memory);
	}
	return err;
}

#if MacOSX
static CFStringEncoding ConvertCodepageToEncoding(uInt32 codePage)
{
	CFStringEncoding encoding = CFStringConvertWindowsCodepageToEncoding(codePage);
	if (encoding == kCFStringEncodingInvalidId)
	{
		encoding = CFStringGetSystemEncoding();
	}
	return encoding;
}
#endif

LibAPI(uInt32) GetCurrentCodePage(LVBoolean acp)
{
#if Win32
	return acp ? GetACP() : GetOEMCP();
#elif MacOSX
    Unused(acp);
	CFStringEncoding encoding = CFStringGetSystemEncoding();
	return CFStringConvertEncodingToWindowsCodepage(encoding);
#else
    if (utf8_is_current_mbcs())
		return CP_UTF8;
	return acp ? CP_ACP : CP_OEMCP;
#endif
}

LibAPI(LVBoolean) HasExtendedASCII(LStrHandle string)
{
	if (string)
	{
		int32 i = LStrLen(*string) - 1;
		UPtr ptr = LStrBuf(*string);

		for (; i >= 0; i--)
		{
			if (*ptr++ >= 0x80)
			{
				return LV_TRUE;
			}
		}
	}
	return LV_FALSE;
}

LibAPI(MgErr) ConvertCString(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed)
{
	MgErr err = noErr;
	if (srccp != destcp)
	{
		WStrHandle ustr = NULL;
		err = MultiByteCStrToWideString(src, srclen, &ustr, srccp);
		if (!err && ustr)
		{
			err = WideStringToMultiByte(ustr, dest, destcp, defaultChar, defUsed);
			DSDisposeHandle((UHandle)ustr);
			return err;
		}
	}
	else
	{
		if (srclen == -1)
			srclen = StrLen(src);
		if (srclen > 0)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)dest, srclen);
			if (!err)
			{
				MoveBlock(src, LStrBuf(**dest), srclen);
				LStrLen(**dest) = srclen;
				return err;
			}
		}
	}
	if (*dest)
		LStrLen(**dest) = 0;
	return err;
}

/* Converts a Unix style path to a LabVIEW platform path */
LibAPI(MgErr) ConvertFromPosixPath(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir)
{
    MgErr err = mgNotSupported;
#if usesWinPath
    Unused(isDir);
    err = ConvertCString(src, srclen, srccp, dest, destcp, defaultChar, defUsed);
	if (!err && *dest)
	{
		int32 len = LStrLen(**dest);
		UPtr buf = LStrBuf(**dest);
		if (src[0] == '/' && src[1] != '/')
		{
			*buf++ = src[1];
			*buf++ = ':';
			len -= 2;
		}
		for (; len; len--, buf++)
		{
			if (*buf == '/')
			{
				*buf = '\\';
			}
		}
	}
#elif usesPosixPath
    Unused(isDir);
	err = ConvertCString(src, srclen, srccp, dest, destcp, defaultChar, defUsed);
#elif usesHFSPath
    Unused(defUsed);
	CFStringEncoding encoding = ConvertCodepageToEncoding(srccp);
	if (encoding != kCFStringEncodingInvalidId)
	{
		CFURLRef urlRef = NULL;
		CFStringRef hfsRef, fileRef = CFStringCreateWithBytes(kCFAllocatorDefault, src, srclen, encoding, false);
		if (!fileRef)
		{
			return mFullErr;
		}
		urlRef = CFURLCreateWithFileSystemPath(NULL, fileRef, kCFURLPOSIXPathStyle, isDir);
		CFRelease(fileRef);
		if (!urlRef)
		{
			return mFullErr;
		}
		hfsRef = CFURLCopyFileSystemPath(urlRef, kCFURLHFSPathStyle);
		CFRelease(urlRef);
		if (hfsRef)
		{
			CFIndex len;
			CFRange range = CFRangeMake(0, CFStringGetLength(hfsRef));
			if (CFStringGetBytes(hfsRef, range, encoding, defaultChar, false, NULL, 0, &len) > 0)
			{
				if (len > 0)
				{
					err = NumericArrayResize(uB, 1, (UHandle*)dest, len + 1);
					if (!err)
					{
						encoding = ConvertCodepageToEncoding(destcp);
						if (encoding != kCFStringEncodingInvalidId)
						{
							if (CFStringGetBytes(hfsRef, range, encoding, 0, false, LStrBuf(**dest), len, &len) > 0)
							{
								LStrBuf(**dest)[len] = 0;
								LStrLen(**dest) = len;
								err = mgNoErr;
							}
							else
							{	
								err = bogusError;
							}
						}
					}
				}
			}
			CFRelease(hfsRef);
		}
		else
		{
			err = mFullErr;
		}
    }
#endif
	return err;
}

LibAPI(MgErr) ConvertLString(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed)
{
	MgErr err;
	if (srccp != destcp)
	{
		WStrHandle wstr = NULL;
		if (LStrLen(*src) > 0)
		{
			err = MultiByteToWideString(src, &wstr, srccp);
			if (!err && wstr)
			{
				err = WideStringToMultiByte(wstr, dest, destcp, defaultChar, defUsed);
				DSDisposeHandle((UHandle)wstr);
			}
			return err;
		}
	}
	else
	{
		err = NumericArrayResize(uB, 1, (UHandle*)dest, LStrLen(*src));
		if (!err)
		{
			MoveBlock((ConstCStr)*src, (CStr)**dest, LStrLen(*src) + sizeof(int32));
		}
		return err;
	}
	if (*dest)
		LStrLen(**dest) = 0;

	return mgNoErr;
}

#if !Unix || !defined(HAVE_ICONV)
static void TerminateLStr(LStrHandle *dest, int32 numBytes)
{
	LStrLen(**dest) = numBytes;
	LStrBuf(**dest)[numBytes] = 0;
}
#endif

/* Converts a LabVIEW platform path to Unix style path */
LibAPI(MgErr) ConvertToPosixPath(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir)
{
    MgErr err = mgNotSupported;
#if usesHFSPath
    Unused(defUsed);
	CFStringEncoding encoding = ConvertCodepageToEncoding(srccp);
	if (encoding != kCFStringEncodingInvalidId)
	{
		CFURLRef urlRef = NULL;
		CFStringRef posixRef, fileRef = CFStringCreateWithBytes(kCFAllocatorDefault, LStrBuf(*src), LStrLen(*src), encoding, false);
		if (!fileRef)
		{
			return mFullErr;
		}
		urlRef = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, fileRef, kCFURLHFSPathStyle, isDir);
		CFRelease(fileRef);
		if (!urlRef)
		{
			return mFullErr;
		}
		posixRef = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);
		CFRelease(urlRef);
		if (posixRef)
		{
			CFIndex len;
			CFRange range = CFRangeMake(0, CFStringGetLength(posixRef));
			if (CFStringGetBytes(posixRef, range, encoding, defaultChar, false, NULL, 0, &len) > 0)
			{
				if (len > 0)
				{
					err = NumericArrayResize(uB, 1, (UHandle*)dest, len + 1);
					if (!err)
					{
						encoding = ConvertCodepageToEncoding(destcp);
						if (encoding != kCFStringEncodingInvalidId)
						{
							if (CFStringGetBytes(posixRef, range, encoding, 0, false, LStrBuf(**dest), len, &len) > 0)
							{
								TerminateLStr(dest, len);
								err = mgNoErr;
							}
							else
							{	
								err = bogusError;
							}
						}
					}
				}
			}
			CFRelease(posixRef);
		}
		else
		{
			err = mFullErr;
		}
	}
#else
    Unused(isDir);
	err = ConvertLString(src, srccp, dest, destcp, defaultChar, defUsed);
#if usesWinPath
	if (!err && *dest)
	{
		int32 len = LStrLen(**dest);
		UPtr buf = LStrBuf(**dest);
		if (buf[1] == ':' && buf[2] == '\\')
		{
			*buf++ = '/';
			*buf++ = LStrBuf(*src)[0];
			len -= 2;
		}
		for (; len; len--, buf++)
		{
			if (*buf == '\\')
			{
				*buf = '/';
			}
		}
	}
#endif
#endif
	return err;
}

#if Unix
#ifdef HAVE_ICONV
static char *iconv_getcharset(uInt32 codePage)
{
	static char cp[10];
	switch (codePage)
	{
		case CP_ACP:
			return "CHAR";
		case CP_OEMCP:
			/* FIXME: determine current local and return according OEM */
			return "CP437";
		case CP_UTF8:
			return "UTF-8";
		default:
			snprintf(cp, 10, "CP%d", codePage);
			return cp;
	}
	return NULL;
}

static MgErr unix_convert_mbtow(const char *src, int32 len, WStrHandle *dest, uInt32 codePage)
{
	iconv_t cd = iconv_open("WCHAR_T", iconv_getcharset(codePage));
	if (cd != (iconv_t)-1)
	{
		MgErr err;
		if (len < 0)
			len = strlen(src);
		err = NumericArrayResize(uB, 1, (UHandle*)dest, len * sizeof(wchar_t));
		if (!err)
		{
			char *inbuf = (char*)src, *outbuf = (char*)LStrBuf(**dest);
			size_t retval, inleft = len, outleft = len * sizeof(wchar_t);
			retval = iconv(cd, &inbuf, &inleft, &outbuf, &outleft);
			if (retval == (size_t)-1)
				err = UnixToLVFileErr();
			else
			    WStrLenSet(**dest, (len * sizeof(wchar_t) - outleft) / sizeof(uInt16));
		}
		if (iconv_close(cd) != 0 && !err)
			err = UnixToLVFileErr();
		return err;
	}
	return UnixToLVFileErr();
}

static MgErr unix_convert_wtomb(const wchar_t *src, int32 srclen, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	MgErr err;
	iconv_t cd = iconv_open(iconv_getcharset(codePage), "WCHAR_T");
	Unused(defaultChar);
	Unused(defaultCharWasUsed);
	if (cd != (iconv_t)-1)
	{
		if (srclen == -1)
			srclen = wcslen(src);

		err = NumericArrayResize(uB, 1, (UHandle*)dest, srclen * sizeof(uInt16) * 2);
		if (!err)
		{
			char *inbuf = (char*)src, *outbuf = (char*)LStrBuf(**dest);
			size_t retval, inleft = srclen * sizeof(uInt16), outleft = srclen * sizeof(uInt16) * 2;
			retval = iconv(cd, &inbuf, &inleft, &outbuf, &outleft);
			if (retval == (size_t)-1)
				err = UnixToLVFileErr();
			else
				LStrLen(**dest) = srclen * 2 - outleft;
		}
		if (iconv_close(cd) != 0 && !err)
			err = UnixToLVFileErr();
		return err;
	}
	return UnixToLVFileErr();
}
#else // HAVE_ICONV
/* We don't have a good way of converting from an arbitrary character set to wide char and back.
   Just do default mbcs to wide char and vice versa ??? */
static MgErr unix_convert_mbtow(const char *src, int32 len, WStrHandle *dest, uInt32 codePage)
{
	int32 offset = 0;
	MgErr err = noErr;

	if (codePage == CP_UTF8)
        err = utf8towchar(src, len, NULL, &offset, 0);
	else
	{
		size_t max, length;
		char *pt = (char*)src;
#if HAVE_MBRTOWC
	    mbstate_t mbs;
#endif
		if (len < 0)
			len = strlen(src);
		max = len;
#if HAVE_MBRTOWC
	    length = mbrtowc(NULL, NULL, 0, &mbs);
	    while (max > 0)
	    {
		    length = mbrtowc(NULL, pt, max, &mbs);
		    if (length > 0)
		    {
			    pt += length;
			    max -= length;
			    offset++;
		    }
	    }
#else
	    length = mbtowc(NULL, NULL, 0);
	    while (max > 0)
	    {
		    length = mbtowc(NULL, pt, max);
		    if (length > 0)
		    {
			    pt += length;
			    max -= length;
			    offset++;
		    }
	     }
#endif
	}
	if (offset > 0)
	{
		err = NumericArrayResize(uB, 1, (UHandle*)dest, offset * sizeof(wchar_t));
		if (!err)
		{
			if (codePage == CP_UTF8)
			{
				err = utf8towchar(src, len, WStrBuf(**dest), NULL, offset);
			}
			else
			{
				offset = mbstowcs(WStrBuf(**dest), src, offset);
			}
			if (!err)
			    LStrLen(**dest) = offset * sizeof(wchar_t) / sizeof(uInt16); 
		}
	}
	return err;
}

static MgErr unix_convert_wtomb(const wchar_t *src, int32 srclen, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	size_t dummy, length, size = 2 * srclen;
	wchar_t wdefChar;
	MgErr err = noErr;

	if (codePage == CP_UTF8)
	{
		err = wchartoutf8(src, srclen, NULL, &size, 0);
	}
	else
	{
		if (defaultChar)
		{
			dummy = mbtowc(NULL, NULL, 0);
			dummy = mbtowc(&wdefChar, &defaultChar, 1);
		}

		err = NumericArrayResize(uB, 1, (UHandle*)dest, size);
		if (!err)
		{
			uChar *dbuf = LStrBuf(**dest), *dend = LStrBuf(**dest) + size;
			const wchar_t *sbuf = src, *send = sbuf + srclen;
#ifdef HAVE_WCRTOMB
			mbstate_t mbs;

			wcrtomb(NULL, *sbuf, &mbs);
			while (sbuf < send)
			{
				length = wcrtomb(dbuf, *sbuf, &mbs);
				if ((length == 0) || (length > MB_CUR_MAX))
				{
					if (defaultChar)
					{
						length = wcrtomb(dbuf, wdefChar, &mbs);
#else
			dummy = wctomb(NULL, *sbuf);
			while (sbuf < send)
			{
				length = wctomb(dbuf, *sbuf);
				if ((length == 0) || (length > MB_CUR_MAX))
				{
					if (defaultChar)
					{
						length = wctomb(dbuf, wdefChar);
#endif
						*defaultCharWasUsed = LV_TRUE;
					}
					else
					{
						err = mgNotSupported;
						break;
					}
				}
				dbuf += length;
				if ((dend - dbuf) < MB_CUR_MAX)
				{
					length = dbuf - LStrBuf(**dest);
					LStrLen(**dest) = length;
					size *= 2;
					err = NumericArrayResize(uB, 1, (UHandle*)dest, size);
					if (err)
						break;
					dbuf = LStrBuf(**dest) + length;
					dend = LStrBuf(**dest) + size;
				}
				sbuf++;
			}
			if (!err)
			{
				length = dbuf - LStrBuf(**dest);
				err = NumericArrayResize(uB, 1, (UHandle*)dest, length + 1);
				TerminateLStr(dest, length);
			}
		}
	}
	return err;
}
#endif
#endif

LibAPI(MgErr) MultiByteCStrToWideString(ConstCStr src, int32 srclen, WStrHandle *dest, uInt32 codePage)
{
	if (*dest)
		WStrLenSet(**dest, 0);
	if (src && src[0])
	{
		MgErr err = mgNotSupported;
#if Win32
		int32 numChars = MultiByteToWideChar(codePage, 0, (LPCSTR)src, srclen, NULL, 0);
		if (numChars > 0)
		{ 
			err = NumericArrayResize(uW, 1, (UHandle*)dest, numChars);
			if (!err)
			{
				LStrLen(**dest) = MultiByteToWideChar(codePage, 0, (LPCSTR)src, srclen, LStrBuf(**dest), numChars);	
			}
		}
#elif MacOSX
		CFStringEncoding encoding = ConvertCodepageToEncoding(codePage);
		if (encoding != kCFStringEncodingInvalidId)
		{
			CFStringRef cfpath = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)src, srclen, encoding, false);
			if (cfpath)
			{
				CFMutableStringRef cfpath2 = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, cfpath);
				CFRelease(cfpath);
				if (cfpath2)
				{
					CFIndex numChars;
					
					CFStringNormalize(cfpath2, kCFStringNormalizationFormC);
    
					numChars = CFStringGetLength(cfpath2);
					if (numChars > 0)
					{ 
						err = NumericArrayResize(uB, 1, (UHandle*)dest, numChars * sizeof(UniChar));
						if (!err)
						{
							CFStringGetCharacters(cfpath2, CFRangeMake(0, numChars), (UniChar*)LStrBuf(**dest));
							LStrLen(**dest) = numChars;
						}
					}
					CFRelease(cfpath2); 
				}
			}
		}
#elif Unix
	    err = unix_convert_mbtow((const char*)src, srclen, dest, codePage);
#endif
		return err;
	}
	return mgNoErr;
}

LibAPI(MgErr) MultiByteToWideString(const LStrHandle src, WStrHandle *dest, uInt32 codePage)
{
	return MultiByteCStrToWideString(LStrBuf(*src), LStrLen(*src), dest, codePage);
}

LibAPI(MgErr) ZeroTerminateLString(LStrHandle *dest)
{
	int32 size = LStrLenH(*dest);
	MgErr err = NumericArrayResize(uB, 1, (UHandle*)dest, size + 1);
	if (!err)
		LStrBuf(**dest)[size] = 0;
	return err;
}

LibAPI(MgErr) WideStringToMultiByte(const WStrHandle src, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	return WideCStrToMultiByte(WStrBuf(*src), WStrLen(*src), dest, codePage, defaultChar, defaultCharWasUsed);
}

LibAPI(MgErr) WideCStrToMultiByte(const wchar_t *src, int32 srclen, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	if (defaultCharWasUsed)
		*defaultCharWasUsed = LV_FALSE;
	if (dest && *dest)
		LStrLen(**dest) = 0;

	if (srclen < 0)
		srclen = (int32)wcslen(src);

	if (srclen > 0)
	{
		MgErr err = mgNotSupported;
#if Win32 
		int32 numBytes = WideCharToMultiByte(codePage, 0, src, srclen, NULL, 0, NULL, NULL);
		if (numBytes > 0)
		{ 
			BOOL defUsed;
			err = NumericArrayResize(uB, 1, (UHandle*)dest, numBytes + 1);
			if (!err)
			{
				numBytes = WideCharToMultiByte(codePage, 0, src, srclen, (LPSTR)LStrBuf(**dest), numBytes, &defaultChar, &defUsed);
				TerminateLStr(dest, numBytes);
				if (defaultCharWasUsed)
					*defaultCharWasUsed = (LVBoolean)(defUsed != FALSE);
			}
		}
#elif MacOSX
		CFStringRef cfpath = CFStringCreateWithCharacters(kCFAllocatorDefault, src, srclen);
		if (cfpath)
		{
			CFMutableStringRef cfpath2 = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, cfpath);
			CFRelease(cfpath);
			if (cfpath2)
			{
				CFStringNormalize(cfpath2, kCFStringNormalizationFormD);
				CFStringEncoding encoding = ConvertCodepageToEncoding(codePage);
				if (encoding != kCFStringEncodingInvalidId)
				{
					CFIndex numBytes;
					if (CFStringGetBytes(cfpath2, CFRangeMake(0, CFStringGetLength(cfpath2)), encoding, defaultChar, false, NULL, 0, &numBytes) > 0)
					{
						err = NumericArrayResize(uB, 1, (UHandle*)dest, numBytes + 1);
						if (!err)
						{
							CFStringGetBytes(cfpath2, CFRangeMake(0, CFStringGetLength(cfpath2)), encoding, defaultChar, false, (UInt8*)LStrBuf(**dest), numBytes, &numBytes);
							TerminateLStr(dest, numBytes);
						}
					}
				}
				CFRelease(cfpath2);
			}
		}
#elif Unix
		err = unix_convert_wtomb(src, srclen, dest, codePage, defaultChar, defaultCharWasUsed);
#endif
		return err;
	}
	return mgNoErr;
}
