/* lvutil.h -- support functions for macbinary

   Version 1.9, Sept 17th, 2007

   Copyright (C) 2002-2007 Rolf Kalbermatter
*/
#define test
#ifndef _lvUtil_H
#define _lvUtil_H

//#include "zlib.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* Possible values for ProcessorType */
#define kM68000		1		/* OBSOLETE */
#define kX86		2
#define kSparc		3
#define kPPC		4
#define kPARISC		5
#define kMIPS		6
#define kDECAlpha	7		/* OBSOLETE */
#define kPIC		8
#define kARM		9
#define kX64		10

#if defined(macintosh) || defined(__PPCC__) || defined(THINK_C) || defined(__SC__) || defined(__MWERKS__) || defined(__APPLE_CC__)
 #ifdef __APPLE_CC__
  #define MacOSX 1
 #endif
 #if defined(__powerc) || defined(__ppc__)
  #define ProcessorType	kPPC
  #define BigEndian 1
 #elif defined(__i386__)
  #define ProcessorType	kX86
  #define BigEndian 0
 #elif defined(__x86_64)
  #define ProcessorType	kX64
  #define BigEndian 0
 #else
  #define ProcessorType	kM68000
  #define BigEndian 1
 #endif
 #define MacOS 1
 #ifdef __APPLE_CC__
  #define MacOSX 1
 #endif
#elif defined(WIN32) || defined(_WIN32)  || defined(__WIN32__) || defined(_WIN32_WCE)
 #ifdef _M_PPC
  #define ProcessorType	kPPC
 #elif defined(_M_IX86)
  #define ProcessorType	kX86
 #elif defined(_M_X64)
  #define ProcessorType	kX64
 #elif defined(_M_ALPHA)
  #define ProcessorType	kDECAlpha
 #elif Compiler == kBorlandC
  #define ProcessorType	kX86
 #elif defined(_ARM_)
  #define ProcessorType 	kARM
 #else
  #error "We don't know the ProcessorType architecture"
 #endif
 #define Win32 1
 #if defined(_DEBUG) || defined(_CVI_DEBUG_)
  #define DEBUG 1
 #endif
 #define BigEndian 0
#elif defined(linux) || defined(__linux) || defined(__linux__)
 #if defined(i386)
  #define ProcessorType	kX86
  #define BigEndian		0
 #elif defined(__alpha)
  #define ProcessorType	kDECAlpha
  #define BigEndian		0
 #elif defined(powerpc)
  #define ProcessorType	kPPC
  #define BigEndian		1
 #elif defined(sparc)
  #define ProcessorType	kSparc
  #define BigEndian		1
 #elif defined(mips)
  #define ProcessorType	kMIPS
  #define BigEndian		1
 #elif defined(arm) || defined(__arm__)
  #define ProcessorType	kARM
  #define BigEndian		0
 #elif defined(__x86_64__)
  #define ProcessorType	kX64
  #define BigEndian		0
 #else
  #error "Unknown Linux platform"
 #endif
 #define Unix 1
 #define HAVE_FCNTL
 #define HAVE_ICONV
 #define HAVE_WCRTOMB
#elif defined(__VXWORKS__)
 #define Unix 1
 #define VxWorks 1
 #if defined (__ppc)
  #define ProcessorType	kPPC
  #define BigEndian 1
 #else
  #define ProcessorType	kX86
  #define BigEndian 0
 #endif
#else
 #error No target defined
#endif

#if Win32
 #define LibAPI(retval) extern __declspec(dllexport) retval
#else
 #define LibAPI(retval)	__attribute__((visibility ("default"))) extern retval
#endif

#if defined(DEBUG)
 #if Win32
  #if defined(_CVI_DEBUG_)
   #define DoDebugger()
  #elif _MSC_VER >= 1400
	 #define DoDebugger()   __debugbreak()
  #else
   #define DoDebugger()    {__asm int 3}
  #endif
 #elif MacOS
  #define DoDebugger()    Debugger()
 #else
  #define DoDebugger()
 #endif
 #define DEBUGPRINTF(args)      DbgPrintf args
