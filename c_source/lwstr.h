/*
   lwstr.h -- widechar/shortchar unicode string support functions for LabVIEW integration

   Version 1.1, Sept 27th, 2019

   Copyright (C) 2017-2019 Rolf Kalbermatter

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
#ifndef _lwStr_H
#define _lwStr_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvtypes.h"
#include <string.h>

/* The LWStr datatype is a special construct to represent platform specific path strings that is never
   meant to be passed to LabVIEW since its inherent data size is not the same across platforms!
   It consists of a UTF16 string for Windows platforms except Pharlap to directly match the format
   of the Windows Wide character API. On other platforms it is a normal 8 bit string that uses the
   current platform encoding scheme, which could be UTF8 on newer Linux and MacOSX platforms.
*/
#if Win32 && !Pharlap
typedef wchar_t			LWChar;
#define lwslen			(int32)wcslen
#define lwsncpy			wcsncpy
#define lwsrchr			wcsrchr
#else
typedef char			LWChar;

#define lwslen			(int32)strlen
#define lwsncpy			strncpy
#define lwsrchr			strrchr       
#endif
#define LWPathSize(h)        (DSGetHandleSize((UHandle)h) - (2 * sizeof(int32))) / sizeof(LWChar)

#define LWPathLenGet(h)      (((h) && *(h)) ? (int32)(((*(h))->byteCnt - sizeof(int32)) / sizeof(LWChar)) : 0)
#define LWPathLenSet(h, l)   ((*(h))->byteCnt = (int32)(((l) * sizeof(LWChar)) + sizeof(int32)), (*(h))->str[l] = 0)

#define LWPathCntGet(h)      (((h) && *(h)) ? (*(h))->pathCnt : 0)
#define LWPathCntSet(h, c)   (((h) && *(h)) ? (*(h))->pathCnt = (uInt16)(c) : 0)

#define LWPathTypeGet(h)     (((h) && *(h)) ? ((*(h))->type) : fAbsPath)
#define LWPathTypeSet(h, t)  (((h) && *(h)) ? (*(h))->type = (uInt8)(t) : fAbsPath)

#define LWPathFlagsGet(h)    (((h) && *(h)) ? ((*(h))->flags) : 0)
#define LWPathFlagsSet(h, f) (((h) && *(h)) ? (*(h))->flags = (uInt8)(f) : 0)

#define LWPathBuf(h)         (((h) && *(h)) ? (*h)->str : NULL)
#define SStrBuf(h)           (char*)LWPathBuf(h)

#if MSWin && (ProcessorType == kX86)
	/* Windows x86 targets use 1-byte structure packing. */
	#pragma pack(push, 1)
#elif Mac
	/* Use natural alignment on the Mac. */
	#if (Compiler == kGCC) || (Compiler == kMetroWerks)
		#pragma options align=natural
	#else
		#error "Unsupported compiler. Figure out how to set alignment to native/natural"
	#endif
#else
	/* Use default (or build's) alignment */
#endif /* struct alignment set */

typedef struct LWPATHREC
{
	int32 byteCnt;         /* In order to be able to pass this as a byte array/string handle we always use size in bytes */ 
	int8 flags;            /* If highest significant bit is set we have a Unicode path */
	int8 type;             /* LabVIEW path structure uses an int16 for path type stored in big endian format in the flattened stream */
	int16 pathCnt;         /* Number of path segemetns in the path */
	LWChar str[1];         /* actual path, widechar in windows, 8 byte in all other platforms */
} LWPathRec, *LWPathPtr, **LWPathHandle;

typedef struct WSTRREC
{
	int32 cnt;             /* In order to be able to pass this as a byte array/string handle we always use size in bytes */ 
	wchar_t wstr[1];
} WStrRec, *WStrPtr, **WStrHandle;

#if MSWin && (ProcessorType == kX86)
	#pragma pack(pop)
#elif Mac
	#pragma options align=reset
#endif /* struct alignment restore */

#define WStrLen(h)			((h) && *(h)) ? (int32)((*(h))->cnt / sizeof(wchar_t)) : 0
#define WStrLenSet(h, l)    ((h) && *(h)) ? (*(h))->cnt = (int32)(l * sizeof(wchar_t)) : 0
#define WStrBuf(h)			((h) && *(h)) ? (*(h))->wstr : NULL

