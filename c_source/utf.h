/*
   utf.h - Support routines for Unicode text format conversions between the different sizes

   Copyright (C) 2015-2018 Rolf Kalbermatter

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
#include "zlib.h"
#include "lvutil.h"
#include <stddef.h>

#ifndef _utfHelper_H
#define _utfHelper_H

#ifdef __cplusplus
extern "C" {
#endif

LibAPI(LVBoolean) utf8_is_current_mbcs(void);

MgErr utf8_validate_next(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point);

/*
 src is expected to contain a valid, and optionally null terminated, utf8 string.
 The function will read the src string until it encounters either a null character
 or slen has been reached, where slen is in bytes, not in utf8 codepoints. Passing
 -1 for slen will process src until a null character is encountered.
 Pass in NULL for dest to determine the size of the necessary buffer which will be
 returned in *offset, length is in that case not used.
*/
LibAPI(MgErr) utf8towchar(const uInt8 *src, int32 slen, wchar_t *dest, int32 *offset, int32 length);
LibAPI(MgErr) wchartoutf8(const wchar_t *src, int32 slen, uInt8 *dest, int32 *offset, int32 length);

MgErr utf16to8(const uInt16 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length);
MgErr utf8to16(const uInt8 *src, int32 slen, uInt16 *dest, int32 *offset, int32 length);
MgErr utf32to8(const uInt32 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length);
MgErr utf8to32(const uInt8 *src, int32 slen, uInt32 *dest, int32 *offset, int32 length);

#ifdef __cplusplus
}
#endif

#endif /* _lvUtil_H */
