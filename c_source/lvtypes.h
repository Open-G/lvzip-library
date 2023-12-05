/*
   lvtypes.h -- LabVIEW type definition

   Copyright (C) 2002-2023 Rolf Kalbermatter

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
#ifndef _lvTypes_H
#define _lvTypes_H

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
#define kARM64		11

#if defined(macintosh) || defined(__PPCC__) || defined(THINK_C) || defined(__SC__) || defined(__MWERKS__) || defined(__APPLE_CC__)
 #if defined(__powerc) || defined(__ppc__)
  #define ProcessorType	kPPC
  #define BigEndian 1
 #elif defined(__i386__)
  #define ProcessorType	kX86
  #define BigEndian 0
 #elif defined(__x86_64)
  #define ProcessorType	kX64
  #define BigEndian 0
 #elif defined(__arm)
  #define ProcessorType	kARM64
  #define BigEndian 0
 #else
  #define ProcessorType	kM68000
  #define BigEndian 1
 #endif
 #define MacOS 1
 #define Win32 0
 #define Unix 0
 #define Pharlap 0
 #ifdef __APPLE_CC__
  #define MacOSX 1
 #endif
#elif defined(WIN32) || defined(_WIN32)  || defined(__WIN32__) || defined(_WIN32_WCE)
 #ifdef _M_PPC
  #define ProcessorType	kPPC
 #elif defined(_M_IX86)
  #define ProcessorType	kX86
  #define PackedStruct  1
 #elif defined(_M_X64)
  #define ProcessorType	kX64
 #elif defined(_M_ALPHA)
  #define ProcessorType	kDECAlpha
 #elif defined(_ARM_)
  #define ProcessorType kARM
 #else
  #error "We don't know the ProcessorType architecture"
 #endif
 #define Win32 1
 #define MacOS 0
 #define MacOSX 0
 #define Unix 0
 #if defined(_DEBUG) || defined(_CVI_DEBUG_)
  #define DEBUG 1
 #endif
 #define BigEndian 0
 #if EMBEDDED
  #define Pharlap 1
 #else
  #define Pharlap 0
 #endif
// #define _WIN32_WINNT 0x0501
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
 #define Win32 0
 #define MacOS 0
 #define MacOSX 0
 #define Pharlap 0
 #define HAVE_FCNTL
 #define HAVE_ICONV
// #define HAVE_WCRTOMB
// #define HAVE_MBRTOWC
#elif defined(__VXWORKS__)
 #define Unix 1
 #define VxWorks 1
 #define Win32 0
 #define MacOS 0
 #define MacOSX 0
 #define Pharlap 0
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

#define Is64Bit			 ProcessorType == kX64 || ProcessorType == kARM64
#define usesHFSPath      MacOS && ProcessorType != kX64
#define usesPosixPath    Unix || (MacOSX && ProcessorType == kX64)
#define usesWinPath      Win32

#if Win32
 #define COBJMACROS
 #include <windows.h>
#elif MacOSX
#elif Unix
 #include <stdio.h>
#endif

#if Win32
 #define LibAPI(retval) extern __declspec(dllexport) retval
#else
 #define LibAPI(retval)	__attribute__((visibility ("default"))) extern retval
#endif

#if defined(DEBUG)
 #define DebugAPI(retval) LibAPI(retval)
 #if Win32
  #if defined(_CVI_DEBUG_)
   #define DoDebugger()
  #elif _MSC_VER >= 1400
	 #define DoDebugger()   __debugbreak()
  #else
   #define DoDebugger(		{__asm int 3}
  #endif
 #elif MacOS
  #define DoDebugger()		Debugger()
 #else
  #define DoDebugger()		__builtin_trap
 #endif
 #define DEBUGPRINTF(args)      DbgPrintf args
#else
 #define DebugAPI(retval) retval
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

#include <wchar.h>

#define LV_FALSE	0
#define LV_TRUE		1

#define Private(T)  typedef struct T##_t { void *p; } *T
#define PrivateH(T)  struct T##_t; typedef struct T##_t **T

typedef enum {  iB=1, iW, iL, iQ, uB, uW, uL, uQ, fS, fD, fX, cS, cD, cX } NumType;

#define stringCode		0x30
#define clustCode		0x50

typedef int32           MgErr;

typedef int32           Bool32;

#if Is64Bit
#define uPtr uQ
#else
#define uPtr uL
#endif

typedef uChar        Str255[256], *PStr, *CStr, *UPtr, **UHandle, **PStrHandle;
typedef const uChar  *ConstCStr, *ConstPStr, *ConstUPtr, ConstStr255[256];

/*** The Support Manager ***/

