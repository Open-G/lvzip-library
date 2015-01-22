#include "zlib.h"
#include "lvutil.h"

#ifndef _utf8_H
#define _utf8_H

//#include "zlib.h"

#ifdef __cplusplus
extern "C" {
#endif

MgErr utf16to8(uInt16 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length);
MgErr utf8to16(uInt8 *src, int32 slen, uInt16 *dest, int32 *offset, int32 length);
MgErr utf32to8(uInt32 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length);
MgErr utf8to32(uInt8 *src, int32 slen, uInt32 *dest, int32 *offset, int32 length);

#ifdef __cplusplus
}
#endif

#endif /* _lvUtil_H */