#else
 #define DoDebugger()
 #define DEBUGPRINTF(args)
/* long DebugPrintf(char* fmt, ...);
 long DebugPrintf(char* fmt, ...)
 {
   return 0;
 }
*/
#endif

#ifndef Unused
/* The macro Unused can be used to avoid compiler warnings for
unused parameters or locals. */
#   ifdef __cplusplus
/* This implementation of Unused is safe for const parameters. */
#       define Unused(var_or_param)    _Unused((const void *)&var_or_param)
        inline void _Unused(const void *) {}
#   elif MSWin
/* This implementation of Unused is not safe for const parameters. */
#       define Unused(var_or_param)    var_or_param=var_or_param
#   else
#       define Unused(var_or_param)    var_or_param=var_or_param
#   endif
#endif /* Unused */

typedef signed char			int8;
typedef unsigned char		uInt8;
typedef uInt8				uChar;
typedef signed short		int16;
typedef unsigned short		uInt16;
typedef signed int			int32;
typedef unsigned int		uInt32;
#if Win32
typedef signed __int64		int64;
typedef unsigned __int64	uInt64;
#else
typedef signed long long 	int64;
typedef unsigned long long 	uInt64;
#endif
typedef float				float32;
typedef double				float64;

typedef uInt8				LVBoolean;

#define LV_FALSE	0
#define LV_TRUE		1

#define Private(T)  typedef struct T##_t { void *p; } *T
#define PrivateH(T)  struct T##_t; typedef struct T##_t **T

typedef enum {  iB=1, iW, iL, iQ, uB, uW, uL, uQ, fS, fD, fX, cS, cD, cX } NumType;

typedef int32           MgErr;

typedef int32           Bool32;

/*** The Support Manager ***/

#define HiNibble(x)		(uInt8)(((x)>>4) & 0x0F)
#define LoNibble(x)		(uInt8)((x) & 0x0F)
#define HiByte(x)		((uInt8)((uInt16)(x)>>8))
#define LoByte(x)		((uInt8)(x))
#define Word(hi,lo)		(((uInt16)(hi)<<8) | ((uInt16)(uInt8)(lo)))
#define Hi16(x)			((uInt16)((uInt32)(x)>>16))
#define Lo16(x)			((uInt16)(x))
#define Long(hi,lo)		(((uInt32)(hi)<<16) | ((uInt32)(uInt16)(lo)))
#define Hi32(x)			((uInt32)((uInt64)(x)>>32))
#define Lo32(x)			((uInt32)(x))
#define Quad(hi,lo)		(((uInt64)(hi)<<32) | ((uInt64)(uInt32)(lo)))

#define Cat4Chrs(c1,c2,c3,c4)   (((int32)(uInt8)(c1)<<24)|((int32)(uInt8)(c2)<<16)|((int32)(uInt8)(c3)<<8)|((int32)(uInt8)(c4)))
#define Cat2Chrs(c1,c2)         (((int16)(uInt8)(c1)<<8)|((int16)(uInt8)(c2)))

#if BigEndian
#define RTToL(c1,c2,c3,c4)  Cat4Chrs(c1,c2,c3,c4)
#define RTToW(c1,c2)        Cat2Chrs(c1,c2)
#else
#define RTToL(c1,c2,c3,c4)  Cat4Chrs(c4,c3,c2,c1)
#define RTToW(c1,c2)        Cat2Chrs(c2,c1)
#endif

enum {                  /* Manager Error Codes */
    mgNoErr=0,
#if !MacOS && !QTMLIncl
    noErr = mgNoErr,
#endif
    mgArgErr=1,
    mFullErr,           /* Memory Mgr   2-3     */
    mZoneErr,

    fEOF,               /* File Mgr     4-12    */
    fIsOpen,
    fIOErr,
    fNotFound,
    fNoPerm,
    fDiskFull,
    fDupPath,
    fTMFOpen,
    fNotEnabled,

    rFNotFound,         /* Resource Mgr 13-15   */
    rAddFailed,
    rNotFound,