#define HiNibble(x)		(uInt8)(((x) >> 4) & 0x0F)
#define LoNibble(x)		(uInt8)((x) & 0x0F)
#define HiByte(x)		((uInt8)((uInt16)(x) >> 8))
#define LoByte(x)		((uInt8)(x))
#define Word(hi,lo)		(((uInt16)(hi) << 8) | ((uInt16)(lo & 0xFF)))
#define Hi16(x)			((uInt16)((uInt32)(x) >> 16))
#define Lo16(x)			((uInt16)(x))
#define Long(hi,lo)		(((uInt32)(hi) << 16) | ((uInt32)(lo & 0xFFFF)))
#define Hi32(x)			((uInt32)((uInt64)(x) >> 32))
#define Lo32(x)			((uInt32)(x))
#define Quad(hi,lo)		(((uInt64)(hi) << 32) | ((uInt64)(lo & 0xFFFFFFFF)))

#define Cat4Chrs(c1,c2,c3,c4)   (((int32)(uInt8)(c1)<<24)|((int32)(uInt8)(c2)<<16)|((int32)(uInt8)(c3)<<8)|((int32)(uInt8)(c4)))
#define Cat2Chrs(c1,c2)         (((int16)(uInt8)(c1)<<8)|((int16)(uInt8)(c2)))

#define Swap16(x)       (uInt16)(((x & 0xff) >> 8) | (x << 8))
#define Swap32(x)       (uInt32)(((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >> 8) | ((x & 0x0000ff00) << 8) | (x << 24))

void		SwapWL(UPtr p);
void		SwapBW(UPtr p);
void		RevBL(UPtr p);
void		RevBQ(UPtr p);

#if BigEndian
#define RTToL(c1,c2,c3,c4)  Cat4Chrs(c1,c2,c3,c4)
#define RTToW(c1,c2)        Cat2Chrs(c1,c2)
#define StdToW(x)			
#define StdToL(x			
#define StdToQ(x)
#define StdToR(x)
#define StdToR32(x)
#define StdToP(x)
#define StdToPtr(x)
#define ConvertBE16(x)		x
#define ConvertBE32(x)		x
#define ConvertLE16(x)		Swap16(x)
#define ConvertLE32(x)		Swap32(x)
#else
#define RTToL(c1,c2,c3,c4)  Cat4Chrs(c4,c3,c2,c1)
#define RTToW(c1,c2)        Cat2Chrs(c2,c1)
#define StdToW(x)			SwapBW((UPtr)(x))
#define StdToL(x)			RevBL((UPtr)(x))
#define StdToQ(x)			RevBQ((UPtr)(x))
#define StdToR(x)			(SwapBW((int8*)(x)), SwapBW(((int8*)(x))+2), SwapBW(((int8*)(x))+4), SwapBW(((int8*)(x))+6))
#define StdToR32(x)			(RevBL((int8*)(x)), RevBL(((int8*)(x))+4), RevBL(((int8*)(x))+8), RevBL(((int8*)(x))+12))
#define StdToP(x)			(SwapBW((int8*)(x)), SwapBW(((int8*)(x))+2))
#if Is64Bit
#define StdToPtr(x)			StdToQ(x)		
#else
#define StdToPtr(x)			StdToL(x)
#endif
#define ConvertBE16(x)		Swap16(x)
#define ConvertBE32(x)		Swap32(x)
#define ConvertLE16(x)		x
#define ConvertLE32(x)		x
#endif
#define WToStd(x)			StdToW(x)
#define LToStd(x)			StdToL(x)
#define QToStd(x)			StdToQ(x)
#define PtrToStd(x)			StdToPtr(x)
#define RToStd(x)			StdToR(x)
#define R32ToStd(x)			StdToR32(x)
#define PToStd(x)			StdToP(x)

