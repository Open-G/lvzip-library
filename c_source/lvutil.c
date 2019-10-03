/* 
   lvutil.c -- support functions for LabVIEW ZIP library

   Version 1.28, Sep 27, 2019

   Copyright (C) 2002-2019 Rolf Kalbermatter

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
//#include <stdio.h>
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
 #define st_atimespec     st_atim
 #define st_mtimespec     st_mtim
 #endif
#elif MacOSX
 #include <CoreFoundation/CoreFoundation.h>
 #include <CoreServices/CoreServices.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <sys/attr.h>
 #include <sys/xattr.h>
 #include <dirent.h>
 #define ftruncate64 ftruncate
 #if ProcessorType!=kX64
  #define MacIsInvisible(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible)
  #define MacIsInvFolder(cpb) ((cpb).dirInfo.ioDrUsrWds.frFlags & kIsInvisible)
  #define MacIsDir(cpb)   ((cpb).nodeFlags & ioDirMask)
  #define MacIsStationery(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsStationery)
  #define MacIsAlias(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsAlias)
  #define kFileChanged    (1L<<7)
  static MgErr OSErrToLVErr(OSErr err);
 #endif

#define kFinfoIsInvisible (OSSwapHostToBigConstInt16(kIsInvisible))

typedef	struct finderinfo {
    u_int32_t type;
    u_int32_t creator;
    u_int16_t fdFlags;
    u_int32_t location;
    u_int16_t reserved;
    u_int32_t extendedFileInfo[4];
} __attribute__ ((__packed__)) finderinfo;

typedef struct fileinfobuf {
    u_int32_t info_length;
    u_int32_t data[8];
} fileinfobuf;
#endif

#include "lwstr.h"

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

static uInt16 LVFileFlagsFromWinFlags(DWORD dwAttrs)
{
	uInt16 flags = 0;
	if (dwAttrs & FILE_ATTRIBUTE_REPARSE_POINT)
		flags |= kIsLink;
	if (!(dwAttrs & FILE_ATTRIBUTE_DIRECTORY))
		flags |= kIsFile;
	if (dwAttrs & FILE_ATTRIBUTE_HIDDEN)
        flags |= kFIsInvisible;
	if (dwAttrs & FILE_ATTRIBUTE_COMPRESSED)
		flags |= kIsCompressed;
	return flags;
}

#undef CreateFile
#undef GetFileAttributes
#undef SetFileAttributes
#undef FindFirstFile
#undef FindNextFile
#if Pharlap
static const char strWildcardPattern[] = "*.*";
#define CreateFile(path, fl, sh, ds, op, se, ov) CreateFileA(LStrBuf(path), fl, sh, ds, op, se, ov)
#define GetFileAttributes(path)         GetFileAttributesA(LStrBuf(path))
#define SetFileAttributes(path, attr)   SetFileAttributesA(LStrBuf(path), attr)
#define FindFirstFile(path, findFiles)  FindFirstFileA(LStrBuf(path), findFiles)
#define FindNextFile(handle, findFiles) FindNextFileA(handle, findFiles)
#else
static const wchar_t strWildcardPattern[] = L"*.*";
#define CreateFile(path, fl, sh, ds, op, se, ov) CreateFileW(WStrBuf(path), fl, sh, ds, op, se, ov)
#define GetFileAttributes(path)         GetFileAttributesW(WStrBuf(path))
#define SetFileAttributes(path, attr)   SetFileAttributesW(WStrBuf(path), attr)
#define FindFirstFile(path, findFiles)  FindFirstFileW(WStrBuf(path), findFiles)
#define FindNextFile(handle, findFiles) FindNextFileW(handle, findFiles)

static WCHAR *extStr = L".LNK";

static int Win32CanBeShortCutLink(WCHAR *str, int32 len)
{
	int32 extLen = lwslen(extStr);
	return (len >= extLen && !_wcsicmp(str + len - extLen, extStr));
}

LibAPI(MgErr) Win32ResolveShortCut(WStrPtr wStr, WStrPtr *wTgt, int32 *bufLen, uInt32 flags, DWORD *dwAttrs)
{
	HRESULT err = noErr;
	IShellLinkW* psl;
	IPersistFile* ppf;
	WIN32_FIND_DATAW fileData;
	WCHAR tempPath[MAX_PATH * 4], *srcPath = WStrBuf(wStr);

	// Don't bother trying to resolve shortcut if it doesn't have a ".lnk" extension.
	if (!Win32CanBeShortCutLink(srcPath, WStrLen(wStr)))
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
			if (SUCCEEDED(err) && flags)
			{
				// Resolve the shortcut.
				err = IShellLinkW_Resolve(psl, NULL, SLR_NO_UI);
			}
			if (SUCCEEDED(err))
			{
				fileData.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
				err = IShellLinkW_GetPath(psl, tempPath, MAX_PATH * 4, &fileData, flags ? 0 : SLGP_RAWPATH); 
				if (SUCCEEDED(err))
				{
					*dwAttrs = fileData.dwFileAttributes;
					if (err == S_OK)
					{
						int32 len = lwslen(tempPath);
						if (flags & kRecursive && Win32CanBeShortCutLink(tempPath, len))
						{
							srcPath = tempPath;
							continue;
						}
						if (*dwAttrs == INVALID_FILE_ATTRIBUTES)
							*dwAttrs = GetFileAttributesW(tempPath);
						
						if (wTgt)
						{
							err = LWStrRealloc(wTgt, bufLen, 0, len + 10);
							if (!err)
							{
								LWStrNCat(*wTgt, 0, *bufLen, tempPath, len);
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

LibAPI(MgErr) Win32ResolveSymLink(WStrPtr wSrc, WStrPtr *wTgt, int32 *bufLen, int32 flags, DWORD *dwAttrs)
{
	HANDLE handle;
	MgErr err = noErr;
	DWORD bytes = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
	PREPARSE_DATA_BUFFER buffer = NULL;
	int32 intLen = wTgt && bufLen ? *bufLen : 0;
	WStrPtr wIntermediate = wTgt && bufLen ? *wTgt : NULL;

	*dwAttrs = GetFileAttributes(wSrc);
	if (*dwAttrs == INVALID_FILE_ATTRIBUTES)
		return Win32GetLVFileErr();

	if (!(*dwAttrs & FILE_ATTRIBUTE_REPARSE_POINT))
		return mgNoErr;

	// Need to acquire backup privileges in order to be able to retrieve a handle to a directory below or call symlink kernel entry points
	Win32ModifyBackupPrivilege(TRUE);
	do
	{
		// Open the link file or directory
		handle = CreateFile(wSrc, GENERIC_READ, 0 /*FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE */, 
			                 NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (handle == INVALID_HANDLE_VALUE)
			return Win32GetLVFileErr();

		if (!buffer)
			buffer = (PREPARSE_DATA_BUFFER)DSNewPtr(bytes);
		if (buffer)
		{
			if (!DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0, buffer, bytes, &bytes, NULL))
				err = Win32GetLVFileErr();
			else if (bytes < 9)
				err = fEOF;
		}
		else
			err = mFullErr;

		// Close the handle to our file so we're not locking it anymore.
		CloseHandle(handle);

		if (!err)
		{
			int32 length;
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
				int32 parentLen = 0, newLen = length,
					  offset = HasDOSDevicePrefix(start, length);

				switch (offset)
				{
					case 0:
						/* No DOS device prefix, if it is an UNC name allocate 6 extra bytes and copy from the second character */
						if (length >= 2 && start[0] == kPathSeperator && start[1] == kPathSeperator)
						{
							relative = FALSE;
							offset = 1;
							newLen += 6;
						}
						/* fall through */
					case 4:
						/* If absolute path with drive specification */
						if (length - offset >= 3 && start[offset] < 128 && isalpha(start[offset]) && start[offset + 1] == ':' && start[offset + 2] == kPathSeperator)
						{
							relative = FALSE;
							newLen += (4 - offset);
						}
						break;
					case 8:
						/* DOS device prefix with UNC extension */
						relative = FALSE;
						offset = 7;
						break;
					default:
						err = mgArgErr;
						break;
				}
				intLen++;

				if (relative)
				{
					parentLen = LWStrParentPath(wSrc, -1);
					if (length && start[0] != kPathSeperator)
						parentLen++;
					newLen = parentLen + length + 1;
				}

				err = LWStrRealloc(&wIntermediate, &intLen, 0, newLen);
				if (!err)
				{
					if (relative)
					{
						if (start[0] == kPathSeperator)
							 offset = 1;

						err = WStrNCat(wIntermediate, 0, intLen, WStrBuf(wIntermediate), parentLen);
					}
					else if (offset == 1 || offset == 7)
					{
						err = WStrNCat(wIntermediate, 0, intLen, L"\\\\?\\UNC", 7);
					}
					else
					{
						err = WStrNCat(wIntermediate, 0, intLen, L"\\\\?\\", 4);
					}

					if (!err)
						err = WStrNCat(wIntermediate, -1, intLen, start + offset, length - offset);
					
					if (!err)
					{
						*dwAttrs = GetFileAttributes(wIntermediate);
						if (*dwAttrs == INVALID_FILE_ATTRIBUTES)
						{
							err = Win32GetLVFileErr();
						}
						else if (!(flags & kRecursive) || !(*dwAttrs & FILE_ATTRIBUTE_REPARSE_POINT))
						{
							if (relative && !flags)
								err = WStrNCat(wIntermediate, 0, intLen, start, length);

							break;
						}
					}
				}
				else
				{
					err = mFullErr;
				}
			}
			else
				err = fIOErr;
		}
		wSrc = wIntermediate;
	} while (!err);
	Win32ModifyBackupPrivilege(FALSE);
	DSDisposePtr((UPtr)buffer);
	if (!err)
	{
		if (bufLen)
			*bufLen = intLen;
		if (wTgt)
		{
			*wTgt = wIntermediate;
			return noErr;
		}
	}
	LWStrDispose(wIntermediate);
	return err;
}
#endif
#elif Unix || MacOSX
/* seconds between Jan 1 1904 GMT and Jan 1 1970 GMT */
#define dt1970re1904    2082844800L

