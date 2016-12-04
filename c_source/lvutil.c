/* lvutil.c -- support functions for LabVIEW ZIP library

   Version 1.13, March 15th, 2013

   Copyright (C) 2002-2013 Rolf Kalbermatter
*/

#define ZLIB_INTERNAL
#include "zlib.h"
#include "ioapi.h"
#include "lvutil.h"
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
    } u;
 } REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;
#elif Unix
 #include <errno.h>
 #include <dirent.h>
 #include <fcntl.h>
 #include <unistd.h>
 #ifdef HAVE_ICONV
  #include <iconv.h>
 #endif
 #include <wchar.h>
 #if VxWorks
  #define FCNTL_PARAM3_CAST(param)  (int32)(param)
 #else
  #define FCNTL_PARAM3_CAST(param)  (param)
 #endif
#elif MacOSX
 #include <CoreFoundation/CoreFoundation.h>
 #include "MacBinaryIII.h"
 #include <sys/stat.h>
 #include <sys/xattr.h>
 #define ftello64 ftello
 #define fseeko64 fseeko
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
 #endif
#endif

#define usesHFSPath MacOS && ProcessorType!=kX64
#define usesPosixPath Unix || (MacOSX && ProcessorType==kX64)
#define usesWinPath Win32

#if usesHFSPath
#define kPathSeperator ':'
typedef SInt16 FileRefNum;
#elif usesPosixPath
#define kPathSeperator '/'
typedef FILE* FileRefNum;
#elif usesWinPath
#define kPathSeperator '\\'
typedef HANDLE FileRefNum;
#endif

#if HAVE_BZIP2
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

#if MacOS
#if usesHFSPath
static MgErr ConvertToPosixPath(const LStrHandle hfsPath, LStrHandle *posixPath, Boolean isDir)
{
    MgErr err = mFullErr;
    CFURLRef urlRef = NULL;
    CFStringRef fileRef, posixRef;
    uInt32 encoding = CFStringGetSystemEncoding();

	if (!posixPath)
		return mgArgErr;

	fileRef = CFStringCreateWithBytes(kCFAllocatorDefault, LStrBuf(*hfsPath), LStrLen(*hfsPath), encoding, false);
    if (*posixPath)
        LStrLen(**posixPath) = 0;
    if (!fileRef)
    {
		return mFullErr;
	}
    urlRef = CFURLCreateWithFileSystemPath(NULL, fileRef, kCFURLHFSPathStyle, isDir);
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
		if (CFStringGetBytes(posixRef, range, encoding, 0, false, NULL, 0, &len) > 0)
		{
			if (len > 0)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)posixPath, len + 1);
				if (!err)
				{
					if (CFStringGetBytes(posixRef, range, encoding, 0, false, LStrBuf(**posixPath), len, &len) > 0)
					{
						LStrBuf(**posixPath)[len] = 0;
						LStrLen(**posixPath) = len;
					}
					else
					{
						err = bogusError;
					}
				}
			}
		}
		else
		{
			err = bogusError;
		}
        CFRelease(posixRef);
    }
    return err;
}

static MgErr ConvertFromPosixPath(ConstCStr posixPath, int32 len, LStrHandle *hfsPath, Boolean isDir)
{
    MgErr err = mFullErr;
    CFURLRef urlRef = NULL;
    CFStringRef hfsRef;
    uInt32 encoding = CFStringGetSystemEncoding();

	if (!hfsPath)
		return mgArgErr;

    if (*hfsPath)
        LStrLen(**hfsPath) = 0;

    urlRef = CFURLCreateFromFileSystemRepresentation(NULL, posixPath, len, isDir);
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
		if (CFStringGetBytes(hfsRef, range, encoding, 0, false, NULL, 0, &len) > 0)
		{
			if (len > 0)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)hfsPath, len + 1);
				if (!err)
				{
					if (CFStringGetBytes(hfsRef, range, encoding, 0, false, LStrBuf(**hfsPath), len, &len) > 0)
					{
						LStrBuf(**hfsPath)[len] = 0;
						LStrLen(**hfsPath) = len;
					}
					else
					{
						err = bogusError;
					}
				}
			}
		}
		else
		{
			err = bogusError;
		}
        CFRelease(hfsRef);
    }
    return err;
}

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
#endif

#if MacOS && !MacOSX
/* Convert a Macintosh UTDDateTime to a LabVIEW timestamp and vice versa */
static void MacConvertFromLVTime(uInt32 lTime, UTCDateTime *mTime)
{
	mTime->fraction = 0;
	mTime->lowSeconds = lTime;
	mTime->highSeconds = 0;
}

static void MacConvertToLVTime(UTCDateTime *mTime, uInt32 *lTime)
{
	*lTime = mTime->lowSeconds;
}
#endif

#if usesHFSPath
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
#endif

#if Win32
/* int64 100ns intervals from Jan 1 1601 GMT to Jan 1 1904 GMT */
static const FILETIME dt1904FileTime = {
    0xe0fb4000 /* low int32 */,
    0x0153b281 /* high int32 */
};

static void Win32ConvertFromLVTime(uInt32 time, FILETIME *pFileTime)
{
    DWORD temp;

    if (time > 0.0)
    {
		/* Convert to int64 100ns intervals since Jan 1 1904 GMT. */
		pFileTime->dwHighDateTime = (DWORD)(time * (78125.0 / 33554432.0));
		pFileTime->dwLowDateTime = (DWORD)(time * 1E7);

		/* Convert to int64 100ns intervals since Jan 1 1601 GMT. */
		temp = pFileTime->dwLowDateTime;
		pFileTime->dwLowDateTime += dt1904FileTime.dwLowDateTime;
		pFileTime->dwHighDateTime += dt1904FileTime.dwHighDateTime;
		if (pFileTime->dwLowDateTime < temp)
			pFileTime->dwHighDateTime++;
    }
    return;
}

static void Win32ConvertToLVTime(FILETIME pFileTime, uInt32 *time)
{
    DWORD temp;

    /* Convert to int64 100ns intervals since Jan 1 1904 GMT. */
    temp = pFileTime.dwLowDateTime;
    pFileTime.dwLowDateTime -= dt1904FileTime.dwLowDateTime;
    pFileTime.dwHighDateTime -= dt1904FileTime.dwHighDateTime;
    if (pFileTime.dwLowDateTime > temp)
		pFileTime.dwHighDateTime--;

    /* Convert to float64 seconds since Jan 1 1904 GMT.
        secs = ((Hi32 * 2^^32) + Lo32) / 10^^7 = (Hi32 * (2^^25 / 5^^7)) + (Lo32 / 10^^7) */
    *time = (uInt32)((pFileTime.dwHighDateTime * (33554432.0 / 78125.0)) + (pFileTime.dwLowDateTime / 1E7));
}

static MgErr Win32ToLVFileErr(void)
{
    int32 err;

    err = GetLastError();
    switch(err)
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
      case ERROR_NO_MORE_FILES:     return mgNoErr;
    }
    return fIOErr;   /* fIOErr generally signifies some unknown file error */
}

#else
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <utime.h>