#if (ProcessorType==kX86) || (ProcessorType==kX64) || (ProcessorType==kM68000)
	#define UseGetSetIntMacros	1
#else
	#define UseGetSetIntMacros	0
#endif

#if UseGetSetIntMacros
#define GetAWord(p)			(*(int16*)(p))
#define SetAWord(p, x)		(*(int16*)(p) = x)
#define GetALong(p)			(*(int32*)(p))
#define SetALong(p, x)		(*(int32*)(p) = x)
#define GetAQuad(p)			(*(int64*)(p))
#define SetAQuad(p, x)		(*(int64*)(p) = x)
#else
int16			GetAWord(void *);
int16			SetAWord(void *, int16);
int32			GetALong(void *);
int32			SetALong(void *, int32);
int64			GetAQuad(void *);
int64			SetAQuad(void *, int64);
#endif

#if !Mac
typedef uInt32		ResType;
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

	amInitErr,				/* (Inter-)Application Message Manager 67- */

	occBadOccurrenceErr,	/* 68  Occurrence Mgr errors */
	occWaitOnUnBoundHdlrErr,
	occFunnyQOverFlowErr,

	fDataLogTypeConflict,	/* 71 */
	ecLVSBCannotBeCalledFromThread, /* ExtCode Mgr	72*/
	amUnrecognizedType,
	mCorruptErr,
	ecLVSBErrorMakingTempDLL,
	ecLVSBOldCIN,			/* ExtCode Mgr	76*/

	dragSktNotFound,		/* Drag Manager 77 - 80*/
	dropLoadErr,
	oleRegisterErr,
	oleReleaseErr,

	fmtTypeMismatch,		/* String processing (printf, scanf) errors */
	fmtUnknownConversion,
	fmtTooFew,
	fmtTooMany,
	fmtScanError,

	ecLVSBFutureCIN,		/* ExtCode Mgr	86*/

	lvOLEConvertErr,
	rtMenuErr,
	pwdTampered,			/* Password processing */
	LvVariantAttrNotFound,		/* LvVariant attribute not found 90-91*/
	LvVariantTypeMismatch,		/* Cannot convert to/from type */

	axEventDataNotAvailable,	/* Event Data Not Available 92-96*/
	axEventStoreNotPresent,		/* Event Store Not Present */
	axOccurrenceNotFound,		/* Occurence Not Found */
	axEventQueueNotCreated,		/* Event Queue not created */
	axEventInfoNotAvailable,	/* Event Info is not available */

	oleNullRefnumPassed,		/* Refnum Passed is Null */

	omidGetClassGUIDErr,		/* Error retrieving Class GUID from OMId 98-100*/
	omidGetCoClassGUIDErr,		/* Error retrieving CoClass GUID from OMId */
	omidGetTypeLibGUIDErr,		/* Error retrieving TypeLib GUID from OMId */

	appMagicBad,				/* bad built application magic bytes */

	iviInvalidDowncast,         /* IVI Invalid downcast*/
	iviInvalidClassSesn,		/* IVI No Class Session Opened */

	maxErr,						/* max manager 104-107 */
	maxConfigErr,				/* something not right with config objects */
	maxConfigLoadErr,			/* could not load configuration */
	maxGroupNotSupported,

	ncSockNotMulticast,			/* net connection multicast specific errors 108-112 */
	ncSockNotSinglecast,
	ncBadMulticastAddr,
	ncMcastSockReadOnly,
	ncMcastSockWriteOnly,

	ncDatagramMsgSzErr,			/* net connection Datagram message size error 113 */

	bufferEmpty,				/* CircularLVDataBuffer (queues/notifiers) */
	bufferFull,					/* CircularLVDataBuffer (queues/notifiers) */
	dataCorruptErr,				/* error unflattening data */

	requireFullPathErr,			/* supplied folder path where full file path is required  */
	folderNotExistErr,			/* folder doesn't exist */

	ncBtInvalidModeErr,			/* invalid Bluetooth mode 119 */
	ncBtSetModeErr,				/* error setting Bluetooth mode 120 */

	mgBtInvalidGUIDStrErr,		/* The GUID string is invalid 121 */

	rVersInFuture,			/* Resource file is a future version 122 */

	mgErrSentinel,		/* 123 */

	mgPrivErrBase = 500,	/* Start of Private Errors */
	mgPrivErrLast = 599,	/* Last allocated in Error DB */


	kAppErrorBase = 1000,	/* Start of application errors */
	kAppInvalidEvent = 1325, /* Invalid event refnum */
	kAppLicenseErr = 1380,	/* Failure to check out license error */
	kAppCharsetConvertErr =1396, /* could not convert text from charset to charset */
	kAppErrorLast = 1399	/* Last allocated in Error DB */
};

