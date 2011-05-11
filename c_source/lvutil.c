/* lvutil.c -- support functions for LabVIEW ZIP library

   Version 1.9, Sept 17th, 2007

   Copyright (C) 2002-2007 Rolf Kalbermatter
*/

#include <stdio.h>
#include <string.h>
#define ZLIB_INTERNAL
#include "zlib.h"
#include "ioapi.h"
#include "lvutil.h"
#include "iomem.h"

#if MacOS
#include <CoreServices/CoreServices.h>  /* instead of #include <Resources.h> */
#include "MacBinaryIII.h"

#define MacIsInvisible(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible)
#define MacIsInvFolder(cpb) ((cpb).dirInfo.ioDrUsrWds.frFlags & kIsInvisible)
#define MacIsDir(cpb)   ((cpb).dirInfo.ioFlAttrib & ioDirMask)
#define MacIsStationery(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsStationery)
#define MacIsAlias(cpb) ((cpb).hFileInfo.ioFlFndrInfo.fdFlags & kIsAlias)
#define kFileChanged    (1L<<7)

static MgErr MakeMacSpec(Path path, FSSpec *fss);
static OSErr MakeFileSpec(int16 vol, int32 dirID, ConstStr255 path, FSSpec *fss);
static MgErr OSErrToLVErr(OSErr err);
#elif Unix
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#elif Win32
#include "iowin.h"
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
#endif

#if MacOS
static OSErr FSpLocationFromFullPath(CStr fullPath,
                                     int32 fullPathLength,
                                     FSSpec *spec)
{
    AliasHandle alias;
    OSErr       err;
    Boolean     wasChanged;
    Str32       nullString;

    /* The value 99 below has been determined by testing so it really is a hack that might
      or might not work everywhere. For now we just go with that and see where it leads. */
    if (fullPathLength < 99)
    {
      StringPtr myString = (StringPtr)DSNewPtr(fullPathLength + 1);
        
      MoveBlock(fullPath, myString + 1, fullPathLength);
      myString[0] = (uInt8)fullPathLength;
    
      err = FSMakeFSSpec(0, 0L, myString, spec);
      if (err)
      {
        DEBUGPRINTF(("FSMakeFSSpec: err = %d", err));
        if (fnfErr == err)
        {
          /* fileNotFound is expected and normal. */
          err = mgNoErr;
        }
      }
      DSDisposePtr(myString);
    }
    else
    {
      /* Create a minimal alias from the full pathname */
      nullString[0] = 0;    /* null string to indicate no zone or server name */
      err = NewAliasMinimalFromFullPath(fullPathLength, fullPath, nullString, nullString, &alias);
      if (err == noErr)
      {
        /* Let the Alias Manager resolve the alias. */
        err = ResolveAlias(NULL, alias, spec, &wasChanged);
        if (err)
        {
          DEBUGPRINTF(("ResolveAlias: err = %d", err));
        }
        /* work around Alias Mgr sloppy volume matching bug */
        if (spec->vRefNum == 0)
        {
          /* invalidate wrong FSSpec */
          spec->parID = 0;
          spec->name[0] =  0;
          err = nsvErr;
        }
        DisposeHandle((Handle)alias);   /* Free up memory used */
      }
    }
    return (err);
}

static MgErr MakeMacSpec(Path path, FSSpec *fss)
{
    MgErr err;
    int32 pathLen = -1;

    DEBUGPRINTF(("FPathToText1: path = %z", path));
    err = FPathToText(path, (LStrPtr)&pathLen);
    if (!err)
    {
      LStrPtr lstr;

      lstr = (LStrPtr)DSNewPClr(sizeof(int32) + pathLen + 1);
      if (!lstr)
        return mFullErr;

      lstr->cnt = pathLen;
      err = FPathToText(path, lstr);
      lstr->str[lstr->cnt] = 0;
      if (!err)
      {
        err = OSErrToLVErr(FSpLocationFromFullPath(lstr->str, lstr->cnt, fss));
        if (err)
          DEBUGPRINTF(("FSpLocationFromFullPath: err = %ld, len = %ld, path = %s",
                       err, lstr->cnt, lstr->str));
      }
      DSDisposePtr(lstr);
    }
    return err;
}

