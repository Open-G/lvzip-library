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
 #include "iowin.h"
 #ifndef INVALID_FILE_ATTRIBUTES
  #define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
 #endif
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

 #define MacSpec	FSRef
 #define MacIsInvisible(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible)
 #define MacIsInvFolder(cpb) ((cpb).dirInfo.ioDrUsrWds.frFlags & kIsInvisible)
 #define MacIsDir(cpb)   ((cpb).nodeFlags & ioDirMask)
 #define MacIsStationery(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsStationery)
 #define MacIsAlias(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsAlias)
 #define kFileChanged    (1L<<7)
 static MgErr OSErrToLVErr(OSErr err);
#endif

#if MacOSX
static MgErr ConvertToPosixPath(const LStrHandle hfsPath, LStrHandle *posixPath, Boolean isDir)
{
    MgErr err = mFullErr;
    CFURLRef urlRef = NULL;
    CFStringRef fileRef, posixRef;
    uInt32 encoding = CFStringGetSystemEncoding();

	if (!posixPath)
		return mgArgErr;

	if (*posixPath)
		LStrLen(**posixPath) = 0;

	fileRef = CFStringCreateWithBytes(kCFAllocatorDefault, LStrBuf(*hfsPath), LStrLen(*hfsPath), encoding, false);
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

static MgErr FSMakePathRef(Path path, FSRef *ref)
{
	LStrHandle str = NULL;
    MgErr err = LVPath_ToText(path, &str);
    DEBUGPRINTF(("FPathToText1: path = %z", path));
	if (!err)
	{
	    err = OSErrToLVErr(FSPathMakeRef(LStrBuf(*str), ref, NULL));
		if (err)
		{
			DEBUGPRINTF(("FSPathMakeRef: err = %ld, len = %ld, path = %s", err, LStrLen(*str), LStrBuf(*str)));
		}
		DSDisposeHandle((UHandle)str);
    }
    return err;
}

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
#elif Unix || Win32
static int32 MakePathDSString(Path path, LStrPtr *lstr, int32 reserve)
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
    }
    return fIOErr;   /* fIOErr generally signifies some unknown file error */
}

#else
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <utime.h>

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