#define Rtm64(x)	(((int32)(x)) + 63	& 0xFFFFFFc0)	/* round up to mod 64 */
#define Rtm16(x)	(((int32)(x)) + 15	& 0xFFFFFFF0)	/* round up to mod 16 */
#define Rtm8(x)		(((int32)(x)) + 7	& 0xFFFFFFF8)	/* round up to mod 8 */
#define Rtm4(x)		(((int32)(x)) + 3	& 0xFFFFFFFc)	/* round up to mod 4 */
#define Rtm2(x)		(((int32)(x)) + 1	& 0xFFFFFFFe)	/* round up to mod 2 */
#if Is64Bit
#define RtmPtr(x)	Rtm8(x)								/* round up to mod pointer size */
#else
#define RtmPtr(x)	Rtm4(x)								/* round up to mod pointer size */
#endif

typedef struct
{
    int16 v;
    int16 h;
} LVPoint;

#define PStrBuf(b)  (&((PStr)(b))[1])
#define PStrLen(b)  (((PStr)(b))[0])    /* # of chars in string */
#define PStrSize(b) (PStrLen(b)+1)      /* # of bytes including length */

int32 CToPStr(ConstCStr src, PStr dest);
int32 PStrCpy(PStr d, ConstPStr s);
int32 StrCpy(CStr t, const CStr s);
int32 StrNCpy(CStr t, const CStr s, int32 l);
int32 StrLen(ConstCStr str);
int32 StrCmp(ConstCStr str1, ConstCStr str2);
int32 StrNCmp(ConstCStr, ConstCStr, size_t);
int32 StrNCaseCmp(ConstCStr, ConstCStr, size_t);

/** @brief Concatenated Pascal string types. */
typedef struct {
    int32	cnt;		/* number of pascal strings that follow */
    uChar	str[1];		/* cnt bytes of concatenated pascal strings */
} CPStr, *CPStrPtr, **CPStrHandle;

typedef struct {
    int32   cnt; 
    uChar   str[1];
} LStr, *LStrPtr, **LStrHandle;
typedef LStr const* ConstLStrP;
typedef LStr const*const* ConstLStrH;

#define LStrLen(p)  ((p)->cnt)
#define LStrBuf(p)  ((p)->str)

#define LStrLenH(h) ((h && *h) ? (*(h))->cnt : 0)
#define LStrBufH(h) ((h && *h) ? (*(h))->str : NULL)

/*** Concatenated Pascal String Support Functions ***/
#define CPStrLen		LStrLen			/* concatenated Pascal vs. LabVIEW strings */
#define CPStrBuf		LStrBuf			/* concatenated Pascal vs. LabVIEW strings */