#if VxWorks
// on VxWorks the stat time values are unsigned long integer
static void VxWorksConvertFromATime(ATime128 *time, unsigned long *sTime)
{
	/* VxWorks uses unsignde integers and can't represent dates before Jan 1, 1970 */
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
	/* The LabVIEW default default value is used to indicate to not update this value */
	if (time->u.f.val || time->u.f.fract)
	{
		sTime->tv_sec = (time_t)(time->u.f.val - dt1970re1904);
		sTime->tv_nsec = (int32_t)(time->u.p.fractHi / 4.294967296);
	}
	else
	{
		sTime->tv_nsec = UTIME_OMIT;
	}
}

static void UnixConvertToATime(struct timespec *sTime, ATime128 *time)
{
	time->u.f.val = (int64_t)sTime->tv_sec + dt1970re1904;
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

static uInt16 UnixFileTypeFromStat(struct stat *statbuf)
{
	uInt16 flags = 0;
	switch (statbuf->st_mode & S_IFMT)
	{
		case S_IFREG:
			flags = kIsFile;
			break;
		case S_IFLNK:
			flags = kIsLink;
	}

#if MacOSX
	if (statbuf->st_flags & UF_HIDDEN)
        flags |= kFIsInvisible;

	if (statbuf->st_flags & UF_COMPRESSED)
		flags |= kIsCompressed;
#endif
	return flags;
}

#if MacOSX
typedef struct
{
    uint32_t     length;
    off_t        size;
} FSizeAttrBuf;

static MgErr MacGetResourceSize(const char *path, uInt64 *size)
{
    int          err;
    attrlist_t   attrList;
    FSizeAttrBuf attrBuf;

    memset(&attrList, 0, sizeof(attrList));
    attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrList.fileattr  = ATTR_FILE_RSRCLENGTH;

	if (getattrlist(path, &attrList, &attrBuf, sizeof(attrBuf), FSOPT_NOFOLLOW)}
		return UnixToLVFileErr();

	assert(attrBuf.length == sizeof(attrBuf));
	*size = attrBuf.size;
	return noErr;
}
#endif
#endif

#if ((Win32 && !Pharlap) || (Unix && !VxWorks) || MacOSX)
static MgErr lvFile_ReadLink(LWStrPtr wSrc, LWStrPtr *wTgt, int32 *tgtLen, uInt32 flags, uInt32 *fileType);
#endif

/* Internal list directory function
   On Windows the pathName is a wide char Long String pointer and the function returns UTF8 encoded filenames in the name array
   On other platforms it uses whatever is the default encoding for both pathName and the filenames, which could be UTF8 (Linux and Mac)
 */
static MgErr lvFile_ListDirectory(LWStrPtr pathName, int32 bufLen, LStrArrHdl *nameArr, FileInfoArrHdl *typeArr, uInt32 cp, int32 flags)
{
	MgErr err;
	int32 rootLength, index = 0, size = 8;
#if Win32
	HANDLE dirp = INVALID_HANDLE_VALUE;
	DWORD dwAttrs;
#if Pharlap
	LPSTR fileName = NULL;
	WIN32_FIND_DATAA fileData;
#else
	PVOID oldRedirection = NULL;
	WStrPtr wTgt = NULL;
    int32 intLen = 0;
	DWORD dwAttrsResolved;
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

	err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, size);
	if (err)
		return err;

	err = NumericArrayResize(uQ, 1, (UHandle*)typeArr, size);
	if (err)
		return err;

#if Win32
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
				(**typeArr)->elm[index].fileFlags = 0;
				(**typeArr)->elm[index].fileType = 0;

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

#if !Pharlap
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

			(**typeArr)->elm[index].fileFlags = 0;
			(**typeArr)->elm[index].fileType = 0;
			fileName = fileData.cFileName;

			/* Create the path to the file for intermediate operations */
			err = LWStrNCat(pathName, rootLength, bufLen, fileName, -1);
			if (err)
				goto FListDirOut;

			dwAttrs = fileData.dwFileAttributes;
			if (dwAttrs != INVALID_FILE_ATTRIBUTES)
			{
#if Pharlap
                (**typeArr)->elm[index].fileFlags = LVFileFlagsFromWinFlags(dwAttrs);
#else
				lvFile_ReadLink(pathName, &wTgt, &intLen, flags, &((**typeArr)->elm[index].fileFlags));
#endif
				if (!(dwAttrs & FILE_ATTRIBUTE_DIRECTORY))
				{
#if !Pharlap
					if (flags && wTgt && (**typeArr)->elm[index].fileFlags & kIsLink)
					{
						LWStrGetFileTypeAndCreator(wTgt, &((**typeArr)->elm[index].fileType),  NULL);
					}
					else 
#endif
					{
						LWStrGetFileTypeAndCreator(pathName, &((**typeArr)->elm[index].fileType),  NULL);
					}
					if ((**typeArr)->elm[index].fileType && (**typeArr)->elm[index].fileType != kUnknownFileType)
					{
						(**typeArr)->elm[index].fileFlags |= kRecognizedType;
					}
				}
			}
			else
			{
				/* can happen if file disappeared since we did FindNextFile */
				(**typeArr)->elm[index].fileFlags |= kErrGettingType | kIsFile;
			}
#if Pharlap
			err = ConvertCString(fileName, -1, CP_ACP, (**nameArr)->elm + index, cp, 0, NULL);
#else
			err = WideCStrToMultiByte(fileName, -1, (**nameArr)->elm + index, cp, 0, NULL);
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
#if !Pharlap
	LWStrDispose(wTgt);
	if (oldRedirection)
		Wow64RevertWow64FsRedirection(&oldRedirection);
#endif
	if (dirp != INVALID_HANDLE_VALUE)
		FindClose(dirp);
#else
	if (!pathName || !LWStrLen(pathName))
		path = "/";
	else
		path = SStrBuf(pathName);

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

	if (!(dirp = opendir(path)))
		return UnixToLVFileErr();

	err = LWAppendPathSeparator(pathName, bufLen);
	if (err)
		goto FListDirOut;

	rootLength = LWStrLen(pathName);

	for (dp = readdir(dirp); dp; dp = readdir(dirp))
	{
		/* Skip the current dir, and parent dir entries. They are not guaranteed to be always the first
		   two entries enumerated! */
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

			(**typeArr)->elm[index].fileFlags = 0;
			(**typeArr)->elm[index].fileType = 0;

			err = ConvertCString((ConstCStr)dp->d_name, -1, CP_ACP, (**nameArr)->elm + index, cp, 0, NULL);
			if (err)
			    goto FListDirOut;
	
			err = LWStrNCat(pathName, rootLength, bufLen, dp->d_name, -1);
			if (err)
			    goto FListDirOut;

			if (lstat(SStrBuf(pathName), &statbuf))
			{
				(**typeArr)->elm[index].fileFlags |= kErrGettingType;
			}
			else
			{
#if !VxWorks    /* VxWorks does not support links */
				if (S_ISLNK(statbuf.st_mode))
				{
					(**typeArr)->elm[index].fileFlags |= kIsLink;
					if (stat(SStrBuf(pathName), &tmpstatbuf) == 0)	/* If link points to something */
						statbuf = tmpstatbuf;				        /* return info about it not link. */
				}
#endif
				if (!S_ISDIR(statbuf.st_mode))
				{
					(**typeArr)->elm[index].fileFlags |= kIsFile;
					LWStrGetFileTypeAndCreator(pathName, &((**typeArr)->elm[index].fileType), NULL);
                    if ((**typeArr)->elm[index].fileType && (**typeArr)->elm[index].fileType != kUnknownFileType)
						(**typeArr)->elm[index].fileFlags |= kRecognizedType;
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
	if (index < size)
	{
		if (*nameArr || index > 0)
			NumericArrayResize(uPtr, 1, (UHandle*)nameArr, index);
		if (*typeArr || index > 0)
			NumericArrayResize(uQ, 1, (UHandle*)typeArr, index);
	}
	return err;
}

/* On Windows and Mac, folderPath is a UTF8 encoded string, on other platforms it is locally encoded but
   this could be UTF8 (Linux, depending on its configuration) */
LibAPI(MgErr) LVFile_ListDirectory(LStrHandle folderPath, LStrArrHdl *nameArr, FileInfoArrHdl *typeArr, int32 flags)
{
	MgErr err = mgArgErr;
	LWStrPtr pathName = NULL;
	int32 bufLen = 0;
#if Win32
	if (LStrLenH(folderPath))
#endif
	{
		bufLen = 280;
		err = UPathToLWStr(folderPath, &pathName, &bufLen);
		if (err)
			return err;
	}
	err = lvFile_ListDirectory(pathName, bufLen, nameArr, typeArr, CP_UTF8, flags);
	LWStrDispose(pathName);
	return err;
}

/* This is the LabVIEW Path version of above function. It's strings are always locally encoded so there
   can be a problem with paths on Windows and other systems that don't use UTF8 as local encoding */
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
			bufLen = 280;
			err = LPathToLWStr(folderPath, &pathName, &bufLen);
			if (err)
				return err;
		}
		err = lvFile_ListDirectory(pathName, bufLen, nameArr, typeArr, CP_ACP, flags);
		LWStrDispose(pathName);
	}
	return err;
}

