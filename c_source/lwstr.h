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

#include "lvutil.h"
#include <string.h>

typedef struct
{
	int32 cnt;
	uInt16 str[1];
} WStrRec, *WStrPtr, **WStrHandle;

#define WStrLen(s)			(int32)(LStrLen(s) * sizeof(uInt16) / sizeof(wchar_t))
#define WStrLenSet(s, l)    LStrLen(s) = l * sizeof(wchar_t) / sizeof(uInt16)
#define WStrBuf(s)			((wchar_t*)(LStrBuf(s)))

/* The LWStr datatype is a special construct to represent platform specific path strings that is never
   meant to be passed to LabVIEW since its inherent data size is not the same across platforms!
   It consists of a UTF16 string for Windows platforms except Pharlap to directly match the format
   of the Windows Wide character API. On other platforms it is a normal 8 bit string that uses the
   current platform encoding scheme, which could be UTF8 on newer Linux and MacOSX platforms.
*/
#if Win32 && !Pharlap
#define LWStrPtr          WStrPtr
#define LWStrLen(p)       WStrLen(p)
#define LWStrBuf(p)       WStrBuf(p)
#define LWStrLenSet(p, s) WStrLenSet(p, s)

#define LWStrNCat         WStrNCat
#define lwslen            (int32)wcslen
#define lwsrchr           wcsrchr
#else
#define LWStrPtr          LStrPtr
#define LWStrLen(p)       LStrLen(p)
#define LWStrBuf(p)       LStrBuf(p)
#define LWStrLenSet(p, s) LStrLen(p) = s

#define LWStrNCat         LStrNCat
#define lwslen            (int32)strlen
#define lwsrchr           strrchr
#endif
#define SStrBuf(s)        (char*)LStrBuf(s)

LibAPI(int32) LStrIsAbsPath(LStrHandle pathName);
LibAPI(MgErr) LStrAppendPath(LStrHandle *pathName, LStrHandle relPath);
LibAPI(MgErr) LStrParentPath(LStrHandle pathName, LStrHandle *fileName);

Bool32 LWStrIsAbsPath(LWStrPtr pathName);
int32 LWStrPathDepth(LWStrPtr pathName, int32 end);
int32 LWStrParentPath(LWStrPtr pathName, int32 end);
MgErr LWStrAppendPath(LWStrPtr pathName, int32 end, LWStrPtr relPath);

MgErr LWStrRealloc(LWStrPtr *buf, int32 *bufLen, int32 retain, size_t numChar);
MgErr LWStrDispose(LWStrPtr buf);

#if Win32 && !Pharlap
MgErr LWStrNCat(LWStrPtr lstr, int32 off, int32 bufLen, const wchar_t *str, int32 strLen);

#define HasDOSDevicePrefix(str, len)                                                                                                       \
 ((len >= 4 && str[0] == kPathSeperator && str[1] == kPathSeperator && (str[2] == '?' || str[2] == '.') && str[3] == kPathSeperator) ?     \
  ((len >= 8 && (str[4] == 'u' || str[4] == 'U') && (str[5] == 'n' || str[5] == 'N') && (str[6] == 'c' || str[6] == 'C') && str[7] == kPathSeperator) ? 6 : 4) : 0)
#else
#define HasDOSDevicePrefix(str, len)   0
MgErr LWStrNCat(LWStrPtr lstr, int32 off, int32 bufLen, const char *str, int32 strLen);
#endif

MgErr LWNormalizePath(LWStrPtr pathName);
MgErr LWAppendPathSeparator(LWStrPtr pathName, int32 bufLen);
MgErr LWStrGetFileTypeAndCreator(LWStrPtr pathName, FMFileType *fType, FMFileType *fCreator);

MgErr LStrPathToLWStr(LStrHandle string, uInt32 codePage, LWStrPtr *lwstr, int32 *reserve);
MgErr UPathToLWStr(LStrHandle pathName, LWStrPtr *lwstr, int32 *reserve);
MgErr LPathToLWStr(Path pathName, LWStrPtr *lwstr, int32 *reserve);

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