#if Unix
/* seconds between Jan 1 1904 GMT and Jan 1 1970 GMT */
#define dt1970re1904    2082844800L

static void UnixConvertFromLVTime(uInt32 time, time_t *sTime)
{
    if (time > 0.0)
    {
		*sTime = time - dt1970re1904;
    }
}

static void UnixConvertToLVTime(time_t sTime, uInt32 *time)
{
		*time = sTime + dt1970re1904;
}
#endif

#if MacOSX || Unix
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
#endif
#endif

LibAPI(void) DLLVersion(uChar* version)
{
    sprintf((char*)version, "lvzlib V 2.2, date: %s, time: %s",__DATE__,__TIME__);
}

#if usesPosixPath
#define LWStrPtr  LStrPtr

static int32 MakePathDSString(Path path, LWStrPtr *lstr, int32 reserve)
{
    int32 pathLen = -1;
    
    MgErr err = FPathToText(path, (LStrPtr)&pathLen);
    if (!err)
    {
        *lstr = (LStrPtr)DSNewPClr(sizeof(int32) + pathLen + reserve + 1);
        if (!*lstr)
            return mFullErr;
        (*lstr)->cnt = pathLen;
        err = FPathToText(path, *lstr);
        if (err)
            DSDisposePtr((UPtr)*lstr);
    }
    return err;
}
#elif usesWinPath
#define LWStrPtr  UStrPtr

static int32 MakePathDSString(Path path, LWStrPtr *wstr, int32 reserve)
{
    int32 pathLen = -1;
	LStrPtr lstr;
    
    MgErr err = FPathToText(path, (LStrPtr)&pathLen);
    if (!err)
    {
        lstr = (LStrPtr)DSNewPClr(sizeof(int32) + pathLen + 1);
        if (!lstr)
            return mFullErr;
        LStrLen(lstr) = pathLen;
        err = FPathToText(path, lstr);
        if (!err)
		{
			LPCSTR buf = (LPCSTR)LStrBuf(lstr);
			int32 off = 0, len = 0;

			pathLen = MultiByteToWideChar(CP_ACP, 0, buf, LStrLen(lstr), NULL, 0);
			if (pathLen > 0)
			{
				if (buf[0] && buf[1] == ':')
				{
					/* Absolute path with drive letter */
					len = 4;
				}
				else if (buf[0] == '\\' && buf[1] == '\\' && buf[2] != '?')
				{
					/* Absolute UNC path */
					len = 6;
					off = 1;
				}
				*wstr = (UStrPtr)DSNewPClr(sizeof(int32) + sizeof(wchar_t) * (len + pathLen + reserve + 1));
				if (*wstr)
				{
					if (buf[0] && buf[1] == ':')
					{
						wcscpy(UStrBuf(*wstr), L"\\\\?\\");
					}
					else if (buf[0] == '\\' && buf[1] == '\\' && buf[2] != '?')
					{
						wcscpy(UStrBuf(*wstr), L"\\\\?\\UNC");
					}
					pathLen = len + MultiByteToWideChar(CP_ACP, 0, buf + off, LStrLen(lstr) - off, UStrBuf(*wstr) + len, pathLen);
					UStrLenSet(*wstr, pathLen);
					UStrBuf(*wstr)[pathLen] = 0;
				}
			}
		}
        DSDisposePtr((UPtr)lstr);
    }
    return err;
}

static MgErr WStrNCat(UStrPtr wstr, int32 off, int32 length, const wchar_t *str)
{
	int32 len = (int32)wcslen(str);
	if (off == -1)
		off = UStrLen(wstr);
	if (off + len < length)
	{
		wcscpy(UStrBuf(wstr) + off, str);
		UStrLenSet(wstr, off + len);
		return noErr;
	}
	return mgArgErr;
}