LibAPI(MgErr) LVPath_HasResourceFork(Path path, LVBoolean *hasResFork, uInt32 *sizeLow, uInt32 *sizeHigh)
{
    MgErr  err = mgNoErr;
#if MacOSX
    FSRef ref;
#else
    Unused(path);
#endif
	*hasResFork = 0;
	if (sizeLow)
		*sizeLow = 0;
	if (sizeHigh)
		*sizeHigh = 0;
#if MacOSX
    err = FSMakePathRef(path, &ref);
    if (!err)
    {
		FSCatalogInfoBitmap whichInfo =  kFSCatInfoNodeFlags | kFSCatInfoRsrcSizes;
		FSCatalogInfo		catalogInfo;
 
		/* get nodeFlags and catalog info */
		err = OSErrToLVErr(FSGetCatalogInfo(&ref, whichInfo, &catalogInfo, NULL, NULL,NULL));
		if (!err && catalogInfo.nodeFlags & kFSNodeIsDirectoryMask)
			err = fIOErr;
		if (!err)
		{
			if (hasResFork)
				*hasResFork = catalogInfo.rsrcLogicalSize > 0;
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
	FSRef ref;
#elif Win32
    LStrPtr lstr;
    HANDLE handle = NULL;
    WIN32_FIND_DATAA fi;
    uInt64 count = 0;
#elif Unix
    LStrPtr lstr;
    struct stat statbuf;
    struct utimbuf buf;
    uInt64 count = 0;
#endif

    if (!path || !comment)
      return mgArgErr;

#if MacOSX
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
		handle = FindFirstFileA((LPCSTR)lstr->str, &fi);
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
				SetFileAttributesA((LPCSTR)lstr->str, fi.dwFileAttributes);
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
				Path temp = path;

				count = 1;

				strcpy(LStrBuf(lstr) + LStrLen(lstr), "\\*.*");
				if (!err)
				{
					handle = FindFirstFileA((LPCSTR)LStrBuf(lstr), &fi);
					if (handle == INVALID_HANDLE_VALUE)
						count = 0;
					else
						while (FindNextFile(handle, &fi))
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
				fileInfo->size = Quad(fi.nFileSizeLow, fi.nFileSizeHigh);
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

			/* No modification of creation time in Unix?
			UnixConvertFromLVTime(fileInfo->cDate, &statbuf.st_ctime);
			*/
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
#if MacOS
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
#if MacOS
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
#if MacOSX
			if (!err)
				err = ConvertToPosixPath(*str, str);
#endif
		}
	}
	return err;
}

#if MacOSX
#define FileRefNum SInt16
#elif Unix
#define FileRefNum FILE*
#elif Win32
#define FileRefNum HANDLE
#endif

static MgErr lvfile_CloseFile(FileRefNum ioRefNum)
{
	MgErr err = mgNoErr;
#if MacOSX
	err = OSErrToLVErr(FSCloseFork(ioRefNum);
#elif Unix
	if (fclose(ioRefNum))
	{
		err = UnixToLVFileErr();
	}
#elif Win32
	if (!CloseHandle(ioRefNum))
	{
		err = Win32ToLVFileErr();
	}
#endif
	return err;
}

static MgErr lvfile_GetSize(FileRefNum ioRefNum, FileOffset *size)
{
#if Unix || Win32
	FileOffset tell;
#if Win32
	MgErr err;
#endif
#endif
	if (0 == ioRefNum)
		return mgArgErr;
	size->q = 0;
#if MacOSX
	return OSErrToLVErr(FSGetForkSize(ioRefNum, &(size->q)));
#elif Unix
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
#elif Win32
	tell.q = 0;
	tell.l.lo = SetFilePointer(ioRefNum, 0, &tell.l.hi, FILE_CURRENT);
	if (tell.l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32ToLVFileErr();
		if (err)
			return err;
	}
	size->l.lo = SetFilePointer(ioRefNum, 0, &size->l.hi, FILE_END);
	if (size->l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32ToLVFileErr();
		if (err)
			return err;
	}
	tell.l.lo = SetFilePointer(ioRefNum, tell.l.lo, &tell.l.hi, FILE_BEGIN);
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
#if Unix || Win32
	FileOffset tell;
#if Win32
	MgErr err = mgNoErr;
#endif
#endif
	if (0 == ioRefNum)
		return mgArgErr;
	if (size->q < 0)
		return mgArgErr;
#if MacOSX
	return OSErrToLVErr(FSSetForkSize(ioRefNum, fsFromStart, size->q));
#elif Unix
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
#elif Win32
	tell.q = 0;
	tell.l.lo = SetFilePointer(ioRefNum, 0, &tell.l.hi, FILE_CURRENT);
	if (tell.l.lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		err = Win32ToLVFileErr();
		if (err)
			return err;
	}
	size->l.lo = SetFilePointer(ioRefNum, size->l.lo, &size->l.hi, FILE_BEGIN);
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
			tell.l.lo = SetFilePointer(ioRefNum, tell.l.lo, &tell.l.hi, FILE_BEGIN);
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
#if MacOSX
	OSErr ret;
#elif Unix || Win32
	FileOffset size, sought, tell;
#if Win32
	MgErr err = mgNoErr;
#endif
#endif
	if (0 == ioRefNum)
		return mgArgErr;

	if ((offs->q == 0) && (mode == fCurrent))
		return noErr;
#if MacOSX
	ret = FSSetForkPosition(ioRefNum, mode, offs.q);
	if (ret == posErr)
	{
		ret = FSSetForkPosition(ioRefNum, fsFromStart, 0);
	}
	if (ret == eofErr)
	{
		ret = FSSetForkPosition(ioRefNum, fsFromLEOF, 0);
	}
	return OSErrToLVErr(ret);
#elif Unix
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
#elif Win32
	size.l.lo = GetFileSize(ioRefNum, &size.l.hi);
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
		tell.l.lo = SetFilePointer(ioRefNum, 0, &tell.l.hi, FILE_CURRENT);
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
		sought.l.lo = SetFilePointer(ioRefNum, sought.l.lo, &sought.l.hi, FILE_BEGIN);
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
#if MacOSX
	return OSErrToLVErr(FSGetForkPosition(ioRefNum, tell));
#elif Unix
	errno = 0;
	tell->q = ftello64(ioRefNum);
	if (tell->q == -1)
	{
		return UnixToLVFileErr();
	}
#elif Win32
	tell->l.hi = 0;
	tell->l.lo = SetFilePointer(ioRefNum, 0, &tell->l.hi, FILE_CURRENT);
	if (tell->l.lo == INVALID_SET_FILE_POINTER /* && GetLastError() != NO_ERROR */)
	{
		return Win32ToLVFileErr();
	}
#endif
	return mgNoErr;
}

static MgErr lvfile_Read(FileRefNum ioRefNum, uInt32 inCount, uInt32 *outCount, UPtr buffer)
{
#if MacOSX
	OSErr	err;
#elif Unix
	MgErr	err = mgNoErr;
	int		actCount;
#elif Win32
	Bool32	readSuccess;
	int		actCount;
#endif
	if (0 == ioRefNum)
		return mgArgErr;
#if MacOSX
	err = FSRead(ioRefNum, &inCount, buffer);
	if (outCount)
	{
		if (err && err != eofErr)
		{
			*outCount = 0L;
		}
		else
		{
			*outCount = inCount;
		}
	}
	return OSErrToLVErr(err);
#elif Unix
	errno = 0;
	actCount = fread((char *)buffer, 1, inCount, ioRefNum);
	if (ferror(ioRefNum))
	{
		err = fIOErr;
		clearerr(ioRefNum);
	}
	else if (feof(ioRefNum))
	{
		err = fEOF;
		clearerr(ioRefNum);
	}
	if (outCount)
		*outCount = actCount;
	return err;
#elif Win32
	readSuccess = ReadFile(ioRefNum, buffer, inCount, &actCount, NULL);
	if (outCount)
	{
		*outCount = actCount;
	}
	if (!readSuccess)
	{
		return Win32ToLVFileErr();
	}

	if (actCount == inCount)
	{
		return mgNoErr;
	}
	else
	{
		return fEOF;
	}
#endif
}

static MgErr lvfile_Write(FileRefNum ioRefNum, uInt32 inCount, uInt32 *outCount, UPtr buffer)
{
#if MacOSX
	OSErr err;
#elif Unix || Win32
	MgErr err = mgNoErr;
	int actCount;
#endif

	if (0 == ioRefNum)
		return mgArgErr;
#if Mac
	err = FSWrite(ioRefNum, &inCount, buffer);
	if (outCount)
	{
		if (err && err != dskFulErr)
		{
			*outCount = 0L;
		}
		else
		{
			*outCount = inCount;
		}
	}
	return OSErrToLVErr(err);
#elif Win32
	if (!WriteFile(ioRefNum, buffer, inCount, &actCount, NULL))
		err = Win32ToLVFileErr();
	if (outCount)
		*outCount = actCount;
	return err;
#elif Unix
	errno = 0;
	actCount = fwrite((char *)buffer, 1, inCount, ioRefNum);
	if (ferror(ioRefNum))
	{
		err = fIOErr;
		clearerr(ioRefNum);
	}
	if (outCount)
		*outCount = actCount;
	return err;
#endif
}

LibAPI(MgErr) LVFile_OpenFile(LVRefNum *refnum, Path path, uInt8 rsrc, uInt32 openMode, uInt32 denyMode)
{
    MgErr err;
	int32 type;
	FileRefNum ioRefNum;
#if MacOSX
    FSRef ref;
	HFSUniStr255 forkName;
    int8 perm;
    OSErr ret;
#elif Win32
    LStrPtr lstr;
    DWORD shareAcc, openAcc;
    DWORD createMode = OPEN_EXISTING;
    int32 attempts;
#elif Unix
    LStrPtr lstr;
    uChar theMode[3];
    struct stat statbuf;
#endif
    *refnum = 0;
    
	err = FGetPathType(path, &type);
    if (err)
		return err;
	
	if ((type != fAbsPath) && (type != fUNCPath)) 
		return mgArgErr;
#if Win32
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
		ioRefNum = CreateFile((LPCSTR)lstr->str, openAcc, shareAcc, 0, createMode, FILE_ATTRIBUTE_NORMAL, 0);
		if (ioRefNum == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION)
		{
			if (--attempts > 0)
				Sleep(50);
		}
		else
			attempts = 0;
    }
    DSDisposePtr((UPtr)lstr);

    if (ioRefNum == INVALID_HANDLE_VALUE)
	{
		err = Win32ToLVFileErr();
	}
#elif Unix || MacOSX
 #if Unix
	if (rsrc)
	{
		return mgNotSupported;
	}
	else
	{
 #else
	if (rsrc & 0x2)
	{
 #endif
		switch (openMode)
		{
			case openWriteOnly:
				/* Treat write-only as read-write, since you can't open a file for write-only
				   using buffered i/o functions without truncating the file. */
			case openReadWrite:
				StrCpy(theMode, (uChar *)"r+");
				break;
			case openReadOnly:
				StrCpy(theMode, (uChar *)"r");
				break;
			case openWriteOnlyTruncate:
				StrCpy(theMode, (uChar *)"w");
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

		err = MakePathDSString(path, &lstr, 5);
		if (!err && rsrc & 0x1)
		{
			strcpy((char*)(LStrBuf(lstr) + LStrLen(lstr)), "/rsrc");
		}
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
 #if MacOSX
	else
	{
		if (err = FSMakePathRef(path, &ref))
		{
			DEBUGPRINTF(("FSMakePathRef: err = %ld", err));
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
		if (!err && if (openMode == openWriteOnlyTruncate)
		{
			FileOffset size;
			size.q = 0;
			err = lvfile_SetSize(ioRefNum, &size);
		}
	}
 #endif
#else
	err = mgNotSupported;
#endif
	if (!err)
	{
		err = FNewRefNum(path, (File)ioRefNum, refnum);
	}
	if (err)
	{
		lvfile_CloseFile(ioRefNum);
		DEBUGPRINTF(("OpenFile: err = %ld, rsrc = %d", err, (int16)rsrc));
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

LibAPI(MgErr) LVFile_SetFilePos(LVRefNum *refnum, FileOffset *offs, int32 mode)
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
		if (codePage == CP_ACP)
			encoding = GetApplicationTextEncoding();
		else if (codePage == CP_OEMCP)
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
	CFStringEncoding encoding = acp ? GetApplicationTextEncoding() : CFStringGetSystemEncoding();
	return CFStringConvertEncodingToWindowsCodepage(encoding);
#else
    if (utf8_is_current_mbcs())
		return CP_UTF8;
	return acp ? CP_ACP : CP_OEMCP;
#endif
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
LibAPI(MgErr) ConvertCPath(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed)
{
    MgErr err = mgNotSupported;
#if Win32
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
#elif Unix
	err = ConvertCString(src, srclen, srccp, dest, destcp, defaultChar, defUsed);
#elif MacOSX
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
		hsfRef = CFURLCopyFileSystemPath(urlRef, kCFURLHFSPathStyle);
		CFRelease(urlRef);
		if (hsfRef)
		{
			CFIndex len;
			CFRange range = CFRangeMake(0, CFStringGetLength(posixRef));
			if (CFStringGetBytes(hsfRef, range, encoding, 0, false, NULL, 0, &len) > 0)
			{
				if (len > 0)
				{
					err = NumericArrayResize(uB, 1, (UHandle*)dest, len + 1);
					if (!err)
					{
						encoding = ConvertCodepageToEncoding(destcp);
						if (encoding != kCFStringEncodingInvalidId)
						{
							if (CFStringGetBytes(hsfRef, range, encoding, 0, false, LStrBuf(**dest), len, &len) > 0)
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
			CFRelease(hsfRef);
		}
		else
		{
			err = mgFullErr;
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
		return DSCopyHandle(*dest, src);
	}
	if (*dest)
		LStrLen(**dest) = 0;

	return mgNoErr;
}

/* Converts a LabVIEW platform path to Unix style path */
LibAPI(MgErr) ConvertLPath(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed)
{
    MgErr err = mgNotSupported;
#if Win32
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
#elif Unix
	err = ConvertLString(src, srccp, dest, destcp, defaultChar, defUsed);
#elif MacOSX
	CFStringEncoding encoding = ConvertCodepageToEncoding(srccp);
	if (encoding != kCFStringEncodingInvalidId)
	{
		CFURLRef urlRef = NULL;
		CFStringRef posixRef, fileRef = CFStringCreateWithBytes(kCFAllocatorDefault, LStrBuf(*src), LStrLen(*src), encoding, false);
		if (!fileRef)
		{
			return mFullErr;
		}
		urlRef = CFURLCreateWithFileSystemPath(NULL, fileRef, kCFURLHSFPathStyle, isDir);
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
			CFRelease(posixRef);
		}
		else
		{
			err = mgFullErr;
		}
	}
#endif
	return err;
}

#if Unix
#define UStrBuf(s)	(wchar_t*)LStrBuf(s)
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

static MgErr unix_convert_wtomb(UStrHandle src, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	iconv_t cd = iconv_open(iconv_getcharset(codePage), "WCHAR_T");
	Unused(defaultChar);
	Unused(defaultCharWasUsed);
	if (cd != (iconv_t)-1)
	{
		MgErr err = NumericArrayResize(uB, 1, (UHandle*)dest, LStrLen(*src) * sizeof(uInt16) * 2);
		if (!err)
		{
			char *inbuf = (char*)UStrBuf(*src), *outbuf = (char*)LStrBuf(**dest);
			size_t retval, inleft = LStrLen(*src) * sizeof(uInt16), outleft = LStrLen(*src) * sizeof(uInt16) * 2;
			retval = iconv(cd, &inbuf, &inleft, &outbuf, &outleft);
			if (retval == (size_t)-1)
				err = UnixToLVFileErr();
			else
				LStrLen(**dest) = LStrLen(*src) * 2 - outleft;
		}
		if (iconv_close(cd) != 0 && !err)
			err = UnixToLVFileErr();
		return err;
	}
	return UnixToLVFileErr();
}
#else
/* We don't have a good way of converting from an arbitrary character set to wide char and back.
   Just do default mbcs to wide char and vice versa ??? */
static MgErr unix_convert_mbtow(const char *src, int32 len, UStrHandle *dest, uInt32 codePage)
{
	int32 offset = 0;

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
				offset = mbstowcs(UStrBuf(**dest), LStrBuf(*src), offset);
			}
			if (!err)
			    LStrLen(**dest) = offset * sizeof(wchar_t) / sizeof(uInt16); 
		}
	}
}
static MgErr unix_convert_wtomb(UStrHandle src, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
		size_t length = LStrLen(*src) * sizeof(uInt16) / sizeof(wchar_t);
		size_t dummy, size = 2 * length;
		wchar_t wdefChar;

		if (codePage == CP_UTF8)
			err = utfwchartoutf8(src, len, NULL, &offset, 0);
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
			wchar_t *sbuf = UStrBuf(*src), *send = sbuf + length;
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
		int32 numChars = MultiByteToWideChar(codePage, 0, src, srclen, NULL, 0);
		if (numChars > 0)
		{ 
			err = NumericArrayResize(uW, 1, (UHandle*)dest, numChars);
			if (!err)
			{
				LStrLen(**dest) = MultiByteToWideChar(codePage, 0, src, srclen, LStrBuf(**dest), numChars);	
			}
		}
#elif MacOSX
		CFStringEncoding encoding = ConvertCodepageToEncoding(codePage);
		if (encoding != kCFStringEncodingInvalidId)
		{
			CFStringRef cfpath = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, src, encoding, kCFAllocatorNull);
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
		int32 numChars = MultiByteToWideChar(codePage, 0, LStrBuf(*src), LStrLen(*src), NULL, 0);
		if (numChars > 0)
		{ 
			err = NumericArrayResize(uW, 1, (UHandle*)dest, numChars);
			if (!err)
			{
				LStrLen(**dest) = MultiByteToWideChar(codePage, 0, LStrBuf(*src), LStrLen(*src), LStrBuf(**dest), numChars);	
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

#ifndef Unix
static void TerminateLStr(LStrHandle *dest, int32 numBytes)
{
	LStrLen(**dest) = numBytes;
	LStrBuf(**dest)[numBytes] = 0;
}
#endif

LibAPI(MgErr) WideStringToMultiByte(const UStrHandle src, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	if (dest && *dest)
		LStrLen(**dest) = 0;
	if (defaultCharWasUsed)
		*defaultCharWasUsed = LV_FALSE;
	if (LStrLen(*src) > 0)
	{
		MgErr err = mgNotSupported;
#if Win32 
		int32 numBytes = WideCharToMultiByte(codePage, 0, LStrBuf(*src), LStrLen(*src), NULL, 0, NULL, NULL);
		if (numBytes > 0)
		{ 
			BOOL defUsed;
			err = NumericArrayResize(uB, 1, (UHandle*)dest, numBytes + 1);
			if (!err)
			{
				numBytes = WideCharToMultiByte(codePage, 0, LStrBuf(*src), LStrLen(*src), LStrBuf(**dest), numBytes, &defaultChar, &defUsed);
				TerminateLStr(dest, numBytes);
				if (defaultCharWasUsed)
					*defaultCharWasUsed = (defUsed != FALSE);
			}
		}
#elif MacOSX
		CFStringRef cfpath = CFStringCreateWithCharacters(NULL, LStrBuf(*src), LStrLen(*src));
		if (cfpath)
		{
			CFMutableStringRef cfpath2 = CFStringCreateMutableCopy(NULL, 0, cfpath);
			CFRelease(cfpath);
			if (cpath2)
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
		err = unix_convert_wtomb(src, dest, codePage, defaultChar, defaultCharWasUsed);
#endif
		return err;
	}
	return mgNoErr;
}