LibAPI(MgErr) LVPath_HasResourceFork(Path pathName, LVBoolean *hasResFork, uInt32 *sizeLow, uInt32 *sizeHigh)
{
    MgErr  err = mgNoErr;
#if MacOSX
    LWStrPtr lwstr = NULL;
#else
    Unused(pathName);
#endif
    if (hasResFork)
	    *hasResFork = 0;
	if (sizeLow)
		*sizeLow = 0;
	if (sizeHigh)
		*sizeHigh = 0;

#if MacOSX
    err = LPathToLWStr(path, &lwstr, NULL);
    if (!err)
    {
		FileOffset offset;
		err = MacGetResourceSize(SStrBuf(lwstr), &offset.q);
		if (!err)
		{
            if (hasResFork)
                *hasResFork = LV_TRUE;
            if (sizeLow)
                *sizeLow = offset.l.lo;
            if (sizeHigh)
                *sizeHigh = offset.l.hi;
           
        }
        DSDisposeHandle((UHandle)lwstr);
    }
#endif
    return err;
}

#if MacOSX
#ifndef UTIME_NOW
#define	UTIME_NOW	-1
#define	UTIME_OMIT	-2
#endif
static int lutimens(const char *path, struct timespec times_in[3])
{

    size_t attrbuf_size = 0;
    struct timespec times_out[3] = {};
    struct attrlist a = {
		.commonattr = 0;
        .bitmapcount = ATTR_BIT_MAP_COUNT
    };
    struct timespec *times_cursor = times_out;
    if (times_in[2].tv_nsec != UTIME_OMIT)
    {
        a.commonattr |= ATTR_CMN_CRTIME;
        *times_cursor++ = times_in[2];
        attrbuf_size += sizeof(struct timespec);
    }
    if (times_in[1].tv_nsec != UTIME_OMIT)
    {
        a.commonattr |= ATTR_CMN_MODTIME;
        *times_cursor++ = times_in[1];
        attrbuf_size += sizeof(struct timespec);
    }
    if (times_in[0].tv_nsec != UTIME_OMIT)
    {
        a.commonattr |= ATTR_CMN_ACCTIME;
        *times_cursor = times_in[0];
        attrbuf_size += sizeof(struct timespec);
    }
    return setattrlist(path, &a, &times_out, attrbuf_size, FSOPT_NOFOLLOW);
}
#endif