static MgErr NormalizePath(UStrPtr wstr)
{
	LPWSTR a, s, e, t = UStrBuf(wstr);
	int32 unc = FALSE, len = UStrLen(wstr);

	a = s = t;
	e = s + len;
	if (!*s || !len)
		return noErr;
  

	// \\?\UNC\server\share
	// \\?\C:\
	// \\server\share
	// C:\

	if (*s == '\\')
	{
		int32 n = 1;
		s++;
		if (s[0] == '\\')
		{
			n++;
			s++;
			if (s[0] == '?' && s[1] == '\\')
			{
				n += 3;
				s += 3;
				if ((s[0] == 'U' || s[0] == 'u') && (s[1] == 'N' || s[1] == 'n') && (s[2] == 'C' || s[2] == 'C') && s[2] == '\\')
				{
					unc = TRUE;
					n += 4;
					s += 4;
				}
			}
			else
			{
				unc = TRUE;
				n++;
				s++;
			}
		}
		t += n;
		a = s;
	}
	if (s[0] && s[1] == ':')
	{
		/* X:\ */
		s += 2;
		t += 2;
		if (s[0] == '\\')
		{
			s++; t++;
		}
	}

	/* Canonicalize the rest of the path */
	while (s < e)
	{
		if (s[0] == '.')
		{
			if (s[1] == '\\' && (s == a || s[-1] == '\\' || s[-1] == ':'))
			{
				s += 2; /* Skip .\ */
			}
			else if (s[1] == '.' && (t == a || t[-1] == '\\'))
			{
				/* \.. backs up a directory, over the root if it has no \ following X:.
				 * .. is ignored if it would remove a UNC server name or initial \\
				 */
				if (t != a)
				{
					if (t > a + 1 && t[-1] == '\\' &&
					    (t[-2] != '\\' || t > a + 2))
					{
						if (t[-2] == ':' && (t > a + 3 || t[-3] == ':'))
						{
							t -= 2;
							while (t > a && *t != '\\')
								t--;
							if (*t == '\\')
								t++; /* Reset to last '\' */
							else
								t = a; /* Start path again from new root */
						}
						else if (t[-2] != ':' && !unc)
							t -= 2;
					}
					while (t > a && *t != '\\')
						t--;
					if (t == a)
					{
						*t = '\\';
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
		*t++ = '\\';
	*t++ = '\0';
	return noErr;
}

LibAPI(MgErr) Win32ResolveShortCut(UStrPtr wStr, UStrPtr *wTgt, Bool32 recursive, DWORD *dwAttrs)
{
	MgErr err = noErr;
	IShellLinkW* psl;
	IPersistFile* ppf;
	WIN32_FIND_DATAW fileData;
	WCHAR *extStr = L".LNK";
	WCHAR tempPath[MAX_PATH], *srcPath = UStrBuf(wStr);
	int32 len = UStrLen(wStr), extLen = (int32)wcslen(extStr);

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
						if (err != S_OK)
						{
							/* Path couldn't be retrieved, is there any other way we can get the info? */
							fileData.cFileName;
						}
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
							
							if (*wTgt && UStrLen(*wTgt) <= len)
							{
								DSDisposePtr((UPtr)*wTgt);
								*wTgt = NULL;
							}
							if (!*wTgt)
								*wTgt = (UStrPtr)DSNewPClr(sizeof(int32) + (len + 1) * sizeof(WCHAR));
							if (*wTgt)
							{
								lstrcpynW(UStrBuf(*wTgt), tempPath, len + 1);
								UStrLenSet(*wTgt, len);
							}
							else
							{
								err = mFullErr;
							}
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
	return err;
}

static BOOL ModifyBackupPrivilege(BOOL fEnable)
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

static MgErr Win32ResolveReparsePoint(UStrPtr wSrc, UStrPtr *wTgt, Bool32 recursive, DWORD *dwAttrs)
{
	HANDLE handle;
	MgErr err = noErr;
	DWORD bytes = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
	PREPARSE_DATA_BUFFER buffer = NULL;
	UStrPtr tempPath = wTgt ? *wTgt : NULL;

	*dwAttrs = GetFileAttributesW(UStrBuf(wSrc));
	if (*dwAttrs == INVALID_FILE_ATTRIBUTES)
	{
		return Win32ToLVFileErr();
	}

	// Need to acquire backup privileges in order to be able to retrieve a handle to a directory below or call symlink kernel entry points
	ModifyBackupPrivilege(TRUE);
	while (!err && *dwAttrs & FILE_ATTRIBUTE_REPARSE_POINT)
	{
		// Open the link file or directory
		handle = CreateFileW(tempPath ? UStrBuf(tempPath) : UStrBuf(wSrc), GENERIC_READ, 0 /*FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE */, 
			                 NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (handle != INVALID_HANDLE_VALUE)
		{
			buffer = (PREPARSE_DATA_BUFFER)DSNewPClr(bytes);
			if (!DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0, buffer, bytes, &bytes, NULL))
				err = Win32ToLVFileErr();

			if (bytes < 9)
				err = fEOF;

			// Close the handle to our file so we're not locking it anymore.
			CloseHandle(handle);
		}
		else
		{
			err = Win32ToLVFileErr();
		}
           
		if (!err)
		{
			BOOL relative = FALSE;
			LPWSTR start = NULL;
			int32 length = 0, offset = 4;

			switch (buffer->ReparseTag)
			{
				case IO_REPARSE_TAG_SYMLINK:
					start = (LPWSTR)((char*)buffer->u.SymbolicLinkReparseBuffer.PathBuffer + buffer->u.SymbolicLinkReparseBuffer.SubstituteNameOffset);
					length = buffer->u.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
					relative = buffer->u.SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE;
					break;
				case IO_REPARSE_TAG_MOUNT_POINT:
					start = (LPWSTR)((char*)buffer->u.MountPointReparseBuffer.PathBuffer + buffer->u.MountPointReparseBuffer.SubstituteNameOffset);
					length = buffer->u.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
					break;
			}
			if (length)
			{
				if (length > 4 && (CompareStringW(LOCALE_SYSTEM_DEFAULT, 0, start, 4, L"\\\\??\\", 4) == CSTR_EQUAL))
				{
					relative = FALSE;
					start += 4;
				}
				else
				{
					length += 4;
				}

				if (relative)
				{
					length += UStrLen(wSrc);
				}
					
				if (tempPath && UStrLen(tempPath) <= length)
				{
					DSDisposePtr((UPtr)tempPath);
					tempPath = NULL;
				}
				if (!tempPath)
				{
					tempPath = (UStrPtr)DSNewPClr(sizeof(int32) + sizeof(WCHAR) * (length + 1));
				}
				if (tempPath)
				{
					wcsncpy(UStrBuf(tempPath), L"\\\\??\\", length);
					if (relative)
					{
						wcsncpy(UStrBuf(tempPath) + offset, UStrBuf(wSrc), UStrLen(wSrc));
						offset += UStrLen(wSrc);
					}
					wcsncpy(UStrBuf(tempPath) + offset, start, length - offset);
					UStrLenSet(tempPath, length);

					*dwAttrs = GetFileAttributesW(UStrBuf(tempPath));
					if (*dwAttrs == INVALID_FILE_ATTRIBUTES)
					{
						err = Win32ToLVFileErr();
					}
					else if (!recursive)
					{
						break;
					}
				}
				else
				{
					err = mFullErr;
				}
			}
		}
	}
	ModifyBackupPrivilege(FALSE);
	if (buffer)
		DSDisposePtr((UPtr)buffer);
	if (tempPath)
	{
		if (!wTgt || err && !*wTgt)
		{
			DSDisposePtr((UPtr)tempPath);
		}
		else
		{
			*wTgt = tempPath;
		}
	}
	return err;
}

static MgErr Win32ResolveLink(UStrPtr wSrc, UStrPtr *wTgt, Bool32 recursive, DWORD *dwAttrs)
{
	MgErr err = mgNotSupported;

	return err;
}

#endif

LibAPI(MgErr) LVPath_ListDirectory(Path folderPath, LStrArrHdl *nameArr, FileInfoArrHdl *typeArr)
{
	MgErr err;
	FInfoRec foldInfo;
#if Win32
	UStrPtr pathName = NULL, wTgt = NULL;
	HANDLE dirp = INVALID_HANDLE_VALUE;
	LPWSTR fileName = NULL, wext;
	DWORD dwAttrs;
	Str255 ext;
	WIN32_FIND_DATAW fileData;
	PVOID oldRedirection = NULL;
	int32 bufLength, rootLength, type, index = 0, size = 8;
#else
	FMListDetails **typeList = NULL;
	FDirEntHandle nameList = NULL;
#endif
	if (!FIsAPath(folderPath))
		return mgArgErr;
	/* Check that we have actually a folder */
	err = FGetInfo(folderPath, &foldInfo);
	if (err)
		return err;
	if (!foldInfo.folder)
		return mgArgErr;

#if Win32
	err = MakePathDSString(folderPath, &pathName, 260);
	if (err)
		return err;

	bufLength = UStrLen(pathName) + 260;
	if (FDepth(folderPath) != 1L)
	{
		err = WStrNCat(pathName, UStrLen(pathName), bufLength, L"\\");
		if (err)
			goto FListDirOut;
	}
	rootLength = UStrLen(pathName);
	err = WStrNCat(pathName, rootLength, bufLength, L"*.*");
	if (err)
		goto FListDirOut;

	err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, size);
	if (err)
		goto FListDirOut;

	err = NumericArrayResize(uL, 1, (UHandle*)typeArr, size * 2);
	if (err)
		goto FListDirOut;

	if (!Wow64DisableWow64FsRedirection(&oldRedirection))
		/* Failed but lets go on anyhow, risking strange results for virtualized paths */
		oldRedirection = NULL;

	dirp = FindFirstFileW(UStrBuf(pathName), &fileData);
	if (dirp == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			if (nameArr)
				(**nameArr)->numItems = 0;
			if (typeArr)
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
		if (wcscmp(fileData.cFileName, L".") && wcscmp(fileData.cFileName, L".."))
		{
			dwAttrs = fileData.dwFileAttributes;

			/* Make sure our arrays are resized to contain the new values */
			if (index + 1 >= size)
			{
				size *= 2;
				err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, size);
				if (err)
					goto FListDirOut;

				err = NumericArrayResize(uL, 1, (UHandle*)typeArr, size * 2);
				if (err)
					goto FListDirOut;
			}

			/* Create the path to the file for intermediate operations */
			err = WStrNCat(pathName, rootLength, bufLength, fileData.cFileName);
			if (err)
				goto FListDirOut;

			(**typeArr)->elm[index].flags = 0;
			if (Win32ResolveShortCut(pathName, &wTgt, TRUE, &dwAttrs))
			{
				(**typeArr)->elm[index].flags |= kIsLink;
			}
			if (dwAttrs == INVALID_FILE_ATTRIBUTES && wcslen(fileData.cAlternateFileName))
			{
				/* Let's try the alternate file name */
				err = WStrNCat(pathName, rootLength, bufLength, fileData.cAlternateFileName);
				if (err)
					goto FListDirOut;

				if (Win32ResolveShortCut(pathName, &wTgt, TRUE, &dwAttrs))
				{
					(**typeArr)->elm[index].flags |= kIsLink;
				}
			}
			/* If we succeed using the alternative name, use this instead */
			if (dwAttrs != INVALID_FILE_ATTRIBUTES)
			{
				fileName = fileData.cAlternateFileName;
			}

			(**typeArr)->elm[index].type = 0;
			if (dwAttrs == INVALID_FILE_ATTRIBUTES)
			{
				/* can happen if file disappeared since we did FindNextFile */
				(**typeArr)->elm[index].flags |= kErrGettingType | kIsFile;
			}
			else
			{
				if (dwAttrs & FILE_ATTRIBUTE_REPARSE_POINT)
				{
					(**typeArr)->elm[index].flags |= kIsLink;
					if (!Win32ResolveLink(pathName, &wTgt, TRUE, &dwAttrs))
					{

					}
				}
				if (!(dwAttrs & FILE_ATTRIBUTE_DIRECTORY))
				{
					(**typeArr)->elm[index].flags |= kIsFile;
					wext = wcsrchr(wTgt ? (LPCWSTR)wTgt : fileName, L'.');
					if (wext)
					{
						int len = WideCharToMultiByte(CP_ACP, 0, wext, -1, (LPSTR)(ext + 1), 255, 0, NULL);
						if (ext[len] == 0)
							len--;
						ext[len] = 0;
					    type = PStrHasRezExt(ext);
						if (type)
						{
							(**typeArr)->elm[index].flags |= kRecognizedType;
							(**typeArr)->elm[index].type = type;
						}
					}
				}
			}
			err = WideCStrToMultiByte(fileName, -1, (**nameArr)->elm + index, CP_ACP, 0, NULL);

			index++;
			(**nameArr)->numItems = index;
			(**typeArr)->numItems = index;
		}
	}
	while (FindNextFileW(dirp, &fileData));

	if (!err)
		err = Win32ToLVFileErr();

FListDirOut:
	if (oldRedirection)
		Wow64RevertWow64FsRedirection(&oldRedirection);
	if (dirp != INVALID_HANDLE_VALUE)
		FindClose(dirp);
	if (index < size)
	{
		if (*nameArr || index > 0)
			NumericArrayResize(uPtr, 1, (UHandle*)nameArr, index);
		if (*typeArr || index > 0)
			NumericArrayResize(uL, 1, (UHandle*)typeArr, index * 2);
	}
	DSDisposePtr((UPtr)pathName);
#else
	nameList = (FDirEntHandle)AZNewHClr(4);
	if (!nameList)
		return mFullErr;
	typeList = (FMListDetails **)AZNewHandle(0);
	if (!typeList)
	{
		AZDisposeHandle((UHandle)nameList);
		return mFullErr;
	}
	err = FListDir(folderPath, nameList, typeList);
	if (!err)
	{
		int32 i = 0, n = CPStrLen(*nameList);
	    UPtr fName = CPStrBuf(*nameList);
		err = NumericArrayResize(uPtr, 1, (UHandle*)nameArr, n);
		if (!err)
		{
			LStrHandle *names = (**nameArr)->elm;
		    err = NumericArrayResize(uL, 1, (UHandle*)typeArr, n * 2);
			if (!err)
			{
				for (i = 0; i < n; i++, names++)
				{
					err = NumericArrayResize(uB, 1, (UHandle*)names, PStrLen(fName));
					if (err)
				        break;

					MoveBlock(PStrBuf(fName), LStrBuf(**names), PStrLen(fName));
					LStrLen(**names) = PStrLen(fName);
					fName += PStrSize(fName);
				}
				MoveBlock((ConstUPtr)*typeList, (UPtr)((**typeArr)->elm), n * sizeof(FMListDetails));
				(**typeArr)->numItems = i;
			}
			n = (**nameArr)->numItems;
			(**nameArr)->numItems = i;
			/* Clear out possibly superfluous handles */
			if (n > i)
			{
				for (; i < n; i++, names++)
				{
					if (*names)
						DSDisposeHandle((UHandle)*names);
					*names = NULL;
				}
			}
		}
	}
	AZDisposeHandle((UHandle)nameList);
	AZDisposeHandle((UHandle)typeList);
#endif
	return err;
}

LibAPI(MgErr) LVPath_HasResourceFork(Path path, LVBoolean *hasResFork, uInt32 *sizeLow, uInt32 *sizeHigh)
{
    MgErr  err = mgNoErr;
#if MacOSX
    LStrHandle lstr = NULL;
#elif MacOS
    FSRef ref;
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
    err = LVPath_ToText(path, &lstr);
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
#elif MacOS
    err = FSMakePathRef(path, &ref);
    if (!err)
    {
		FSCatalogInfoBitmap whichInfo =  kFSCatInfoNodeFlags | kFSCatInfoRsrcSizes;
		FSCatalogInfo		catalogInfo;
 
		/* get nodeFlags and catalog info */
		err = OSErrToLVErr(FSGetCatalogInfo(&ref, whichInfo, &catalogInfo, NULL, NULL,NULL));
		if (!err && catalogInfo.nodeFlags & kFSNodeIsDirectoryMask)
			err = fIOErr;
		if (!err && catalogInfo.rsrcLogicalSize > 0)
		{
			if (hasResFork)
				*hasResFork = LV_TRUE;
			if (sizeLow)
				*sizeLow = Lo32(catalogInfo.rsrcLogicalSize);
			if (sizeHigh)
				*sizeHigh = Hi32(catalogInfo.rsrcLogicalSize);
		}
    }
#endif
    return err;
}

LibAPI(MgErr) LVPath_UtilFileInfo(Path path,
                                  uInt8 write,
                                  uInt8 *isDirectory,
                                  LVFileInfo *fileInfo,
                                  LStrHandle comment)
{
    MgErr err = mgNoErr;
#if MacOSX
    FInfoRec infoRec;
#elif MacOS
	FSRef ref;
#elif Win32
    UStrPtr lstr = NULL;
    HANDLE handle = NULL;
	WIN32_FIND_DATAW fi = {0};
    uInt64 count = 0;
#elif Unix
    LStrPtr lstr = NULL;
    struct stat statbuf;
    struct utimbuf buf;
    uInt64 count = 0;
#endif

    if (!path || !comment)
      return mgArgErr;

#if MacOSX
    if (write)
    {
        infoRec.creator = fileInfo->creator;
        infoRec.type = fileInfo->type;
        infoRec.permissions = 0;
        infoRec.size = fileInfo->size;
        infoRec.rfSize = fileInfo->rfSize;
        infoRec.cdate = fileInfo->cDate;
        infoRec.mdate = fileInfo->mDate;
        infoRec.folder = *isDirectory;
        infoRec.isInvisible = fileInfo->flags & kFIsInvisible;
        infoRec.location.v = fileInfo->location.v;
        infoRec.location.h = fileInfo->location.h;
        infoRec.owner[0] = 0;
        infoRec.group[0] = 0;
        err = FSetInfo(path, &infoRec);
    }
    else
    {
        err = FGetInfo(path, &infoRec /*, kFGetInfoAll*/);
        if (!err)
        {
            fileInfo->creator = infoRec.creator;
            fileInfo->type = infoRec.type;
            fileInfo->size = infoRec.size;
            fileInfo->rfSize = infoRec.rfSize;
            fileInfo->cDate = infoRec.cdate;
            fileInfo->mDate = infoRec.mdate;
            *isDirectory = infoRec.folder;
            fileInfo->flags = infoRec.isInvisible ? kFIsInvisible : 0;
            fileInfo->location.v = infoRec.location.v;
            fileInfo->location.h = infoRec.location.h;
        }
    }
#elif MacOS
	err = FSMakePathRef(path, &ref);
    if (!err)
    {
		FSCatalogInfoBitmap whichInfo = write ? kFSCatInfoSettableInfo : kFSCatInfoGettableInfo;
		FSCatalogInfo catInfo;
		FSSpec fss;

		err = OSErrToLVErr(FSGetCatalogInfo(&ref, whichInfo, &catInfo, NULL, &fss, NULL));
		if (!err)
		{
			DTPBRec dtpb;
			OSErr ret;
		
			dtpb.ioCompletion = NULL;
			dtpb.ioNamePtr = NULL;
			dtpb.ioVRefNum = fss.vRefNum;

			*isDirectory = catInfo.nodeFlags & kFSNodeIsDirectoryMask != 0;
			if (write)
			{
				if (*isDirectory)
				{
					FolderInfo *info = (FolderInfo*)catInfo.finderInfo; 
					info->finderFlags = fileInfo->flags;				// uInt16
					info->location.v = fileInfo->location.v;
					info->location.h = fileInfo->location.h;

					ExtendedFolderInfo *xInfo = (ExtendedFolderInfo*)catInfo.extFinderInfo;
					xInfo->extendedFinderFlags = fileInfo->xFlags;		// uInt16
					xInfo->putAwayFolderID = fileInfo->putAwayId;		// int32
				}
				else
				{
					FileInfo *info = (FileInfo*)catInfo.finderInfo;
					info->fileType = fileInfo->type;					// OSType
					info->fileCreator = fileInfo->creator;				// OSType
					info->finderFlags = fileInfo->flags;				// uInt16
					info->location.v = fileInfo->location.v;			// Point
					info->location.h = fileInfo->location.h;

					ExtendedFileInfo *xInfo = (ExtendedFileInfo*)catInfo.extFinderInfo;
					xInfo->extendedFinderFlags = fileInfo->xFlags;     // uInt16
					xInfo->putAwayFolderID = fileInfo->putAwayId;      // int32
				}
				MacConvertFromLVTime(fileInfo->cDate, &catInfo.createDate);
				MacConvertFromLVTime(fileInfo->mDate, &catInfo.contentModDate);

				err = OSErrToLVErr(FSSetCatalogInfo(&ref, whichInfo, &catInfo));
				if (err)
				{
					DEBUGPRINTF(("FSSetCatalogInfo: err = %ld", err));
				}
				else if (LStrLen(*comment) > 0)
				{
					/* Ignore error for getting Desktop comments */
					ret = OSErrToLVErr(PBDTGetPath(&dtpb));
					if (!ret)
					{
						dtpb.ioNamePtr = fss.name;
						dtpb.ioDirID = fss.parID;
						dtpb.ioDTBuffer = (UPtr)LStrBuf(*comment);
						dtpb.ioDTReqCount = LStrLen(*comment);
						PBDTSetCommentSync(&dtpb);
					}
				}
			}
			else
			{
				if (*isDirectory)
				{
					FolderInfo *info = (FolderInfo*)catInfo.finderInfo; 
					fileInfo->type = kUnknownType;						// OSType
					fileInfo->creator = kUnknownType;					// OSType
					fileInfo->flags = info->finderFlags;				// uInt16
					fileInfo->location.v = info->location.v;			// Point
					fileInfo->location.h = info->location.h;
					fileInfo->finderId = info->reservedField;			// uInt16
					fileInfo->size = catInfo.valence;
					fileInfo->rfSize = 0;
					
					ExtendedFolderInfo *xInfo = (ExtendedFolderInfo*)catInfo.extFinderInfo;
					fileInfo->xFlags = xInfo->extendedFinderFlags;		// uInt16
					fileInfo->putAwayId = xInfo->putAwayFolderID;		// int32
				}
				else
				{
					FileInfo *info = (FileInfo*)catInfo.finderInfo; 
					fileInfo->type = info->fileType;					// OSType
					fileInfo->creator = info->fileCreator;				// OSType
					fileInfo->flags = info->finderFlags;				// uInt16
					fileInfo->location.v = info->location.v;			// Point
					fileInfo->location.h =info->location.h;
					fileInfo->finderId = info->reservedField;			// uInt16

					fileInfo->size = catInfo.dataLogicalSize;
					fileInfo->rfSize = catInfo.rsrcLogicalSize;
				
					ExtendedFileInfo *xInfo = (ExtendedFileInfo*)catInfo.extFinderInfo;
					fileInfo->xFlags = xInfo->extendedFinderFlags;		// uInt16
					fileInfo->putAwayId = xInfo->putAwayFolderID;		// int32
				}
				MacConvertToLVTime(&catInfo.createDate, &fileInfo->cDate);
				MacConvertToLVTime(&catInfo.contentModDate, &fileInfo->mDate);

				/* Ignore error for getting Desktop comments */
				ret = OSErrToLVErr(PBDTGetPath(&dtpb));
				if (!ret)
				{
					err = DSSetHandleSize((UHandle)comment, 255);
					if (!err)
					{
						LStrLen(*comment) = 255;
						dtpb.ioNamePtr = fss.name;
						dtpb.ioDirID = fss.parID;
						dtpb.ioDTBuffer = (UPtr)LStrBuf(*comment);
						dtpb.ioDTReqCount = 255;
						dtpb.ioDTActCount = 0;
						ret = PBDTGetCommentSync(&dtpb);
						LStrLen(*comment) = ret ? 0 : dtpb.ioDTActCount;
					}
				}
			}
		}
		else
		{
			DEBUGPRINTF(("FSGetCatalogInfo: err = %ld", err));
		}
    }
	else
	{
		DEBUGPRINTF(("FSMakePathRef: err = %ld", err));
    }
#elif Win32
    err = MakePathDSString(path, &lstr, 4);
    if (err)
		return err;

    if (FDepth(path) == 1)
    {
		*isDirectory = TRUE;
		fi.ftCreationTime.dwLowDateTime = fi.ftCreationTime.dwHighDateTime = 0;
		fi.ftLastWriteTime.dwLowDateTime = fi.ftLastWriteTime.dwHighDateTime = 0;
		fi.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    }
    else
    {
		handle = FindFirstFileW(UStrBuf(lstr), &fi);
		if (handle == INVALID_HANDLE_VALUE)
			err = Win32ToLVFileErr();
		else
		{
			*isDirectory = ((fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0);
			if (!FindClose(handle)) 
				err = Win32ToLVFileErr();
		}
    }

    if (!err)
    {
		if (write)
		{
				Win32ConvertFromLVTime(fileInfo->cDate, &fi.ftCreationTime);
			Win32ConvertFromLVTime(fileInfo->mDate, &fi.ftLastWriteTime);
			if (!SetFileTime(handle, &fi.ftCreationTime, &fi.ftLastAccessTime, &fi.ftLastWriteTime))
				err = Win32ToLVFileErr();
			fi.dwFileAttributes = GetFileAttributesA((LPCSTR)lstr->str);
			if (fi.dwFileAttributes != INVALID_FILE_ATTRIBUTES)
			{
				if (fileInfo->flags & 0x4000)
					fi.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
				else
					fi.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
				SetFileAttributesW(UStrBuf(lstr), fi.dwFileAttributes);
			}
		}
		else
		{
			fileInfo->type = kUnknownFileType;
			fileInfo->creator = kUnknownCreator;
			Win32ConvertToLVTime(fi.ftCreationTime, &fileInfo->cDate);
			Win32ConvertToLVTime(fi.ftLastWriteTime, &fileInfo->mDate);
			fileInfo->rfSize = 0;
			fileInfo->flags = (fi.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? 0x4000 : 0;
			if (*isDirectory)
			{
				count = 1;

				wcscpy(UStrBuf(lstr) + UStrLen(lstr), L"\\*.*");
				if (!err)
				{
					handle = FindFirstFileW(UStrBuf(lstr), &fi);
					if (handle == INVALID_HANDLE_VALUE)
						count = 0;
					else
						while (FindNextFileW(handle, &fi))
							count++;
					FindClose(handle);

					if (FDepth(path) == 1)
						fileInfo->size = count;
					else
						fileInfo->size = count - 2;
				}
			}
			else
			{
				fileInfo->size = Quad(fi.nFileSizeHigh, fi.nFileSizeLow);
			}
		}
    }
    DSDisposePtr((UPtr)lstr);
#elif Unix
    err = MakePathDSString(path, &lstr, 0);
    if (err)
		return err;

    if (stat((const char*)lstr->str, &statbuf))
		err = UnixToLVFileErr();

    if (!err)
    {
		*isDirectory = S_ISDIR(statbuf.st_mode);
		if (write)
		{
			buf.actime = statbuf.st_atime;
			buf.modtime = statbuf.st_mtime;

			UnixConvertFromLVTime(fileInfo->cDate, &statbuf.st_ctime);
			UnixConvertFromLVTime(fileInfo->mDate, &buf.modtime);
			if (utime((const char*)lstr->str, &buf))
				err = fIOErr;
		}
		else
		{
			fileInfo->type = kUnknownFileType;
			fileInfo->creator = kUnknownCreator;
			UnixConvertToLVTime(statbuf.st_ctime, &fileInfo->cDate);
			UnixConvertToLVTime(statbuf.st_mtime, &fileInfo->mDate);
			fileInfo->rfSize = 0;
			if (*isDirectory)
			{
				DIR *dirp;
				struct dirent *dp;

				if (!(dirp = opendir((const char*)lstr->str)))
					return UnixToLVFileErr();

				for (dp = readdir(dirp); dp; dp = readdir(dirp))
					count++;
				closedir(dirp);
				fileInfo->size = count - 2;
			}
			else
			{
				fileInfo->size = statbuf.st_size;
			}
		}
    }
#else
    err = mgNotSupported;
#endif
    return err;
}

LibAPI(MgErr) LVPath_EncodeMacbinary(Path srcPath, Path dstPath)
{
#if MacOS && !MacOSX
    MacSpec srcFSSpec;
    MacSpec dstFSSpec;
    MgErr  err;

    err = MakeMacSpec(srcPath, &srcFSSpec);
    if (!err)
    {
		err = MakeMacSpec(dstPath, &dstFSSpec);
		if (!err)
			err = OSErrToLVErr(EncodeMacbinaryFiles(&srcFSSpec, &dstFSSpec));
    }
    return err;
#else
    Unused(srcPath);
    Unused(dstPath);
    return mgNotSupported;
#endif
}

LibAPI(MgErr) LVPath_DecodeMacbinary(Path srcPath, Path dstPath)
{
#if MacOS && !MacOSX
    MacSpec srcFSSpec;
    MacSpec dstFSSpec;
    MgErr  err;

    err = MakeMacSpec(srcPath, &srcFSSpec);
    if (!err)
    {
		err = MakeMacSpec(dstPath, &dstFSSpec);
		if (!err)
			err = OSErrToLVErr(DecodeMacBinaryFiles(&srcFSSpec, &dstFSSpec));
    }
    return err;
#else
    Unused(srcPath);
    Unused(dstPath);
    return mgNotSupported;
#endif
}

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
				err = ConvertToPosixPath(*str, str, false);
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
	err = ConvertFromPosixPath(str, len, &hfsPath, isDir);
	if (!err)
	{
		err = FTextToPath(LStrBuf(*hfsPath), LStrLen(*hfsPath), path);
	}
#else
	Unused(isDir);
	err = FTextToPath(str, len, path);
#endif
	return err;
}

LibAPI(MgErr) LVPath_CreateLink(Path path, uInt32 flags, Path target)
{
    MgErr err = mgNoErr;
    LWStrPtr src = NULL;
    LWStrPtr tgt = NULL;

    if (!FIsAbsPath(path))
        return mgArgErr;

    err =  MakePathDSString(path, &src, 0);
    if (!err)
    {
        err = MakePathDSString(target, &tgt, 0);
        if (!err)
        {
#if MacOSX || Unix
            if (flags & kLinkHard)
            {
                if (link((const char*)UStrBuf(src), (const char*)UStrBuf(tgt)))
                    err = UnixToLVFileErr();
            }
            else
            {
                if (symlink((const char*)LStrBuf(src), (const char*)LStrBuf(tgt)))
                    err = UnixToLVFileErr();
            }
#elif Win32
            if (FExists(target))
            {
                FInfoRec finfo;
                err = FGetInfo(target, &finfo);
                if (!err && finfo.folder)
                    flags |= kLinkDir;
            }
            if (!err)
            {
				// Need to acquire backup privileges in order to be able to call symlink kernel entry points
				ModifyBackupPrivilege(TRUE);
                if (flags & kLinkHard)
                {
                    if (flags & kLinkDir)
                        err = mgNotSupported;
                    else if (!CreateHardLinkW(UStrBuf(src), UStrBuf(tgt), NULL))
                    {
                        err = Win32ToLVFileErr();
                    }
                }
                else if (!CreateSymbolicLinkW(UStrBuf(src), UStrBuf(tgt), flags & kLinkDir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0))
                {
                    err = Win32ToLVFileErr();
                }
 				ModifyBackupPrivilege(FALSE);
           }
#else
            err = mgNotSupported;
#endif
            DSDisposePtr((UPtr)tgt);
        }
        DSDisposePtr((UPtr)src);
    }
    return err;
}

LibAPI(MgErr) LVPath_ReadLink(Path path, Path *target, int32 *fileType)
{
    MgErr err = mgNoErr;
    LWStrPtr src = NULL;

    if (!FIsAbsPath(path))
        return mgArgErr;

    err = MakePathDSString(path, &src, 0);
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
                    ptr = realloc(buf, retval + LStrLen(*src) + 1);
                    if (ptr)
                    {
                        buf = ptr;
                        memmove(buf + LStrLen(*src) + 1, buf, retval);
                        memmove(buf, LStrBuf(*src), LStrLen(*src));
                        buf[LStrLen(*src)] = '/';
                        retval += LStrLen(*src) + 1;
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
#elif Win32
		LWStrPtr wTgt = NULL;
		DWORD dwAttr;
		err = Win32ResolveLink(src, &wTgt, FALSE, &dwAttr);
		if (!err)
		{
			LStrHandle handle = NULL;
			err = WideCStrToMultiByte((LPCWCHAR)wTgt, -1, &handle, CP_ACP, 0, NULL);
			if (!err)
				err = FTextToPath(LStrBuf(*handle), LStrLen(*handle), target);
			if (handle)
				DSDisposeHandle((UHandle)handle);
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
		}
        DSDisposePtr((UPtr)wTgt);
#else
        err = mgNotSupported;
#endif
        DSDisposePtr((UPtr)src);
    }
    return err;
}


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
		err = Win32ToLVFileErr();
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
		err = Win32ToLVFileErr();
		if (err)
			return err;
	}
	size->l.lo = SetFilePointer(ioRefNum, 0, (PLONG)&size->l.hi, FILE_END);
	if (size->l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32ToLVFileErr();
		if (err)
			return err;
	}
	tell.l.lo = SetFilePointer(ioRefNum, tell.l.lo, (PLONG)&tell.l.hi, FILE_BEGIN);
	if (tell.l.lo == INVALID_SET_FILE_POINTER /* && GetLastError() != NO_ERROR */)
	{
		err = Win32ToLVFileErr();
		if (err)
			return err;
	}
#endif
	return mgNoErr;
}

static MgErr lvfile_SetSize(FileRefNum ioRefNum, FileOffset *size)
{
#if usesPosixPath || usesWinPath
	FileOffset tell;
#if usesWinPath
	MgErr err = mgNoErr;
#endif
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
		err = Win32ToLVFileErr();
		if (err)
			return err;
	}
	size->l.lo = SetFilePointer(ioRefNum, size->l.lo, (PLONG)&size->l.hi, FILE_BEGIN);
	if (size->l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32ToLVFileErr();
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
				return Win32ToLVFileErr();
			}
		}
	}
	else
	{
		return Win32ToLVFileErr();
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
		err = Win32ToLVFileErr();
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
			err = Win32ToLVFileErr();
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
			return Win32ToLVFileErr();
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
		return Win32ToLVFileErr();
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
		return Win32ToLVFileErr();
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
		return Win32ToLVFileErr();
    }
	if (outCount)
		*outCount = actCount;