static OSErr MakeFileSpec(int16 vRefNum, int32 dirID, ConstStr255 path, FSSpec *fss)
{
    OSErr err = FSMakeFSSpec(vRefNum, dirID, path, fss);
    if (err)
    {
      DEBUGPRINTF(("FSMakeFSSpec: err = %ld", err));
      if (fnfErr == err)
      {
        /* fileNotFound is expected and normal. */
        err = mgNoErr;
      }
      else if (bdNamErr == err)
      {
        /* MacOSX seems to return sometimes bdNamErr for longer filenames,
         so we try to catch that too. */
        Str255 temp;
        
        DEBUGPRINTF(("MakeFileSpec: trying with subpaths"));

		if (!vRefNum)
        {
          HParamBlockRec hpb;

          temp[0] = 0;
          hpb.volumeParam.ioVRefNum = vRefNum;
          hpb.volumeParam.ioNamePtr = (StringPtr)temp;
          if (path == NULL )
          {
            hpb.volumeParam.ioVolIndex = 0;     /* use ioVRefNum only */
          }
          else
          {
            PStrCpy(temp, path);
            hpb.volumeParam.ioNamePtr = temp;
            hpb.volumeParam.ioVolIndex = -1;    /* use ioNamePtr/ioVRefNum combination */
          }
          err = OSErrToLVErr(PBHGetVInfoSync(&hpb));
          if (err)
            DEBUGPRINTF(("PBHGetVInfo: err = %ld", err));
          else
            vRefNum = hpb.volumeParam.ioVRefNum;
        }

        if (vRefNum)
        {
          FSSpec fst;
          uChar *p;
          int32 i = 0;

          /* remove the last elt(s) of the path and see if we have success */
          if (path)
            PStrCpy(temp, path);
          p = (uChar*)&temp[PStrLen(path)];

          do
          {
            for (; p > temp; p--, i++)
            {
              if (*p == ':')
              {
                break;
                p--, i++;
              }
            }
            if (p <= temp)
              return bdNamErr;
            PStrLen(temp) = PStrLen(path) - i;
            err = FSMakeFSSpec(vRefNum, dirID, temp, &fst);
            if (err)
              DEBUGPRINTF(("FSMakeFSSpec: err2 = %ld, path = %p", err, temp));
          } while (bdNamErr == err);

          if (!err || (fnfErr == err))
          {
            CInfoPBRec cpb;

            PStrLen(p) = i;

            memset(&cpb, 0, sizeof(CInfoPBRec));
            cpb.hFileInfo.ioNamePtr = fst.name;
            cpb.hFileInfo.ioVRefNum = fst.vRefNum;
            cpb.hFileInfo.ioDirID = fst.parID;
            cpb.hFileInfo.ioFDirIndex = 0;           /* use ioNamePtr and ioDirID */
            err = OSErrToLVErr(PBGetCatInfoSync(&cpb));
            if (err)
              DEBUGPRINTF(("PBGetCatInfo: err = %ld", err));
            else
            {
              err = MakeFileSpec(fst.vRefNum, cpb.dirInfo.ioDrDirID, p, fss);
              if (err)
                DEBUGPRINTF(("MakeFileSpec: err = %ld, path = %p", err, p));
            }
          }
        }
      }
      else
        return err;
    }

    /* Workaround adapted from Apple Developer Support sample code in
       MoreFiles 1.2.1:FSpCompat.c.
       Fix a bug in Macintosh PC Exchange's MakeFSSpec code where 0 is
       returned in the parID field when making an FSSpec to the volume's root
       directory by passing a full pathname in MakeFSSpec's fileName parameter. */
    if ((err == mgNoErr) && (fss->parID == 0))
    {
      fss->parID = fsRtParID;
    }
    return err;
}

