#include "zlib.h"
#include "lvutil.h"
#include <stddef.h>

#ifndef _utfHelper_H
#define _utfHelper_H

//#include "zlib.h"

#ifdef __cplusplus
extern "C" {
#endif

LVBoolean utf8_is_current_mbcs();

MgErr utf8_validate_next(uInt8 *src, int32 *offset, int32 length, uInt32 *code_point);

/*
 src is expected to contain a valid, and optionally null terminated, utf8 string.
 The function will read the src string until it encounters either a null character
 or slen has been reached, where slen is in bytes, not in utf8 codepoints. Passing
 -1 for slen will process src until a null character is encountered.
 Pass in NULL for dest to determine the size of the necessary buffer which will be
 returned in *offset, length is in that case not used.
*/
MgErr utf8towchar(uInt8 *src, int32 slen, wchar_t *dest, int32 *offset, int32 length);
MgErr wchartoutf8(wchar_t *src, int32 slen, uInt8 *dest, int32 *offset, int32 length);

MgErr utf16to8(uInt16 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length);
MgErr utf8to16(uInt8 *src, int32 slen, uInt16 *dest, int32 *offset, int32 length);
MgErr utf32to8(uInt32 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length);
MgErr utf8to32(uInt8 *src, int32 slen, uInt32 *dest, int32 *offset, int32 length);

#ifdef __cplusplus
}
#endif

#endif /* _lvUtil_H */