    iNotFound,          /* Image Mgr    16-17   */
    iMemoryErr,

    dPenNotExist,       /* Draw Mgr     18      */

    cfgBadType,         /* Config Mgr   19-22   */
    cfgTokenNotFound,
    cfgParseError,
    cfgAllocError,

    ecLVSBFormatError,  /* extCode Mgr  23      */
    ecLVSBOffsetError,  /* extCode Mgr  24      */
    ecLVSBNoCodeError,  /* extCode Mgr  25      */

    wNullWindow,        /* Window Mgr   26-27   */
    wDestroyMixup,

    menuNullMenu,       /* Menu Mgr     28      */

    pAbortJob,          /* Print Mgr    29-35   */
    pBadPrintRecord,
    pDriverError,
    pWindowsError,
    pMemoryError,
    pDialogError,
    pMiscError,

    dvInvalidRefnum,    /* Device Mgr   36-41   */
    dvDeviceNotFound,
    dvParamErr,
    dvUnitErr,
    dvOpenErr,
    dvAbortErr,

    bogusError,         /* generic error 42 */
    cancelError,        /* cancelled by user 43 */

    OMObjLowErr,        /* object message dispatcher errors 44-52 */
    OMObjHiErr,
    OMObjNotInHeapErr,
    OMOHeapNotKnownErr,
    OMBadDPIdErr,
    OMNoDPinTabErr,
    OMMsgOutOfRangeErr,
    OMMethodNullErr,
    OMUnknownMsgErr,

    mgNotSupported,

    ncBadAddressErr,        /* Net Connection errors 54-66 */
    ncInProgress,
    ncTimeOutErr,
    ncBusyErr,
    ncNotSupportedErr,
    ncNetErr,
    ncAddrInUseErr,
    ncSysOutOfMem,
    ncSysConnAbortedErr,    /* 62 */
    ncConnRefusedErr,
    ncNotConnectedErr,
    ncAlreadyConnectedErr,
    ncConnClosedErr,        /* 66 */
};

typedef struct
{
    int16 v;
    int16 h;
} LVPoint;

typedef uChar        Str255[256], *PStr, *CStr, *UPtr, **UHandle;
typedef const uChar  *ConstCStr, *ConstPStr, *ConstUPtr, ConstStr255[256];

#define PStrBuf(b)  (&((PStr)(b))[1])
#define PStrLen(b)  (((PStr)(b))[0])    /* # of chars in string */
#define PStrSize(b) (PStrLen(b)+1)      /* # of bytes including length */

int32 CToPStr(ConstCStr src, PStr dest);
int32 PStrCpy(PStr d, ConstPStr s);
int32 StrCpy(CStr t, const CStr s);
int32 StrNCpy(CStr t, const CStr s, int32 l);
int32 StrLen(ConstCStr str);

typedef struct {
    int32   cnt; 
    uChar   str[1];
} LStr, *LStrPtr, **LStrHandle;

#define LStrLen(p)  ((p)->cnt)
#define LStrBuf(p)  ((p)->str)

#define LStrLenH(h) ((h) ? (*(h))->cnt : 0)
#define LStrBufH(h) ((h) ? (*(h))->str : NULL)

typedef struct {
    int32   cnt;
    uChar   str[256];
} LStr256;

/* Magic Cookies */
typedef uInt32 MagicCookie;

struct MagicCookieJarStruct;
typedef struct MagicCookieJarStruct MCJar, **MagicCookieJar;
typedef UPtr MagicCookieInfo;

#define kNotAMagicCookieJar ((MagicCookieJar) 0)

typedef struct {
	int32		count;		/* number of cookies that follow */
	MagicCookie cookie[1];	/* cookies */
} MagicCookieList, *MagicCookieListPtr, **MagicCookieListHandle;