static Boolean HasFMExtension(FSSpec *fss, int32 attr)
{
    HParamBlockRec pb;
    Str255 volName;
    char *p;
    GetVolParmsInfoBuffer vParms;

    strncpy(volName, fss->name, PStrLen(fss->name));
    volName[PStrLen(fss->name)] = 0;
    p = strchr(volName, ':');
    if (!p)
      return false;
    *(++p) = 0;

    pb.volumeParam.ioCompletion = 0L;
    pb.volumeParam.ioNamePtr = volName;
    pb.volumeParam.ioVRefNum = fss->vRefNum;
    pb.ioParam.ioReqCount = sizeof(vParms);
    pb.ioParam.ioBuffer = (Ptr)&vParms;
    if (PBHGetVolParmsSync(&pb))
    {
      return false;
    }
    return ((vParms.vMAttrib & attr) == attr);
}

/*
Calculate difference in seconds between local time zone and daylight
savings time settings and Universal Time Coordinate (also called GMT).
*/
static int32 UTCShift()
{
    MachineLocation loc;
    int32 delta;

    ReadLocation(&loc);
    delta = loc.u.gmtDelta & 0x00ffffff;  /* get sec east of UTC */
    if (delta & 0x800000)
      delta |= 0xff000000;                /* sign extend */
    return -delta;                        /* secs from UTC */
}

static void MacConvertFromLVTime(uInt32 time, uInt32 *sTime)
{
    if (time > 0.0)
    {
      *sTime = time - UTCShift();
    }
    return;
}