#if Win32 && !Pharlap
#define HasDOSDevicePrefix(str, len)                                                                                                       \
 ((len >= 4 && str[0] == kPathSeperator && str[1] == kPathSeperator && (str[2] == '?' || str[2] == '.') && str[3] == kPathSeperator) ?     \
  ((len >= 8 && (str[4] == 'u' || str[4] == 'U') && (str[5] == 'n' || str[5] == 'N') && (str[6] == 'c' || str[6] == 'C') && str[7] == kPathSeperator) ? 8 : 4) : 0)
#else
#define HasDOSDevicePrefix(str, len)   0
#endif

int32 LWPathRootLen(LWPathHandle pathName, int32 offset, uInt8 *type);
Bool32 LWPathIsOfType(LWPathHandle pathName, int32 offset, uInt8 type);
MgErr LWPathAppendSeparator(LWPathHandle pathName);
int32 LWPathDepth(LWPathHandle pathName, int32 end);
int32 LWPathParentInternal(LWPathHandle filePath, int32 rootLen, int32 end);
int32 LWPathNextElement(LWPathHandle filePath, int32 start, int32 end);
int32 LWPathParent(LWPathHandle pathName, int32 end);
MgErr LWPathAppend(LWPathHandle pathName, int32 end, LWPathHandle *newPath, LWPathHandle relPath);
MgErr LWPathGetFileTypeAndCreator(LWPathHandle pathName, ResType *fType, ResType *fCreator);
MgErr LWPathNormalize(LWPathHandle pathName);
MgErr LWPathZeroTerminate(LWPathHandle pathName, LWChar *end);

MgErr LWPathResize(LWPathHandle *buf, size_t numChar);
MgErr LWPathDispose(LWPathHandle buf);
MgErr LWPathNCat(LWPathHandle *lwstr, int32 off, const LWChar *str, int32 strLen);

typedef enum
{
	kDefaultPath = 0,
	kLeaveHFSPath = 1,
	kCvtKeepDOSDevice = 2
} CvtFlags;

MgErr LStrToLWPath(const LStrHandle string, uInt32 codePage, LWPathHandle *lwstr, uInt32 flags, int32 reserve);
MgErr LStrFromLWPath(LStrHandle *pathName, uInt32 codePage, const LWPathHandle lwstr, int32 offset, uInt32 flags);
Bool32 LStrIsAPathOfType(const LStrHandle pathName, int32 offset, uInt8 type);
Bool32 LStrIsAbsPath(const LStrHandle pathName, int32 offset);

LibAPI(MgErr) UPathToLWPath(const LStrHandle pathName, LWPathHandle *lwstr, uInt32 flags);
LibAPI(MgErr) LPathToLWPath(const Path pathName, LWPathHandle *lwstr, uInt32 flags, int32 reserve);
LibAPI(MgErr) UPathFromLWPath(LStrHandle *pathName, const LWPathHandle *lwstr, uInt32 flags);
LibAPI(MgErr) LPathFromLWPath(Path *pathName, const LWPathHandle *lwstr, uInt32 flags);
LibAPI(int32) LStrRootPathLen(const LStrHandle pathName, int32 offset, uInt8 *type);

LibAPI(MgErr) LStrAppendPath(LStrHandle *pathName, LStrHandle relPath);
LibAPI(MgErr) LStrParentPath(LStrHandle pathName, LStrHandle *fileName, LVBoolean *empty);
LibAPI(MgErr) LRefAppendPath(LWPathHandle *pathName, LStrHandle relPath);
LibAPI(MgErr) LRefParentPath(LWPathHandle *pathName, LStrHandle *fileName, LVBoolean *empty);

/* Different conversion functions between various codepages */
#define CP_ACP                    0           // default to ANSI code page
#define CP_OEMCP                  1           // default to OEM  code page
#define CP_UTF8                   65001       // UTF-8 translation

LibAPI(uInt32) GetCurrentCodePage(LVBoolean acp);
LibAPI(LVBoolean) HasExtendedASCII(LStrHandle string);
LibAPI(MgErr) MultiByteCStrToWideString(ConstCStr src, int32 srclen, WStrHandle *dest, uInt32 codePage);
LibAPI(MgErr) MultiByteToWideString(const LStrHandle src, WStrHandle *dest, uInt32 codePage);
LibAPI(MgErr) WideStringToMultiByte(const WStrHandle src, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed);
LibAPI(MgErr) WideCStrToMultiByte(const wchar_t *src, int32 srclen, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed);

LibAPI(MgErr) ConvertCString(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed);
LibAPI(MgErr) ConvertLString(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed);
LibAPI(MgErr) ConvertFromPosixPath(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir);
LibAPI(MgErr) ConvertToPosixPath(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir);

#ifdef __cplusplus
}
#endif

#endif