MagicCookieJar MCNewBigJar(uInt32 size);
MgErr MCDisposeJar(MagicCookieJar);
MagicCookie MCNewCookie(MagicCookieJar, MagicCookieInfo);
MgErr MCDisposeCookie(MagicCookieJar, MagicCookie, MagicCookieInfo);
MgErr MCGetCookieInfo(MagicCookieJar, MagicCookie, MagicCookieInfo);
MgErr MCGetCookieInfoPtr(MagicCookieJar, MagicCookie, MagicCookieInfo*);
MgErr MCGetCookieList(MagicCookieJar, MagicCookieListHandle*);
Bool32 MCIsACookie(MagicCookieJar, MagicCookie);

/* Runtime Cleanup support */
enum {		/* cleanup modes (when to call cleanup proc) */
	kCleanRemove,
	kCleanExit,				/* only when LabVIEW exits */
	kCleanOnIdle,			/* whenever active vi goes idle */
	kCleanAfterReset,		/* whenever active vi goes idle after a reset */
	kCleanOnIdleIfNotTop,	/* whenever active vi goes idle if active vi is not current vi */
	kCleanAfterResetIfNotTop/* whenever active vi goes idle after a reset if active vi is not current vi */
	};
typedef int32	(*CleanupProcPtr)(UPtr);
int32 RTSetCleanupProc(CleanupProcPtr, UPtr, int32);

/* File Manager */
typedef struct {
	uInt32 type;    /* handled by LabVIEW Type & Creator */
	uInt32 creator; /* handled by LabVIEW Type & Creator */
	uInt64 size;    /* not modified, use EOF */
	uInt64 rfSize;  /* Mac only, not modified, use EOF */
	uInt32 cDate;
	uInt32 mDate;
	uInt16 flags;   
	LVPoint location;  /* Mac only */
	uInt16 finderId;   /* Mac only */
	uInt16 xFlags;     /* Mac only */
	int32 putAwayId;   /* Mac only */
} LVFileInfo;

typedef enum _FMFileType {
	kInvalidType	=0,
	kUnknownFileType=RTToL('?','?','?','?'),
	kTextFileType	=RTToL('T','E','X','T'),
	/** Typical directory types */
	kHardDiskDirType=RTToL('h','d','s','k'),
	kFloppyDirType	=RTToL('f','l','p','y'),
	kNetDriveDirType=RTToL('s','r','v','r')
}FMFileType;

typedef enum  {
	kInvalidCreator	=0,
	kUnknownCreator =RTToL('?','?','?','?'),
	/** LabVIEW creator type */
	kLVCreatorType	=RTToL('L','B','V','W')
}FMFileCreator;

/** Used for FGetInfo */
typedef struct {			/**< file/directory information record */
	FMFileType type;		/**< system specific file type-- 0 for directories */
	FMFileCreator creator;	/**< system specific file creator-- 0 for directories */
	int32	permissions;	/**< system specific file access rights */
	int32	size;			/**< file size in bytes (data fork on Mac) or entries in folder */
	int32	rfSize;			/**< resource fork size (on Mac only) */
	uInt32	cdate;			/**< creation date */
	uInt32	mdate;			/**< last modification date */
	Bool32	folder;			/**< indicates whether path refers to a folder */
	Bool32	isInvisible; /**< indicates whether the file is visible in File Dialog */
	struct {
		int16 v, h;
	} location;			/**< system specific geographical location */
	Str255	owner;			/**< owner (in pascal string form) of file or folder */
	Str255	group;			/**< group (in pascal string form) of file or folder */
} FInfoRec, *FInfoPtr;