#endif
    return err;
}

#if MacOSX && usesPosixPath
static char *namedResourceFork = "/..namedfork/rsrc";
#endif

LibAPI(MgErr) LVFile_OpenFile(LVRefNum *refnum, Path path, uInt8 rsrc, uInt32 openMode, uInt32 denyMode)
{
    MgErr err;
	int32 type;
	FileRefNum ioRefNum;
#if usesHFSPath
    FSRef ref;
	HFSUniStr255 forkName;
    int8 perm;
    OSErr ret;
#elif usesPosixPath
    LStrPtr lstr = NULL;
    struct stat statbuf;
    char theMode[4];
#elif usesWinPath
    UStrPtr lstr = NULL;
    DWORD shareAcc, openAcc;
    DWORD createMode = OPEN_EXISTING;
    int32 attempts;\

	ioRefNum = INVALID_HANDLE_VALUE;
#endif
    *refnum = 0;
    
	err = FGetPathType(path, &type);
    if (err)
		return err;
	
	if ((type != fAbsPath) && (type != fUNCPath)) 
		return mgArgErr;
#if usesWinPath
    if (FDepth(path) == 1L)
		return mgArgErr;

    if (rsrc)
		return mgNotSupported;

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

    err = MakePathDSString(path, &lstr, 0);
    if (err)
		return err;
    /* Open the specified file. */
    attempts = 3;

    while (attempts)
	{
		ioRefNum = CreateFileW(UStrBuf(lstr), openAcc, shareAcc, 0, createMode, FILE_ATTRIBUTE_NORMAL, 0);
		if (ioRefNum == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION)
		{
			if (--attempts > 0)
				Sleep(50);
		}
		else
			break;
    }
    DSDisposePtr((UPtr)lstr);

    if (ioRefNum == INVALID_HANDLE_VALUE)
	{
		err = Win32ToLVFileErr();
	}
#elif usesPosixPath
 #if Unix
	if (rsrc)
	{
		return mgNotSupported;
	}
	else
 #endif
	{
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
		if (rsrc)
		{
			err = MakePathDSString(path, &lstr, strlen(namedResourceFork));
			if (!err)
			{
				strcpy((char*)(LStrBuf(lstr) + LStrLen(lstr)), namedResourceFork);
			}
		}
		else
 #else
			err = MakePathDSString(path, &lstr, 0);
 #endif
		if (err)
			return err;

		/* Test for file existence first to avoid creating file with mode "w". */
		if (openMode == openWriteOnlyTruncate && stat((const char*)lstr->str, &statbuf))
		{
			DSDisposePtr((UPtr)lstr);
			return fNotFound;
		}

		errno = 0;
		ioRefNum = fopen((const char*)LStrBuf(lstr), (char *)theMode);
		DSDisposePtr((UPtr)lstr);
		if (!ioRefNum)
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
			if (fcntl(fileno(ioRefNum), F_SETLK, FCNTL_PARAM3_CAST(&lockInfo)) == -1)
			{
				err = UnixToLVFileErr();
				fclose(ioRefNum);
			}
		}
 #endif
 	}