static void MacConvertToLVTime(uInt32 sTime, uInt32 *time)
{
    *time = sTime + UTCShift();
    return;
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
static int32 MakePathDSString(Path path, LStrPtr *lstr)
{
    int32 pathLen = -1;

    MgErr err = FPathToText(path, (LStrPtr)&pathLen);
	DoDebugger();
    if (!err)
    {
      *lstr = (LStrPtr)DSNewPClr(sizeof(int32) + pathLen + 1);
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
    return;
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

extern void ZEXPORT DLLVersion(uChar* version)
{
    sprintf((char*)version, "lvzlib date: %s, time: %s",__DATE__,__TIME__);
}

extern MgErr ZEXPORT LVPath_HasResourceFork(Path path, int32 *hasResFork)
{
#if MacOS
    FSSpec theFSSpec;
    MgErr  err = mgNoErr;

    *hasResFork = 0;

    err = MakeMacSpec(path, &theFSSpec);
    if (!err)
    {
      int theRefNum = FSpOpenResFile(&theFSSpec, fsRdPerm);
      if (ResError() == mgNoErr) /* we've got a resource file */ 
      {
        CloseResFile(theRefNum);
        *hasResFork = 1;
      }
    }
    return err;
#else
    *hasResFork = 0;
    return mgNoErr;
#endif
}

extern MgErr ZEXPORT LVPath_EncodeMacbinary(Path srcPath, Path dstPath)
{
#if MacOS
    FSSpec srcFSSpec;
    FSSpec dstFSSpec;
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
    return mgNotSupported;
#endif
}

extern MgErr ZEXPORT LVPath_DecodeMacbinary(Path srcPath, Path dstPath)
{
#if MacOS
    FSSpec srcFSSpec;
    FSSpec dstFSSpec;
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
    return mgNotSupported;
#endif
}

extern MgErr ZEXPORT LVPath_OpenFile(LVRefNum *refnum, Path path, uInt8 rsrc, uInt32 openMode, uInt32 denyMode)
{
    MgErr err;
	int32 type;
    File ioRefNum;
#if MacOS
    FSSpec fss;
    int16 perm;
    OSErr ret;
    HParamBlockRec pb;
    Boolean hasDeny = HasFMExtension(&fss, 1L << bHasOpenDeny);
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
    
    if (err = FGetPathType(path, &type))
      return err;
	
	if ((type != fAbsPath) && (type != fUNCPath)) 
	  return mgArgErr;

#if MacOS
    if (err = MakeMacSpec(path, &fss))
    {
      DEBUGPRINTF(("MakeMacSpec: err = %ld", err));
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

    if (hasDeny)
    {
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
    }

    pb.fileParam.ioCompletion = 0;
    pb.fileParam.ioNamePtr = fss.name;
    pb.fileParam.ioVRefNum = fss.vRefNum;
    pb.fileParam.ioDirID = fss.parID;
    pb.ioParam.ioMisc = NULL;

    if (hasDeny)
    {
      pb.accessParam.ioDenyModes = perm;
      if (rsrc)
        ret = PBHOpenRFDenySync(&pb);
      else
        ret = PBHOpenDenySync(&pb);
    }
    else
    {
      pb.ioParam.ioPermssn = perm;
      if (rsrc)
        ret = PBHOpenRFSync(&pb);
      else
        ret = PBHOpenSync(&pb);
    }

    if (!(err = OSErrToLVErr(ret)))
    {
      ioRefNum = (File)pb.ioParam.ioRefNum;
      if (openMode == openWriteOnlyTruncate)
        err = OSErrToLVErr(SetEOF(ioRefNum, 0L));
    }
#elif Win32
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

    err = MakePathDSString(path, &lstr);
    if (err)
      return err;
    /* Open the specified file. */
    attempts = 3;

    while (attempts)
    {
      ioRefNum = CreateFile((LPCSTR)lstr->str, openAcc, shareAcc, 0, createMode,
                            FILE_ATTRIBUTE_NORMAL, 0);
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
      err = Win32ToLVFileErr();
#elif Unix
    if (rsrc)
      return mgNotSupported;

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

    err = MakePathDSString(path, &lstr);
    if (err)
      return err;

    /* Test for file existence first to avoid creating file with mode "w". */
    if (openMode == openWriteOnlyTruncate && stat(lstr->str, &statbuf))
    {
      DSDisposePtr((UPtr)lstr);
      return fNotFound;
    }

    errno = 0;
    ioRefNum = (File)fopen(lstr->str, (char *)theMode);
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
      if (fcntl(fileno((FILE *)ioRefNum), F_SETLK, (int32) &lockInfo) == -1)
      {
        err = UnixToLVFileErr();
        fclose((FILE *)ioRefNum);
      }
    }
#endif
#else
    err = mgNotSupported;
#endif
    if (!err)
      err = FNewRefNum(path, ioRefNum, refnum);
    else
      DEBUGPRINTF(("OpenFile: err = %ld, rsrc = %d", err, rsrc));

    return err;
}

extern MgErr ZEXPORT LVPath_UtilFileInfo(Path path,
                                         uInt8 write,
                                         uInt8 *isDirectory,
                                         LVFileInfo *fileInfo,
                                         LStrHandle comment)
{
    MgErr err = mgNoErr;
    int32 count = 0;
#if MacOS
    FSSpec fss;
    CInfoPBRec cpb;
#elif Win32
    LStrPtr lstr;
    HANDLE handle = NULL;
    WIN32_FIND_DATAA fi;
#elif Unix
    LStrPtr lstr;
    struct stat statbuf;
    struct utimbuf buf;
#endif

    if (!path || !comment)
      return mgArgErr;

#if MacOS
    if (err = MakeMacSpec(path, &fss))
    {
      DEBUGPRINTF(("MakeMacSpec: err = %ld", err));
      return err;
    }

    memset(&cpb, 0, sizeof(CInfoPBRec));
    cpb.hFileInfo.ioNamePtr = fss.name;
    cpb.hFileInfo.ioVRefNum = fss.vRefNum;
    cpb.hFileInfo.ioDirID = fss.parID;

    err = OSErrToLVErr(PBGetCatInfoSync(&cpb));
    if (err)
      DEBUGPRINTF(("PBGetCatInfo: err = %ld", err));

    if (!err)
    {
 #if !MacOSX
      DTPBRec dtpb;

      memset(&dtpb, 0, sizeof(DTPBRec));
      dtpb.ioVRefNum = fss.vRefNum;
      err = OSErrToLVErr(PBDTGetPath(&dtpb));
      if (err)
	  {
        dtpb.ioNamePtr = nil;
        DEBUGPRINTF(("PBGetPath: err = %ld", err));
        /* Ignore error for getting/setting Desktop comments */
        err = mgNoErr;
	  }
	  else
	  {
        dtpb.ioNamePtr = fss.name;
        dtpb.ioDirID = fss.parID;
      }
 #endif
      if (!err)
      {
        *isDirectory = (MacIsDir(cpb) != 0);

        if (write)
        {
          if (*isDirectory)
          {
            cpb.dirInfo.ioDrUsrWds.frFlags = fileInfo->flags;
            cpb.dirInfo.ioDrUsrWds.frLocation.v = fileInfo->location.v;
            cpb.dirInfo.ioDrUsrWds.frLocation.h = fileInfo->location.h;

            MacConvertFromLVTime(fileInfo->cDate, &cpb.dirInfo.ioDrCrDat);
            MacConvertFromLVTime(fileInfo->mDate, &cpb.dirInfo.ioDrMdDat);
          }
          else
          {
            cpb.hFileInfo.ioFlFndrInfo.fdType = fileInfo->type;
            cpb.hFileInfo.ioFlFndrInfo.fdCreator = fileInfo->creator;
            cpb.hFileInfo.ioFlFndrInfo.fdFlags = fileInfo->flags;
            cpb.hFileInfo.ioFlFndrInfo.fdFldr = fileInfo->fId;
            cpb.hFileInfo.ioFlFndrInfo.fdLocation.v = fileInfo->location.v;
            cpb.hFileInfo.ioFlFndrInfo.fdLocation.h = fileInfo->location.h;
            cpb.hFileInfo.ioFlXFndrInfo.fdScript = fileInfo->sId;
            cpb.hFileInfo.ioFlXFndrInfo.fdXFlags = fileInfo->xFlags;

            MacConvertFromLVTime(fileInfo->cDate, &cpb.hFileInfo.ioFlCrDat);
            MacConvertFromLVTime(fileInfo->mDate, &cpb.hFileInfo.ioFlMdDat);
          }

          err = OSErrToLVErr(PBSetCatInfoSync(&cpb));
          if (err)
            DEBUGPRINTF(("PBSetCatInfo: err = %ld", err));
 #if !MacOSX
          if (!err && comment && dtpb.ioNamePtr)
          {
            dtpb.ioDTBuffer = LStrBuf(*comment);
            dtpb.ioDTReqCount = LStrLen(*comment);
            /* Ignore error for setting Desktop comments */
            PBDTSetCommentSync(&dtpb);
          }
 #endif
        }
        else
        {
          if (*isDirectory)
          {
            fileInfo->type = kUnknownType;
            fileInfo->creator = kUnknownType;
            fileInfo->flags = cpb.dirInfo.ioDrUsrWds.frFlags;
            fileInfo->fId = 0;
            fileInfo->location.v = cpb.dirInfo.ioDrUsrWds.frLocation.v;
            fileInfo->location.h = cpb.dirInfo.ioDrUsrWds.frLocation.h;
            fileInfo->size = cpb.dirInfo.ioDrNmFls;
            fileInfo->rfSize = 0;

            MacConvertToLVTime(cpb.dirInfo.ioDrCrDat, &fileInfo->cDate);
            MacConvertToLVTime(cpb.dirInfo.ioDrMdDat, &fileInfo->mDate);
          }
          else
          {
            fileInfo->type = cpb.hFileInfo.ioFlFndrInfo.fdType;
            fileInfo->creator = cpb.hFileInfo.ioFlFndrInfo.fdCreator;
            fileInfo->flags = cpb.hFileInfo.ioFlFndrInfo.fdFlags;
            fileInfo->fId = cpb.hFileInfo.ioFlFndrInfo.fdFldr;
            fileInfo->location.v = cpb.hFileInfo.ioFlFndrInfo.fdLocation.v;
            fileInfo->location.h = cpb.hFileInfo.ioFlFndrInfo.fdLocation.h;
            fileInfo->size = cpb.hFileInfo.ioFlLgLen;
            fileInfo->rfSize = cpb.hFileInfo.ioFlRLgLen;
            fileInfo->sId = cpb.hFileInfo.ioFlXFndrInfo.fdScript;
            fileInfo->xFlags = cpb.hFileInfo.ioFlXFndrInfo.fdXFlags;

            MacConvertToLVTime(cpb.hFileInfo.ioFlCrDat, &fileInfo->cDate);
            MacConvertToLVTime(cpb.hFileInfo.ioFlMdDat, &fileInfo->mDate);
          }

 #if !MacOSX
          if (comment && dtpb.ioNamePtr)
          {
            err = DSSetHandleSize((UHandle)comment, 255);
            if (!err)
            {
              dtpb.ioDTBuffer = LStrBuf(*comment);
              dtpb.ioDTReqCount = LStrLen(*comment) = 255;
              dtpb.ioDTActCount = 0;
              err = OSErrToLVErr(PBDTGetCommentSync(&dtpb));
              LStrLen(*comment) = err ? 0 : dtpb.ioDTActCount;
              /* Ignore error for getting Desktop comments */
              err = mgNoErr;
            }
          }
 #endif
        }
      }
    }
#elif Win32
    err = MakePathDSString(path, &lstr);
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

          DSDisposePtr((UPtr)lstr);
          err = FPathToPath(&temp);
          if (!err)
            err = FAppendName(temp, (PStr)"\003*.*");

          err = MakePathDSString(temp, &lstr);
          FDisposePath(temp);

          if (!err)
          {
            handle = FindFirstFileA((LPCSTR)lstr->str, &fi);
            if (handle == INVALID_HANDLE_VALUE)
              count = 0;
            else
              for (;FindNextFile(handle, &fi);)
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
          fileInfo->size = fi.nFileSizeLow;
        }
      }
    }
    DSDisposePtr((UPtr)lstr);
#elif Unix
    err = MakePathDSString(path, &lstr);
    if (err)
      return err;

    if (stat(lstr->str, &statbuf))
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
        if (utime(lstr->str, &buf))
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

          if (!(dirp = opendir(lstr->str)))
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

extern long ZEXPORT InitializeFileFuncs (zlib_filefunc64_def* pzlib_filefunc_def, LStrHandle *memory)
{
    DoDebugger();

    if (pzlib_filefunc_def)
    {
      if (memory)
      {
        fill_mem_filefunc(pzlib_filefunc_def, memory);
      }
      else
      {
#if Win32
        fill_win32_filefunc64A(pzlib_filefunc_def);
#else
        fill_fopen64_filefunc(pzlib_filefunc_def);
#endif
      }
      return mgNoErr;
    }
    else
      return sizeof(zlib_filefunc64_def);
}

#if MacOSX
static MgErr ConvertToPosixPath(const CStr hfsPath, CStr posixPath, int32 *len)
{
    MgErr err = mFullErr;
    CFStringRef fileRef, posixRef = NULL;
    CFURLRef urlRef = NULL;
    Boolean isDir = FALSE;
    uInt32 enc = CFStringGetSystemEncoding();

	#error "Remove me"
	
    if (!len)
      return mgArgErr;

    fileRef = CFStringCreateWithCString(NULL, hfsPath, enc);
    if (fileRef)
    {
      urlRef = CFURLCreateWithFileSystemPath(NULL, fileRef, kCFURLHFSPathStyle, isDir);
      CFRelease(fileRef);
    }

    if (urlRef)
    {
      posixRef = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);
      CFRelease(urlRef);
    }

    if (posixRef)
    {
      err = mgNoErr;
      if (posixPath && (*len > CFStringGetLength(posixRef)))
      {
        if (!CFStringGetCString(posixRef, posixPath, *len, enc))
          err = mgArgErr;
      }
      *len = CFStringGetLength(posixRef);
      CFRelease(posixRef);
    }
    return err;
}
#endif

extern MgErr ZEXPORT LVPath_ToText(Path path, CStr str, int32 *len)
{
    MgErr err;
    LStrPtr lstr;
    int32 pathLen = -1;

    if (!len)
      return mgArgErr;
    err = FPathToText(path, (LStrPtr)&pathLen);
    if (!err)
    {
      lstr = (LStrPtr)DSNewPClr(sizeof(int32) + pathLen + 1);
      if (!lstr)
        return mFullErr;
      lstr->cnt = pathLen;
      err = FPathToText(path, lstr);
#if MacOSX
      if (!err)
        err = ConvertToPosixPath(lstr->str, NULL, &pathLen);
#endif
      if (!err)
      {
        if (str && (*len > pathLen))
        {
#if MacOSX
          err = ConvertToPosixPath(lstr->str, str, len);
#else
          StrNCpy(str, lstr->str, *len);
#endif
        }
      }
      DSDisposePtr((UPtr)lstr);
      *len = pathLen + 1;
    }
    return err;
}