/** Used for FGetInfo, 64-bit version */
typedef uInt32	FGetInfoWhich;
enum {
	kFGetInfoType			= 1L << 0,
	kFGetInfoCreator		= 1L << 1,
	kFGetInfoPermissions	= 1L << 2,
	kFGetInfoSize			= 1L << 3,
	kFGetInfoRFSize			= 1L << 4,
	kFGetInfoCDate			= 1L << 5,
	kFGetInfoMDate			= 1L << 6,
	kFGetInfoFolder			= 1L << 7,
	kFGetInfoIsInvisible	= 1L << 8,
	kFGetInfoLocation		= 1L << 9,
	kFGetInfoOwner			= 1L << 10,
	kFGetInfoGroup			= 1L << 11,
	kFGetInfoAll			= 0xEFFFFFFFL
};
typedef struct {			/**< file/directory information record */
	FMFileType type;		/**< system specific file type-- 0 for directories */
	FMFileCreator creator;	/**< system specific file creator-- 0 for directories */
	int32	permissions;	/**< system specific file access rights */
	int64	size;			/**< file size in bytes (data fork on Mac) or entries in folder */
	int64	rfSize;			/**< resource fork size (on Mac only) */
	uInt32	cdate;			/**< creation date */
	uInt32	mdate;			/**< last modification date */
	Bool32	folder;			/**< indicates whether path refers to a folder */
	Bool32	isInvisible; /**< indicates whether the file is visible in File Dialog */
	struct {
		int16 v, h;
	} location;			/**< system specific geographical location */
	Str255	owner;			/**< owner (in pascal string form) of file or folder */
	Str255	group;			/**< group (in pascal string form) of file or folder */
} FInfoRec64, *FInfo64Ptr;

/** Used for FGetVolInfo */
typedef struct {
	uInt32	size;			/**< size in bytes of a volume */
	uInt32	used;			/**< number of bytes used on volume */
	uInt32	free;			/**< number of bytes available for use on volume */
} VInfoRec;

/** Used with FListDir2 */
typedef struct {
	int32 flags;
	FMFileType type;
} FMListDetails;

/** Type Flags used with FMListDetails */
#define kIsFile				0x01
#define kRecognizedType		0x02
#define kIsLink				0x04
#define kFIsInvisible		0x08
#define kIsTopLevelVI		0x10	/**< Used only for VIs in archives */
#define kErrGettingType		0x20	/**< error occurred getting type info */
#if Mac
#define kFIsStationery		0x40
#endif
    
enum { openReadWrite, openReadOnly, openWriteOnly, openWriteOnlyTruncate }; /* open modes */
enum { denyReadWrite, denyWriteOnly, denyNeither}; /* deny modes */

enum { fStart = 1, fEnd, fCurrent };	/* seek modes */
/*
Win32:
#define FILE_BEGIN           0
#define FILE_CURRENT         1
#define FILE_END             2

MacOSX:
enum {
	fsAtMark = 0,
	fsFromStart = 1,
	fsFromLEOF = 2,
	fsFromMark = 3
}

Unix:
SEEK_SET
SEEK_END
SEEK_START
*/
enum { fAbsPath, fRelPath, fNotAPath, fUNCPath, nPathTypes};                            /* path type codes */

Private(File);
typedef struct PATHREF PathRef;
typedef PathRef* Path;
typedef const PathRef* ConstPath;

typedef MagicCookie LVRefNum;
#define kNotARefNum ((LVRefNum)0L)	/* canonical invalid magic cookie */

/* LabVIEW exported functions */
MgErr FPathToText(Path path, LStrPtr lstr);
MgErr FPathToPath(Path *p);
MgErr FAppendName(Path path, PStr name);
Bool32 FIsAPathOfType(Path path, int32 ofType);
MgErr FGetPathType(Path, int32*);
int32 FDepth(Path path);
MgErr FDisposePath(Path p);
MgErr FNewRefNum(Path path, File fd, LVRefNum* refnum);
Bool32 FIsARefNum(LVRefNum);
MgErr FDisposeRefNum(LVRefNum);
MgErr FRefNumToFD(LVRefNum, File*);
MgErr FGetInfo(ConstPath path, FInfoPtr infop);
MgErr FGetInfo64(ConstPath path, FInfo64Ptr infop, FGetInfoWhich which);
MgErr FSetInfo(ConstPath path, FInfoPtr infop);
MgErr FSetInfo64(ConstPath path, FInfo64Ptr infop);
MgErr FMRead(File fd, int32 inCount, int32* outCount, UPtr buffer);
MgErr FMWrite(File fd, int32 inCount, int32* outCount, UPtr buffer);

int32 DbgPrintf(CStr fmt, ...);