#define kSetableWinFileFlags kWinFileInfoArchive | kWinFileInfoHidden | kWinFileInfoNotIndexed | \
				             kWinFileInfoOffline | kWinFileInfoReadOnly | kWinFileInfoSystem | kWinFileInfoTemporary

static uInt16 UnixFlagsFromWindows(uInt16 attr)
{
	uInt16 flags = attr & kWinFileInfoReadOnly ?  0444 : 0666;
	if (attr & kWinFileInfoDirectory)
		flags |= 040000;
	else if (attr & kWinFileInfoDevice)
		flags |= 060000;
	else if (attr & kWinFileInfoReparsePoint)
		flags |= 0120000;
	else
		flags |= 010000; 
	return flags;
}

static uInt16 WinFlagsFromUnix(uInt32 mode)
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

static uInt32 MacFlagsFromWindows(uInt16 attr)
{
	uInt32 flags = 0;
	if (!(attr & kWinFileInfoDirectory) && (attr & kWinFileInfoReadOnly))
		flags |= kMacFileInfoImmutable;
	if (attr & kWinFileInfoHidden)
		flags |= kMacFileInfoHidden;
	if (attr & kWinFileInfoCompressed)
		flags |= kMacFileInfoCompressed;
	if (!(attr & kWinFileInfoArchive))
		flags |= kMacFileInfoNoDump;
	return flags;
}