typedef struct ATIME128 {
	union {
	  struct {
#if BigEndian
		int32 valHi;
	    uInt32 valLo;
	    uInt32 fractHi;
	    uInt32 fractLo;
#else
	    uInt32 fractLo;
	    uInt32 fractHi;
	    uInt32 valLo;
		int32 valHi;
#endif
	  } p;
	  struct {
#if BigEndian
	    int64 val;
	    uInt64 fract;
#else
	    uInt64 fract;
		int64 val;
#endif
	  } f;
	} u;
} ATime128, *ATime128Ptr;

/* Memory Manager */
UPtr DSNewPtr(size_t size);
UPtr DSNewPClr(size_t size);
MgErr DSDisposePtr(UPtr);
UHandle DSNewHandle(size_t size);
UHandle DSNewHClr(size_t size);
MgErr DSSetHandleSize(UHandle, size_t);
MgErr DSSetHSzClr(UHandle, size_t);
int32 DSGetHandleSize(UHandle);
MgErr DSDisposeHandle(UHandle);
MgErr DSCopyHandle(UHandle *ph, const UHandle hsrc);
MgErr DSSetHandleFromPtr(void *ph, const void *psrc, size_t n);

UHandle AZNewHandle(size_t size);
UHandle AZNewHClr(size_t size);
MgErr AZSetHandleSize(UHandle, size_t);
int32 AZGetHandleSize(UHandle);
MgErr AZDisposeHandle(UHandle);
MgErr AZCopyHandle(void *ph, const void *hsrc);
MgErr AZSetHandleFromPtr(void *ph, const void *psrc, size_t n);

void MoveBlock(const void *ps, void *pd, size_t size);
void ClearMem(void *pd, size_t size);

typedef		int32 (*CompareProcPtr)(const void*, const void*);
void QSort(UPtr, int32, int32, CompareProcPtr);
int32 BinSearch(ConstUPtr, int32, int32, UPtr, CompareProcPtr);

MgErr NumericArrayResize(int32, int32, UHandle*, size_t);
MgErr SetArraySize(int16 **tdp, int32 off, int32 dims, UHandle *p, int32 size);

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
typedef enum _FMFileType {

	kInvalidType			= 0,
	kUnknownFileType		= RTToL('?','?','?','?'),
    kTextFileType			= RTToL('T','E','X','T'),
    kLinkFileType			= RTToL('s','l','n','k'),
    kInstrFileType			= RTToL('L','V','I','N'),
    kDataLogType			= RTToL('L','V','D','L'),
    kResFileType			= RTToL('L','V','R','S'),
    kArcFileType			= RTToL('L','V','A','R'),
    kCustCtlFileType		= RTToL('L','V','C','C'),

    kMenuFileType			= RTToL('L','M','N','U'),
    kAnyInstrType			= RTToL('V','I','*',' '),	/* fake file type for LVIN or LVCC */
    kWizRegFileType			= RTToL('L','V','W','Z'),

    kExeFileType			= RTToL('E','X','E',' '),
    kComFileType			= RTToL('C','O','M',' '),
    kBatFileType			= RTToL('B','A','T',' '),
    kPifFileType			= RTToL('P','I','F',' '),

    kTempVIFileType			= RTToL('s','V','I','N'),
    kTempCustCtlFileType	= RTToL('s','V','C','C'),
    kRtmFileType			= RTToL('R','M','N','U'),

	/** Typical directory types */
	kHardDiskDirType		= RTToL('h','d','s','k'),
	kFloppyDirType			= RTToL('f','l','p','y'),
	kNetDriveDirType		= RTToL('s','r','v','r')
} FMFileType;

typedef enum  {
	kInvalidCreator			= 0,
	kUnknownCreator			= RTToL('?','?','?','?'),
	/** LabVIEW creator type */
	kLVCreatorType			= RTToL('L','B','V','W')
} FMFileCreator;