#elif usesHFSPath
    err = FSMakePathRef(path, &ref);
    if (err)
	{
		DEBUGPRINTF(((CStr)"FSMakePathRef: err = %ld", err));
		return err;
	}

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
		ret = FSOpenFork(&ref, forkName.length, forkName.unicode, perm, &ioRefNum);
	}
	err = OSErrToLVErr(ret);
	if (!err && openMode == openWriteOnlyTruncate)
	{
		FileOffset size;
		size.q = 0;
		err = lvfile_SetSize(ioRefNum, &size);
	}
#else
	err = mgNotSupported;
#endif
	if (!err)
	{
		err = FNewRefNum(path, (File)(ioRefNum), refnum);
	}
	if (err)
	{
		lvfile_CloseFile(ioRefNum);
		DEBUGPRINTF(((CStr)"OpenFile: err = %ld, rsrc = %d", err, (int16)rsrc));
	}
	return err;
}

LibAPI(MgErr) LVFile_CloseFile(LVRefNum *refnum)
{
	FileRefNum ioRefNum;
	MgErr err = FRefNumToFD(*refnum, (File*)&ioRefNum);
	if (!err)
	{
		err = lvfile_CloseFile(ioRefNum);
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

LibAPI(uInt32) determine_codepage(uLong *flags, LStrHandle string)
{
	uInt32 cp = Hi16(*flags);
	if (!cp)
	{
		int32 i = LStrLen(*string) - 1;
		UPtr ptr = LStrBuf(*string);

		cp = CP_OEMCP;
		// No codepage defined, try to find out if we have extended characters and set UTF_* in that case
		for (; i >= 0; i--)
		{
			if (ptr[i] >= 0x80)
			{
				cp = CP_UTF8;
				break;
			}
		}
	}
	return cp;
}

LibAPI(MgErr) ConvertCString(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed)
{
	MgErr err;
	if (srccp != destcp)
	{
		UStrHandle ustr = NULL;
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
		err = NumericArrayResize(uB, 1, (UHandle*)dest, srclen);
		if (!err)
		{
			MoveBlock(src, LStrBuf(**dest), srclen);
			LStrLen(**dest) = srclen;
			return err;
		}
	}
	if (*dest)
		LStrLen(**dest) = 0;
	return err;
}

/* Converts a Unix style path to a LabVIEW platform path */
LibAPI(MgErr) ConvertCPath(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir)
{
    MgErr err = mgNotSupported;
#if usesWinPath
    Unused(isDir);
    err = ConvertCString(src, srclen, srccp, dest, destcp, defaultChar, defUsed);
	if (!err)
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
    Unused(defaultChar);
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
			if (CFStringGetBytes(hfsRef, range, encoding, 0, false, NULL, 0, &len) > 0)
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
		UStrHandle ustr = NULL;
		if (LStrLen(*src) > 0)
		{
			err = MultiByteToWideString(src, &ustr, srccp);
			if (!err && ustr)
			{
				err = WideStringToMultiByte(ustr, dest, destcp, defaultChar, defUsed);
				DSDisposeHandle((UHandle)ustr);
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
LibAPI(MgErr) ConvertLPath(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir)
{
    MgErr err = mgNotSupported;
#if usesWinPath
    Unused(isDir);
	err = ConvertLString(src, srccp, dest, destcp, defaultChar, defUsed);
	if (!err)
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
#elif usesPosixPath
    Unused(isDir);
	err = ConvertLString(src, srccp, dest, destcp, defaultChar, defUsed);
#elif usesHFSPath
    Unused(defaultChar);
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
		urlRef = CFURLCreateWithFileSystemPath(NULL, fileRef, kCFURLHFSPathStyle, isDir);
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
			if (CFStringGetBytes(posixRef, range, encoding, 0, false, NULL, 0, &len) > 0)
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

static MgErr unix_convert_mbtow(const char *src, int32 len, UStrHandle *dest, uInt32 codePage)
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
			    LStrLen(**dest) = (len * sizeof(wchar_t) - outleft) / sizeof(uInt16);
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
static MgErr unix_convert_mbtow(const char *src, int32 len, UStrHandle *dest, uInt32 codePage)
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
				err = utf8towchar(src, len, UStrBuf(**dest), NULL, offset);
			}
			else
			{
				offset = mbstowcs(UStrBuf(**dest), src, offset);
			}
			if (!err)
			    LStrLen(**dest) = offset * sizeof(wchar_t) / sizeof(uInt16); 
		}
	}
	return err;
}

static MgErr unix_convert_wtomb(const wchar_t *src, int32 srclen, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	size_t dummy, length, size = 2 * srclength;
	wchar_t wdefChar;
	MgErr err = noErr;

	if (codePage == CP_UTF8)
		err = wchartoutf8(src, srclen, NULL, &size, 0);
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
			wchar_t *sbuf = src, *send = sbuf + length;
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

LibAPI(MgErr) MultiByteCStrToWideString(ConstCStr src, int32 srclen, UStrHandle *dest, uInt32 codePage)
{
	if (*dest)
		LStrLen(**dest) = 0;
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
        Unused(srclen);
		CFStringEncoding encoding = ConvertCodepageToEncoding(codePage);
		if (encoding != kCFStringEncodingInvalidId)
		{
			CFStringRef cfpath = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, (const char)src, encoding, kCFAllocatorNull);
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

LibAPI(MgErr) MultiByteToWideString(const LStrHandle src, UStrHandle *dest, uInt32 codePage)
{
	if (*dest)
		LStrLen(**dest) = 0;
	if (LStrLen(*src) > 0)
	{
		MgErr err = mgNotSupported;
#if Win32
		int32 numChars = MultiByteToWideChar(codePage, 0, (LPCSTR)LStrBuf(*src), LStrLen(*src), NULL, 0);
		if (numChars > 0)
		{ 
			err = NumericArrayResize(uW, 1, (UHandle*)dest, numChars);
			if (!err)
			{
				LStrLen(**dest) = MultiByteToWideChar(codePage, 0, (LPCSTR)LStrBuf(*src), LStrLen(*src), LStrBuf(**dest), numChars);	
			}
		}
#elif MacOSX
		CFStringEncoding encoding = ConvertCodepageToEncoding(codePage);
		if (encoding != kCFStringEncodingInvalidId)
		{
			CFStringRef cfpath = CFStringCreateWithBytes(kCFAllocatorDefault, LStrBuf(*src), LStrLen(*src), encoding, false);
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
		err = unix_convert_mbtow((const char*)LStrBuf(*src), LStrLen(*src), dest, codePage);
#endif
		return err;
	}
	return mgNoErr;
}

LibAPI(MgErr) ZeroTerminateLString(LStrHandle *dest)
{
	int32 size = dest ? LStrLen(**dest) : 0;
	MgErr err = NumericArrayResize(uB, 1, (UHandle*)dest, size + 1);
	if (!err)
		LStrBuf(**dest)[size] = 0;
	return err;
}

LibAPI(MgErr) WideStringToMultiByte(const UStrHandle src, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	return WideCStrToMultiByte(UStrBuf(*src), UStrLen(*src), dest, codePage, defaultChar, defaultCharWasUsed);
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
		CFStringRef cfpath = CFStringCreateWithCharacters(NULL, src, srclen));
		if (cfpath)
		{
			CFMutableStringRef cfpath2 = CFStringCreateMutableCopy(NULL, 0, cfpath);
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