static MgErr lvFile_FileInfo(LWStrPtr pathName, int32 bufLen, uInt8 write, LVFileInfo *fileInfo)
{
	MgErr err = noErr;
#if Win32 // Windows
    HANDLE handle = NULL;
	uInt64 count = 0;
#if Pharlap
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
	struct timespec times[3];
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
							fileInfo->winFlags = WinFlagsFromUnix(fileInfo->unixFlags);

					fi.dwFileAttributes = (fi.dwFileAttributes & ~kSetableWinFileFlags) | (fileInfo->winFlags & kSetableWinFileFlags);
                    if (!fi.dwFileAttributes)
						fi.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

				    SetFileAttributes(pathName, fi.dwFileAttributes);
			    }
			}
		}
		else
		{
			fileInfo->uid = 0xFFFFFFFF;
			fileInfo->gid = 0xFFFFFFFF;
			FileTimeToATime(&fi.ftCreationTime, &fileInfo->cDate);
			FileTimeToATime(&fi.ftLastWriteTime, &fileInfo->mDate);
			FileTimeToATime(&fi.ftLastAccessTime, &fileInfo->aDate);
			fileInfo->rfSize = 0;
			fileInfo->winFlags = Lo16(fi.dwFileAttributes);
			fileInfo->unixFlags = UnixFlagsFromWindows(fileInfo->winFlags);
			fileInfo->macFlags = MacFlagsFromWindows(fileInfo->winFlags);
			fileInfo->lvFlags = LVFileFlagsFromWinFlags(fi.dwFileAttributes);

			if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				fileInfo->type = kUnknownFileType;
				fileInfo->creator = kUnknownCreator;

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

					err = LWAppendPathSeparator(pathName, bufLen);
					if (!err)
				        err = LWStrNCat(pathName, -1, bufLen, strWildcardPattern, 3);
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
				if (!pathName || LWStrLen(pathName) <= 3)
				    fileInfo->size = count;
				else
				    fileInfo->size = count - 2;
			}
			else
			{		
				LWStrGetFileTypeAndCreator(pathName, &fileInfo->type, &fileInfo->creator);
				if (fileInfo->type && fileInfo->type != kUnknownFileType)
					fileInfo->lvFlags |= kRecognizedType;
				fileInfo->size = Quad(fi.nFileSizeHigh, fi.nFileSizeLow);
			}
		}
    }