/** Used for FGetInfo */
typedef struct {			/**< file/directory information record */
	ResType type;			/**< system specific file type, kUnknownFileType for directories */
	ResType creator;		/**< system specific file creator, kUnknownFileType for directories */
	int32	permissions;	/**< system specific file access rights */
	int32	size;			/**< file size in bytes (data fork on Mac) or entries in folder */
	int32	rfSize;			/**< resource fork size (on Mac only) */
	uInt32	cdate;			/**< creation date */
	uInt32	mdate;			/**< last modification date */
	Bool32	folder;			/**< indicates whether path refers to a folder */
	Bool32	isInvisible;    /**< indicates whether the file is visible in File Dialog */
	LVPoint location;		/**< system specific geographical location */
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
	ResType type;			/**< system specific file type-- 0 for directories */
	ResType creator;		/**< system specific file creator-- 0 for directories */
	int32	permissions;	/**< system specific file access rights */
	int64	size;			/**< file size in bytes (data fork on Mac) or entries in folder */
	int64	rfSize;			/**< resource fork size (on Mac only) */
	uInt32	cdate;			/**< creation date */
	uInt32	mdate;			/**< last modification date */
	Bool32	folder;			/**< indicates whether path refers to a folder */
	Bool32	isInvisible;    /**< indicates whether the file is visible in File Dialog */
	LVPoint location;		/**< system specific geographical location */
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
	uInt32 fileFlags;
	ResType fileType;
} FMListDetails;

/** @brief Data types used to describe a list of entries from a directory. */
typedef CPStr FDirEntRec, *FDirEntPtr, **FDirEntHandle;

/** Type Flags used with FMListDetails */
#define kIsFile				0x0001
#define kRecognizedType		0x0002
#define kIsLink				0x0004
#define kFIsInvisible		0x0008
#define kIsTopLevelVI		0x0010	/* Used only for VIs in archives */
#define kErrGettingType		0x0020	/* error occurred getting type info */
#if Mac
#define kFIsStationery		0x0040
#endif
#define kIsCompressed		0x0080
#define kIsArchive		    0x0100  /* kIsFile and kRecognizedType should also be set */
    
enum { openNormal, openReplace, openCreate, openOpenOrCreate, openReplaceOrCreate}; /* open modes */
enum { accessReadWrite, accessReadOnly, accessWriteOnly}; /* access modes */
enum { denyReadWrite, denyWriteOnly, denyNeither}; /* deny modes */

#define kNoBuffering	0x00010000

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

#define kMaxFileExtLength		12				/* can handle .framework */

#if usesWinPath
 #define kPathSeperator			'\\'
 typedef HANDLE					FileRefNum;
#else
 // We always use posix APIs on non-Windows platforms
 #define kPathSeperator			'/'
 typedef FILE*					FileRefNum;
#endif
#define kRelativePathPrefix		'.'
#define kPosixPathSeperator		'/'

Private(File);
typedef struct PATHREF PathRef;
typedef PathRef* Path;
typedef const PathRef* ConstPath;

typedef MagicCookie LVRefNum;
#define kNotARefNum ((LVRefNum)0L)	/* canonical invalid magic cookie */

/* LabVIEW exported functions */
MgErr FRelPath(ConstPath start, ConstPath end, Path relPath);
MgErr FAddPath(ConstPath basePath, ConstPath relPath, Path newPath);
MgErr FAppendName(Path path, ConstPStr name);
MgErr FName(ConstPath path, PStrHandle name);
MgErr FNamePtr(ConstPath path, PStr name);
MgErr FDirName(ConstPath path, Path dir);
MgErr FVolName(ConstPath path, Path vol);
Path FMakePath(Path path, int32 type, ...);
Path FEmptyPath(Path);
Path FNotAPath(Path);
Bool32 FIsAPath(Path path);
Bool32 FIsEmptyPath(Path path);
MgErr FTextToPath(UPtr str, int32 len, Path* path);
MgErr FPathToText(Path path, LStrPtr lstr);
MgErr FPathToPath(Path *p);
Bool32 FIsAPathOfType(Path path, int32 ofType);
MgErr FGetPathType(Path, int32*);
int32 FDepth(Path path);
MgErr FDisposePath(Path p);