UPtr DSNewPClr(size_t size);
MgErr DSDisposePtr(UPtr);
UHandle DSNewHClr(size_t size);
MgErr DSSetHandleSize(UHandle, size_t);
int32 DSGetHandleSize(UHandle);
MgErr DSDisposeHandle(UHandle);
MgErr DSCopyHandle(void *ph, const void *hsrc);

void MoveBlock(ConstUPtr ps, UPtr pd, size_t size);

MgErr NumericArrayResize(int32, int32, UHandle*, size_t);

#define Min(a, b)      ((a) < (b)) ? (a) : (b) 
#define Max(a, b)      ((a) > (b)) ? (a) : (b) 

/* Our exported functions */
LibAPI(void) DLLVersion OF((uChar*  Version));

LibAPI(MgErr) LVPath_ToText(Path path, LStrHandle *str);
LibAPI(MgErr) LVPath_HasResourceFork(Path path, LVBoolean *hasResFork, uInt32 *sizeLow, uInt32 *sizeHigh);
LibAPI(MgErr) LVPath_EncodeMacbinary(Path srcFileName, Path dstFileName);
LibAPI(MgErr) LVPath_DecodeMacbinary(Path srcFileName, Path dstFileName);

LibAPI(MgErr) LVPath_UtilFileInfo(Path path,
                   uInt8 write,
                   uInt8 *isDirectory,
                   LVFileInfo *finderInfo,
                   LStrHandle comment);

typedef union
{
	int64 q;
#if BigEndian
	struct
	{
		int32 hi;
		uInt32 lo;
	} l;
#else
	struct
	{
		uInt32 lo;
		int32 hi;
	} l;
#endif
} FileOffset;

LibAPI(MgErr) LVFile_OpenFile(LVRefNum *refnum, Path path, uInt8 rsrc, uInt32 openMode, uInt32 denyMode);
LibAPI(MgErr) LVFile_CloseFile(LVRefNum *refnum);
LibAPI(MgErr) LVFile_GetSize(LVRefNum *refnum, FileOffset *size);
LibAPI(MgErr) LVFile_SetSize(LVRefNum *refnum, FileOffset *size);
LibAPI(MgErr) LVFile_GetFilePos(LVRefNum *refnum, FileOffset *offs);
LibAPI(MgErr) LVFile_SetFilePos(LVRefNum *refnum, FileOffset *offs, uInt16 mode);
LibAPI(MgErr) LVFile_Read(LVRefNum *refnum, uInt32 inCount, uInt32 *outCount, UPtr buffer);
LibAPI(MgErr) LVFile_Write(LVRefNum *refnum, uInt32 inCount, uInt32 *outCount, UPtr buffer);

LibAPI(MgErr) InitializeFileFuncs(LStrHandle filefunc_def);
LibAPI(MgErr) InitializeStreamFuncs(LStrHandle  filefunc_def, LStrHandle *memory);

#define CP_ACP                    0           // default to ANSI code page
#define CP_OEMCP                  1           // default to OEM  code page
#define CP_UTF8                   65001       // UTF-8 translation

typedef struct
{
	int32 cnt;
	uInt16 str[1];
} UString, **UStrHandle;

LibAPI(MgErr) ZeroTerminateLString(LStrHandle *dest);

LibAPI(uInt32) GetCurrentCodePage(LVBoolean acp);
LibAPI(MgErr) MultiByteCStrToWideString(ConstCStr src, int32 srclen, UStrHandle *dest, uInt32 codePage);
LibAPI(MgErr) MultiByteToWideString(const LStrHandle src, UStrHandle *dest, uInt32 codePage);
LibAPI(MgErr) WideStringToMultiByte(const UStrHandle src, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed);

LibAPI(MgErr) ConvertCString(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed);
LibAPI(MgErr) ConvertCPath(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir);
LibAPI(MgErr) ConvertLString(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed);
LibAPI(MgErr) ConvertLPath(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir);

#ifdef __cplusplus
}
#endif

#endif /* _lvUtil_H */