#else
    if (lstat(SStrBuf(pathName), &statbuf))
		return UnixToLVFileErr();

	if (write)
	{
		if (fileInfo->winFlags)
		{
			if (!fileInfo->unixFlags)
				fileInfo->unixFlags = UnixFlagsFromWindows(fileInfo->winFlags);
			if (!fileInfo->macFlags)
				fileInfo->macFlags = MacFlagsFromWindows(fileInfo->winFlags);
		}
#if VxWorks
		VxWorksConvertFromATime(&fileInfo->aDate, &times.actime);
		VxWorksConvertFromATime(&fileInfo->mDate, &times.modtime);
        VxWorksConvertFromATime(&fileInfo->cDate, &statbuf.st_ctime);
		if (utime(SStrBuf(pathName), &times))
#else
		UnixConvertFromATime(&fileInfo->aDate, &times[0]);
		UnixConvertFromATime(&fileInfo->mDate, &times[1]);
        UnixConvertFromATime(&fileInfo->cDate, &times[2]);
		if (lutimens(SStrBuf(pathName), times))
#endif
			err = UnixToLVFileErr();
#if !VxWorks
		/*
		 * Changing the ownership probably won't succeed, unless we're root
         * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
         * the mode; current BSD behavior is to remove all setuid bits on
         * chown. If chown fails, loose setuid/setgid bits.
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
		else if (fileInfo->macFlags && lchflags(SStrBuf(pathName), fileInfo->macFlags))
			err = UnixToLVFileErr();			
#endif
	}
	else
	{
		fileInfo->uid = statbuf.st_uid;
		fileInfo->gid = statbuf.st_gid;
#if VxWorks
		VxWorksConvertToATime(statbuf.st_ctime, &fileInfo->cDate);
		VxWorksConvertToATime(statbuf.st_atime, &fileInfo->aDate);
		VxWorksConvertToATime(statbuf.st_mtime, &fileInfo->mDate);
#else
#if MacOSX
		UnixConvertToATime(&statbuf.st_birthtimespec, &fileInfo->cDate);
#endif
		UnixConvertToATime(&statbuf.st_mtimespec, &fileInfo->mDate);
		UnixConvertToATime(&statbuf.st_atimespec, &fileInfo->aDate);
#endif
		fileInfo->unixFlags = Lo16(statbuf.st_mode);
		fileInfo->winFlags = WinFlagsFromUnix(fileInfo->unixFlags);
#if MacOSX
		fileInfo->macFlags = statbuf.st_flags;
#else
		fileInfo->macFlags = 0;
#endif
		fileInfo->lvFlags = UnixFileTypeFromStat(&statbuf);

		if (S_ISDIR(statbuf.st_mode))
		{
			DIR *dirp;
			struct dirent *dp;

			fileInfo->type = kUnknownFileType;
			fileInfo->creator = kUnknownCreator;

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
			/* Try to determine LabVIEW file types based on file ending? */
			err = LWStrGetFileTypeAndCreator(pathName, &fileInfo->type, &fileInfo->creator);
			if (fileInfo->type && fileInfo->type != kUnknownFileType)
				fileInfo->lvFlags |= kRecognizedType;

			fileInfo->size = statbuf.st_size;
#if MacOSX
			MacGetResourceSize(SStrBuf(pathName), &fileInfo->rfSize);
#else
			fileInfo->rfSize = 0;
#endif
		}
	}