MgErr FNewRefNum(Path path, File fd, LVRefNum* refnum);
Bool32 FIsARefNum(LVRefNum);
MgErr FRefNumToFD(LVRefNum, File*);
MgErr FDisposeRefNum(LVRefNum);
Bool32 FExists(ConstPath path);
MgErr FGetInfo(ConstPath path, FInfoPtr infop);
MgErr FGetInfo64(ConstPath path, FInfo64Ptr infop, FGetInfoWhich which);
MgErr FSetInfo(ConstPath path, FInfoPtr infop);
MgErr FSetInfo64(ConstPath path, FInfo64Ptr infop);
MgErr FMRead(File fd, int32 inCount, int32* outCount, UPtr buffer);
MgErr FMWrite(File fd, int32 inCount, int32* outCount, UPtr buffer);
MgErr FListDir(ConstPath path, FDirEntHandle list, FMListDetails **);
uInt32 PStrHasRezExt(PStr str);
uInt32 HasRezExt(Path path);

int32 DbgPrintf(CStr fmt, ...);

#define Min(a, b)      ((a) < (b)) ? (a) : (b) 
#define Max(a, b)      ((a) > (b)) ? (a) : (b) 

typedef struct
{
	int32 numItems;
	LStrHandle elm[1];
} LStrArrRec, *LStrArrPtr, **LStrArrHdl;

typedef struct
{
	uInt32 flags;
	ResType type;
} FileTypeRec, *FileTypePtr;

typedef struct
{
	int32 numItems;
	FileTypeRec elm[1];
} FileTypeArrRec, *FileTypeArrPtr, **FileTypeArrHdl;

/*
Resource types
*/
#define dataResource		RTToL('D','A','T','A')
#define flagResource		RTToL('F','L','A','G')
#define initResource		RTToL('I','N','I','T')
#define lblResource			RTToL('L','B','L',' ')
#define controlType			RTToL('L','V','C','T')
#define nodeTypeLV			RTToL('L','V','N','D')
#define LabVIEWConfigResource RTToL('L','V','C','F')
#define docType				instrFileType
#define idType				RTToL('R','S','I','D')	/* int32 resource id		*/
#define codeVIType			RTToL('L','V','S','B')
#define rectResource		RTToL('R','E','C','T')
#define specResource		RTToL('S','P','E','C')
#define serialNoResource	RTToL('S','R','N','O')	/* uncoded serialNo */
#define hidSerialNoResource RTToL('F','L','A','G')	/* this is where the coded serialNo is hid */
#define stringResource		RTToL('S','T','R',' ')
#define strgResource		RTToL('S','T','R','G')
#define stringListResource	RTToL('S','T','R','#')
#define lStrListResource	RTToL('L','S','T','#')
#define mappedStrResource	RTToL('S','T','R','M')
#define versionResource		RTToL('v','e','r','s')
#define paletteMenuResource RTToL('P','A','L','M')
#define aDirResource		RTToL('A','D','i','r')
#define compFileResource	RTToL('C','P','R','F')
#define uncompFileResource	RTToL('U','C','R','F')
#define printResource		RTToL('P','R','T',' ')
#define printJobResource	RTToL('P','J','O','B')
#define printMarginResource RTToL('P','M','A','R')
#define platformResource	RTToL('P','L','A','T')
#define extListResource		RTToL('E','X','T','L')
#define hObjListResource	RTToL('H','O','B','J')
#define cPStrHandResource	RTToL('C','P','S','T')
#define filterListResource	RTToL('F','D','F','L')

#define paletteMenu2Resource	RTToL('P','L','M','2')
#define pathListResource	RTToL('L','P','T','H')
#define pathResource		RTToL('P','A','T','H')
#define icl4ResType			RTToL('i', 'c', 'l', '4')
#define icl8ResType			RTToL('i', 'c', 'l', '8')
#define iconResType			RTToL('I', 'C', 'O', 'N')

#ifdef __cplusplus
}
#endif

#endif /* _lvTypes_H */