#endif
    return err;
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
		err = UPathToLWStr(filePath, &pathName, &bufLen);
		if (err)
			return err;
	}
    if (!err)
	{
		err = lvFile_FileInfo(pathName, bufLen, write, fileInfo);
		LWStrDispose(pathName);
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

    err = LPathToLWStr(filePath, &pathName, &bufLen);
    if (!err)
	{
		err = lvFile_FileInfo(pathName, bufLen, write, fileInfo);
		LWStrDispose(pathName);
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

#if Win32 && !Pharlap
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
	if (!(dwFlags & 0x8000) && !pCreateSymbolicLinkW)
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
		BOOL isRelative = TRUE, isDirectory = FALSE;
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
			hFile = CreateFile(lwSymlinkFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, lpsa, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS || FILE_FLAG_OPEN_REPARSE_POINT, NULL);
			deletefunc = RemoveDirectoryW;
		}
		else
		{
			hFile = CreateFile(lwSymlinkFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, lpsa, CREATE_NEW, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
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
				offset = HasDOSDevicePrefix(lpTargetFileName, length);
				switch (offset)
				{
					case 0:
						if (length >= 3 && lpTargetFileName[0] == kPathSeperator && lpTargetFileName[1] == kPathSeperator && lpTargetFileName[2] < 128 && isalpha(lpTargetFileName[2]))
						{
							isRelative = FALSE;
						}
					case 4:
						if (length >= 2 && lpTargetFileName[offset] < 128 && isalpha(lpTargetFileName[offset]) && lpTargetFileName[offset + 1] == ':')
						{
							isRelative = FALSE;
						}
						break;
					case 8:
						isRelative = FALSE;
						break;
					default:
						break;
				}
				if (!isRelative)
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

				buffer->SymbolicLinkReparseBuffer.Flags = isRelative ? SYMLINK_FLAG_RELATIVE : 0;
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

#if ((Win32 && !Pharlap) || (Unix && !VxWorks) || MacOSX)
static MgErr lvFile_CreateLink(LWStrPtr src, LWStrPtr tgt, uInt32 flags)
{
    MgErr err = mgNotSupported;
#if MacOSX || Unix
    if (flags & kLinkHard)
    {
        if (link(SStrBuf(src), (const char*)LStrBuf(tgt)))
            err = UnixToLVFileErr();
    }
    else
    {
        if (symlink(SStrBuf(src), (const char*)LStrBuf(tgt)))
            err = UnixToLVFileErr();
    }
#elif Win32
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
#endif
    return err;
}
#endif

LibAPI(MgErr) LVPath_CreateLink(Path path, Path target, uInt32 flags)
{
    MgErr err = mgNotSupported;
    LWStrPtr src = NULL;
    LWStrPtr tgt = NULL;

    if (!FIsAbsPath(path))
        return mgArgErr;

#if ((Win32 && !Pharlap) || (Unix && !VxWorks) || MacOSX)
	err =  LPathToLWStr(path, &src, NULL);
    if (!err)
    {
        err = LPathToLWStr(target, &tgt, NULL);
        if (!err)
        {
			err = lvFile_CreateLink(src, tgt, flags);
			LWStrDispose(tgt);
		}
		LWStrDispose(src);
	}
#endif
	return err;
}

LibAPI(MgErr) LVFile_CreateLink(LStrHandle path, LStrHandle target, uInt32 flags)
{
    MgErr err = mgNotSupported;
    LWStrPtr src = NULL;
    LWStrPtr tgt = NULL;

    if (!LStrIsAbsPath(path))
        return mgArgErr;

#if ((Win32 && !Pharlap) || (Unix && !VxWorks) || MacOSX)
	err =  UPathToLWStr(path, &src, NULL);
    if (!err)
    {
        err = UPathToLWStr(target, &tgt, NULL);
        if (!err)
        {
			err = lvFile_CreateLink(src, tgt, flags);
			LWStrDispose(tgt);
		}
		LWStrDispose(src);
	}
#endif
	return err;
}

#if ((Win32 && !Pharlap) || (Unix && !VxWorks) || MacOSX)
/* Read the path a link points to
   src: Path of the supposed link to read its destination
   tgt: Returned path with the location
   bufLen: on input the size of the buffer in tgt passed in, on return the reallocated size if any
   flags: kRecursive, resolve the link over multiple levels if applicable
          kResolve, resolve the location to be an absolute path if it wasn't absolute
		  none, return relative paths if the link is relative
   fileType: the file type of the returned location
   Returns: noErr on success resolution, cancelErr if the path is not a symlink, other errors as returned by the used API functions
*/
static MgErr lvFile_ReadLink(LWStrPtr wSrc, LWStrPtr *wTgt, int32 *tgtLen, uInt32 flags, uInt32 *fileType)
{
	MgErr err = noErr;
	LWStrPtr src = wSrc, tmp = NULL;
	int32 tmpLen = 0; 
#if Win32
	DWORD dwAttr = GetFileAttributes(wSrc);
	if (dwAttr == INVALID_FILE_ATTRIBUTES)
        return Win32GetLVFileErr();

	*fileType = LVFileFlagsFromWinFlags(dwAttr);

	do
	{
		if (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			err = Win32ResolveSymLink(src, wTgt, tgtLen, flags, &dwAttr);
		}
		else if (!(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
		{
			err = Win32ResolveShortCut(src, wTgt, tgtLen, flags, &dwAttr);
			if (!err)
				*fileType |= kIsLink;
		}
		if (!err && flags)
		{
			int32 offset = 0;
			/* Is the link target a relative path? We allow an empty path to be equivalent to . */
			if (!LWStrLen(*wTgt) || !LWStrIsAbsPath(*wTgt))
			{
				offset = LWStrParentPath(src, -1);
				if (LWStrLen(*wTgt) && LWStrBuf(*wTgt)[0] != kPathSeperator)
					offset++;
			}
			err = LWStrRealloc(&tmp, &tmpLen, offset, offset + LWStrLen(*wTgt));
			if (!err && offset && src == wSrc)
 				err = LWStrNCat(tmp, 0, tmpLen, LWStrBuf(src), offset);
			if (!err && LWStrLen(*wTgt))
 				err = LWStrNCat(tmp, offset, tmpLen, LWStrBuf(*wTgt), LWStrLen(*wTgt));

			/* lstat could fail as the symlink path does not have to point to a valid file or directory */
			if (!err && (dwAttr = GetFileAttributes(tmp)) == INVALID_FILE_ATTRIBUTES)
				break;

			src = tmp;
		}
    }
	while (!err && flags & kRecursive);

	if (err == cancelError)
		err = noErr;

	if (!err && tmp)
		*fileType |= LVFileFlagsFromWinFlags(dwAttr) << 16;
#else
    struct stat statbuf;

    if (lstat(SStrBuf(src), &statbuf))
        return UnixToLVFileErr();

	*fileType = UnixFileTypeFromStat(&statbuf);

	if (!S_ISLNK(statbuf.st_mode))
		/* no symlink, abort */
        return cancelError;
        
    do
    {
		do
		{
			err = LWStrReallocBuf(wTgt, tgtLen, 0, statbuf.st_size);
		    if (!err)
			{
				statbuf.st_size = readlink(SStrBuf(src), SStrBuf(*wTgt), *tgtLen);
				if (statbuf.st_size < 0)
					err = UnixToLVFileErr();
			}
		}
		while (!err && statbuf.st_size >= *tgtLen);
		if (!err && flags)
		{
			int32 offset = 0;
			/* Is the link target a relative path? */
			if (!statbuf.st_size || LWStrBuf(*wTgt)[0] != kPosixPathSeperator)
			{
				offset = LWStrParentPath(src, -1);
				if (LWStrLen(*wTgt) && LWStrBuf(*wTgt)[0] != kPosixPathSeperator)
					offset++;
			}
			err = LWStrReallocBuf(&tmp, &tmpLen, offset, offset + statbuf.st_size );
			if (!err && offset && src == wSrc)
 				err = LWStrNCat(tmp, 0, tmpLen, LWStrBuf(src), offset);
			if (!err && statbuf.st_size)
 				err = LWStrNCat(tmp, offset, tmpLen, LWStrBuf(*wTgt), statbuf.st_size);

			/* lstat could fail as the symlink path does not have to point to a valid file or directory */
			if (!err && lstat(SStrBuf(tmp), &statbuf))
				break;

			src = tmp;
		}
    }
	while (!err && flags & kRecursive && S_ISLNK(statbuf.st_mode));

	if (!err && tmp)
		*fileType |= UnixFileTypeFromStat(&statbuf) << 16;

#endif
	LWStrDispose(tmp);
	return err;
}
#endif

LibAPI(MgErr) LVFile_ReadLink(LStrHandle path, LStrHandle *target, uInt32 flags, uInt32 *fileType)
{
    MgErr err = mgNoErr;
    LWStrPtr src = NULL;
	int32 bufLen = 0;

	if (fileType)
		*fileType = 0;

	if (!LStrIsAbsPath(path))
        return mgArgErr;

#if !Pharlap && !VxWorks
    err = UPathToLWStr(path, &src, NULL);
    if (!err)
    {
		LWStrPtr wTgt = NULL;
		err = lvFile_ReadLink(src, &wTgt, &bufLen, flags, fileType);
		if (!err && wTgt)
		{
#if usesWinPath
			int32 offset = HasDOSDevicePrefix(LWStrBuf(wTgt), LWStrLen(wTgt));
			if (offset == 8)
				offset = 6;
			err = WideCStrToMultiByte(WStrBuf(wTgt) + offset, WStrLen(wTgt) - offset, target, CP_UTF8, 0, NULL);
			if (!err && offset == 6)
				LStrBuf(**target)[offset] = kPathSeperator;
#else
			err = ConvertToPosixPath(&wTgt, CP_ACP, target, CP_UTF8, 0, NULL, !(*fileType & kIsFile));
#endif
		}
		LWStrDispose(wTgt);
    }
    LWStrDispose(src);
    return err;
#else
	return mgNotSupported;
#endif
}

LibAPI(MgErr) LVPath_ReadLink(Path path, Path *target, uInt32 flags, uInt32 *fileType)
{
    MgErr err = mgNoErr;
    LWStrPtr src = NULL;
	int32 bufLen = 0;

    if (!FIsAbsPath(path))
        return mgArgErr;

#if !Pharlap && !VxWorks
	if (fileType)
		*fileType = 0;

    err = LPathToLWStr(path, &src, NULL);
    if (!err)
    {
		LWStrPtr wTgt = NULL;
		err = lvFile_ReadLink(src, &wTgt, &bufLen, flags, fileType);
		if (!err && wTgt)
		{
			LStrHandle handle = NULL;
#if usesWinPath
			int32 offset = HasDOSDevicePrefix(LWStrBuf(wTgt), LWStrLen(wTgt));
			if (offset == 8)
				offset = 6;
			err = WideCStrToMultiByte(LWStrBuf(wTgt) + offset, LWStrLen(wTgt) - offset, &handle, CP_ACP, 0, NULL);
			if (!err && offset == 6)
				LStrBuf(*handle)[offset] = kPathSeperator;
#else
			err = ConvertFromPosixPath(LWStrBuf(wTgt), LWStrLen(wTgt), CP_ACP, &handle, CP_ACP, '?', NULL, !(*fileType & kIsFile));
#endif
			if (!err)
				err = FTextToPath(LStrBuf(*handle), LStrLen(*handle), target);
			DSDisposeHandle((UHandle)handle);
		}
        LWStrDispose(wTgt);
    }
    LWStrDispose(src);
    return err;
#else
 	return mgNotSupported;
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
 #if !Pharlap
	wchar_t *rsrcPostfix = NULL;
 #endif
#endif
    
#if usesWinPath
 #if Pharlap
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

 #if !Pharlap
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
		err = LStrNCat(lwstr, -1, bufLen, namedResourceFork, -1);
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
    FSRef ref;
	HFSUniStr255 forkName;
    int8 perm;
    OSErr ret;
    MgErr err;

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

    err = FSMakePathRef(lwstr, &ref);
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
#if (usesWinPath && !Pharlap)
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
	/* Can't open the root (desktop) */
	if (!LStrLenH(filePath))
		return mgArgErr;
#endif

	err = UPathToLWStr(filePath, &pathName, &bufLen);
	if (!err)
	{
		err = lvFile_OpenFile(&ioRefNum, pathName, bufLen, rsrc, openMode, denyMode);
		LWStrDispose(pathName);
		if (!err)
		{
			err = FNewRefNum(NULL, (File)(ioRefNum), refnum);
		}
	}
	return err;
}

LibAPI(MgErr) LVPath_OpenFile(LVRefNum *refnum, Path path, uInt32 rsrc, uInt32 openMode, uInt32 denyMode)
{
#if (usesWinPath && !Pharlap)
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

	err = LPathToLWStr(path, &pathName, &bufLen);
	if (!err)
	{
		err = lvFile_OpenFile(&ioRefNum, pathName, bufLen, rsrc, openMode, denyMode);
		LWStrDispose(pathName);
	}
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
