/*
   utf.c - Support routines for Unicode text format conversions between the different sizes

   Copyright (C) 2015-2024 Rolf Kalbermatter

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
#include "utf.h"
#include <stdlib.h>

// Unicode constants
// Leading (high) surrogates: 0xd800 - 0xdbff
// Trailing (low) surrogates: 0xdc00 - 0xdfff
#define LEAD_SURROGATE_MIN  0xd800u
#define LEAD_SURROGATE_MAX  0xdbffu
#define TRAIL_SURROGATE_MIN 0xdc00u
#define TRAIL_SURROGATE_MAX 0xdfffu
#define LEAD_OFFSET         LEAD_SURROGATE_MIN - (0x10000 >> 10)
#define SURROGATE_OFFSET    0x10000u - (LEAD_SURROGATE_MIN << 10) - TRAIL_SURROGATE_MIN

// Maximum valid value for a Unicode code point
#define CODE_POINT_MAX      0x0010ffffu

static MgErr utf8_is_valid(const uInt8 *src, int32 *offset, int32 length);
static MgErr utf8_replace_invalid(const uInt8 *src, int32 *soff, int32 slen, uInt8 *dest, int32 *doff, int32 dlen, uInt32 replacement);
static MgErr utf8_advance(const uInt8 *src, int32 *offset, int32 length, int32 distance);
static MgErr utf8_distance(const uInt8 *src, int32 *offset, int32 length, int32 *distance);

static LVBoolean utf8_is_trail(uInt8 oc)
{
    return ((oc & 0xC0) == 0x80);
}

static LVBoolean utf16_is_lead_surrogate(uInt16 cp)
{
    return (LVBoolean)(cp >= LEAD_SURROGATE_MIN && cp <= LEAD_SURROGATE_MAX);
}

/*
static LVBoolean utf16_is_trail_surrogate(uInt16 cp)
{
    return (LVBoolean)(cp >= TRAIL_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX);
}
*/

static LVBoolean utf32_is_surrogate(uInt32 cp)
{
    return (LVBoolean)(cp >= LEAD_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX);
}

static LVBoolean utf32_is_code_point_valid(uInt32 cp)
{
    return (LVBoolean)(cp <= CODE_POINT_MAX && !utf32_is_surrogate(cp));
}

static int32 utf8_sequence_length(const uInt8 *lead)
{
    if (*lead < 0x80)                       // 0x0xxxxxxx
        return 1;
    else if ((*lead & 0xE0) == 0xC0)        // 0x110xxxxx   
        return 2;
    else if ((*lead & 0xF0) == 0xE0)        // 0x1110xxxx
        return 3;
    else if ((*lead & 0xF8) == 0xF0)        // 0x11110xxx
        return 4;
    return 0;
}

static LVBoolean utf32_is_overlong_sequence(uInt32 cp, int32 length)
{
    if (cp < 0x80)
    {
        if (length != 1)
            return LV_TRUE;
    }
    else if (cp < 0x800)
    {
        if (length != 2)
            return LV_TRUE;
    }
    else if (cp < 0x10000)
    {
        if (length != 3)
            return LV_TRUE;
    }
    return LV_FALSE;
}

#define NOT_ENOUGH_ROOM     -1
#define INCOMPLETE_SEQUENCE -2
#define INVALID_LEAD        -3
#define OVERLONG_SEQUENCE   -4
#define INVALID_CODE_POINT  -5

static MgErr utf8_increase_safely(const uInt8 *src, int32 *offset, int32 length)
{
    if (++(*offset) >= length)
        return NOT_ENOUGH_ROOM;

    if (!utf8_is_trail(src[*offset]))
        return INCOMPLETE_SEQUENCE;

    return mgNoErr;
}

#define UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length) {MgErr ret = utf8_increase_safely(src, offset, length); if (ret) return ret;}

// utf8_get_sequence_x functions decode utf-8 sequences of the length x
static MgErr utf8_get_sequence_1(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    if (*offset >= length)
        return NOT_ENOUGH_ROOM;

    *code_point = src[*offset];
	++(*offset);
    return mgNoErr;
}

static MgErr utf8_get_sequence_2(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    if (*offset >= length)
        return NOT_ENOUGH_ROOM;

    *code_point = src[*offset];

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point = ((*code_point << 6) & 0x7c0) + (src[*offset] & 0x3f);
	++(*offset);
    return mgNoErr;
}

static MgErr utf8_get_sequence_3(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    if (*offset >= length)
        return NOT_ENOUGH_ROOM;

    *code_point = src[*offset];

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point = ((*code_point << 12) & 0xf000) + ((src[*offset] << 6) & 0xfc0);

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point += src[*offset] & 0x3f;
	++(*offset);
    return mgNoErr;
}

static MgErr utf8_get_sequence_4(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    if (*offset >= length)
        return NOT_ENOUGH_ROOM;

    *code_point = src[*offset];

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point = ((*code_point << 18) & 0x1c0000) + ((src[*offset] << 12) & 0x3f000);

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point += (src[*offset] << 6) & 0xfc0;

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point += src[*offset++] & 0x3f;

    return mgNoErr;
}

static MgErr utf8_append(uInt32 cp, uInt8 *buffer, int32 *offset, int32 length)
{
    if (buffer)
    {
        buffer = buffer + *offset;
    }

    if (cp < 0x80)
    {
        // one octet
        if (length < *offset + 1)
            return mgArgErr;
        if (buffer)
        {
            *(buffer++) = (uInt8)(cp & 0x7F);
        }
        *offset += 1;
    }
    else if (cp < 0x800)
    {
        // two octets
        if (length < *offset + 2)
            return mgArgErr;
        if (buffer)
        {
            *(buffer++) = (uInt8)((cp >> 6)          | 0xc0);
            *(buffer++) = (uInt8)((cp & 0x3f)        | 0x80);
        }
        *offset += 2;
    }
    else if (cp < 0x10000)
    {
        // three octets
        if (length < *offset + 3)
            return mgArgErr;
        if (buffer)
        {
            *(buffer++) = (uInt8)((cp >> 12)         | 0xe0);
            *(buffer++) = (uInt8)(((cp >> 6) & 0x3f) | 0x80);
            *(buffer++) = (uInt8)((cp & 0x3f)        | 0x80);
        }
        *offset += 3;
    }
    else
    {
        // four octets
        if (length < *offset + 4)
            return mgArgErr;
        if (buffer)
        {
            *(buffer++) = (uInt8)((cp >> 18)         | 0xf0);
            *(buffer++) = (uInt8)(((cp >> 12) & 0x3f)| 0x80);
            *(buffer++) = (uInt8)(((cp >> 6) & 0x3f) | 0x80);
            *(buffer++) = (uInt8)((cp & 0x3f)        | 0x80);
        }
        *offset += 4;
    }
    return mgNoErr;
}

MgErr utf8_next(const uInt8 *buffer, int32 *offset, int32 length, uInt32 *cp)
{
    if (*buffer && (length < 0 || *offset < length))
    {
        const uInt8 *ptr = buffer + *offset;
        int32 len = utf8_sequence_length(ptr);
        if (len <= 0 || (length >= 0 && *offset + len > length))
            return mgArgErr;
        *cp = *ptr++;
        switch (len)
        {
            case 1:
                break;
            case 2:
                *cp = ((*cp << 6) & 0x7ff) + (*ptr++ & 0x3f);
                break;
            case 3:
                *cp = ((*cp << 12) & 0xffff) + ((*ptr++ << 6) & 0xfff);
                *cp += (*ptr++ & 0x3f);
                break;
            case 4:
                *cp = ((*cp << 18) & 0x1fffff) + ((*ptr++ << 12) & 0x3ffff);
                *cp += (*ptr++ << 6) & 0xfff;
                *cp += (*ptr++ & 0x3f);
                break;
        }
        *offset = (int32)(ptr - buffer);
        return mgNoErr;
    }
    return mgArgErr;
}

static MgErr utf8_prior(const uInt8 *src, int32 *offset, int32 length, uInt32 *cp)
{
    // can't do much if it == start
    if (*offset <= 0)
        return mgArgErr;

    // Go back until we hit either a lead octet or start
    while (utf8_is_trail(src[*offset--]))
    {
        if (*offset < 0)
            return mgArgErr;
    }
    return utf8_next(src, offset, length, cp);
}

MgErr utf8_advance(const uInt8 *src, int32 *offset, int32 length, int32 distance)
{
    MgErr err = mgNoErr;
    int32 i;
    uInt32 cp;
    for (i = 0; !err && i < distance; ++i)
    {
        err = utf8_next(src, offset, length, &cp);
    }
    return err;
}

MgErr utf8_distance(const uInt8 *src, int32 *offset, int32 length, int32 *distance)
{
    MgErr err = mgNoErr;
    uInt32 cp;
    for (*distance = 0; !err && src[*offset] && (length < 0 || *offset < length); (*distance)++)
    {
        err = utf8_next(src, offset, length, &cp);
    }
    return err;
}

MgErr utf8_validate_next(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    MgErr err = mgNoErr;
    uInt32 cp = 0;

    // Save the original value of offset so we can go back in case of failure
    // Of course, it does not make much sense with i.e. stream iterators
    int32 off = *offset;

    // Determine the sequence length based on the lead octet
    int32 len = utf8_sequence_length(src + *offset);

    // Get trail octets and calculate the code point
    switch (len)
    {
        case 0:
            return INVALID_LEAD;
        case 1:
            err = utf8_get_sequence_1(src, offset, length, &cp);
            break;
        case 2:
            err = utf8_get_sequence_2(src, offset, length, &cp);
            break;
        case 3:
            err = utf8_get_sequence_3(src, offset, length, &cp);
            break;
        case 4:
            err = utf8_get_sequence_4(src, offset, length, &cp);
            break;
    }

    if (!err)
    {
        // Decoding succeeded. Now, security checks...
        if (utf32_is_code_point_valid(cp))
        {
            if (!utf32_is_overlong_sequence(cp, length))
            {
                if (code_point)
                    *code_point = cp;
                offset++;
                return mgNoErr;
            }
            else
                err = OVERLONG_SEQUENCE;
        }
        else
            err = INVALID_CODE_POINT;
    }

    if (err)
		// Restore the original value of the offset
		*offset = off;
    return err;
}

static MgErr utf8_replace_invalid(const uInt8 *src, int32 *soff, int32 slen, uInt8 *dest, int32 *doff, int32 dlen, uInt32 replacement)
{
	MgErr err = mgNoErr;
    while (src[*soff] && (slen < 0 || *soff < slen))
    {
        int32 off = *soff;
        err = utf8_validate_next(src, soff, slen, NULL);
        switch (err)
		{
            case mgNoErr:
                for (; off < *soff && *doff < dlen; off++, (*doff)++)
                    dest[*doff] = src[off];
                break;
            case NOT_ENOUGH_ROOM:
                return err;
            case INVALID_LEAD:
                err = utf8_append(replacement, dest, doff, dlen);
                ++(*soff);
                break;
            case INCOMPLETE_SEQUENCE:
            case OVERLONG_SEQUENCE:
            case INVALID_CODE_POINT:
                err = utf8_append(replacement, dest, doff, dlen);
                ++(*soff);
                // just one replacement mark for the sequence
                while (src[*soff] && (slen < 0 || *soff < slen) && utf8_is_trail(src[*soff]))
                    ++(*soff);
                break;
        }
    }
    return err;
}

static MgErr utf8_is_valid(const uInt8 *src, int32 *offset, int32 length)
{
	MgErr err = mgNoErr;
	while (!err && src[*offset] && (length < 0 || *offset < length))
	{
		err = utf8_validate_next(src, offset, length, NULL);
	}
	return err;
}

MgErr utf16to8(const uInt16 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length)
{
    MgErr err = mgNoErr;
	int32 soff = 0, doff = offset ? *offset : 0;
    uInt32 cp = 0;
    while (!err && src[soff] && (slen < 0 || soff < slen))
    {
        cp = src[soff++];
        // Take care of surrogate pairs first
        if (utf16_is_lead_surrogate((uInt16)cp) && (slen < 0 || soff < slen))
        {
            uInt32 trail_surrogate = src[soff++];
            cp = (cp << 10) + trail_surrogate + SURROGATE_OFFSET;
        }
        err = utf8_append(cp, dest, &doff, length);
    }
	if (!err && offset)
		*offset = doff;
    return err;
}

MgErr utf8to16(const uInt8 *src, int32 slen, uInt16 *dest, int32 *offset, int32 length)
{
    MgErr err = mgNoErr;
    int32 soff = 0, doff = offset ? *offset : 0;
    uInt32 cp = 0;
    while (!err && src[soff] && (slen < 0 || soff < slen))
    {
        err = utf8_next(src, &soff, slen, &cp);
        if (!err)
        {
            if (cp > 0xffff && (!dest || doff + 2 <= length))
            {
                //make a surrogate pair
                if (dest)
                {
                    dest[doff++] = (uInt16)((cp >> 10)   + LEAD_OFFSET);
                    dest[doff++] = (uInt16)((cp & 0x3ff) + TRAIL_SURROGATE_MIN);
                }
				else
					doff += 2;
            }
            else if (!dest || doff < length)
            {
                if (dest)
                {
                    dest[doff] = (uInt16)(cp);
                }
				doff++;
            }
            else
            {
                err = mgArgErr;
            }
        }
    }
	if (!err && offset)
		*offset = doff;
    return err;
}

MgErr utf32to8(const uInt32 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length)
{
    MgErr err = mgNoErr;
    int32 soff = 0, doff = offset ? *offset : 0;
    while (!err && src[soff] && (slen < 0 || soff < slen))
    {
        err = utf8_append(src[soff++], dest, &doff, length);
    }
	if (!err && offset)
		*offset = doff;
    return err;
}

MgErr utf8to32(const uInt8 *src, int32 slen, uInt32 *dest, int32 *offset, int32 length)
{
    MgErr err = mgNoErr;
    int32 soff = 0, doff = offset ? *offset : 0;
    uInt32 cp = 0;
    while (!err && src[soff] && (slen < 0 || soff < slen) && (!dest || doff < length))
    {
        err = utf8_next(src, &soff, slen, &cp);
        if (!err)
        {
            if (dest)
            {
                dest[doff] = cp;
            }
            doff++;
        }
    }
    if (!err && offset)
        *offset = doff;
    return err;
}

LibAPI(MgErr) utf8towchar(const uInt8 *src, int32 slen, wchar_t *dest, int32 *offset, int32 length)
{
#if WCHAR_MAX <= 0xFFFFu
    return utf8to16(src, slen, (uInt16*)dest, offset, length);
#else
    return utf8to32(src, slen, (uInt32*)dest, offset, length);
#endif
}

LibAPI(MgErr) wchartoutf8(const wchar_t *src, int32 slen, uInt8 *dest, int32 *offset, int32 length)
{
#if WCHAR_MAX <= 0xFFFFu
    return utf16to8((uInt16*)src, slen, dest, offset, length);
#else
    return utf32to8((uInt32*)src, slen, dest, offset, length);
#endif
}

static wchar_t testchar = 0x00DF;

LibAPI(LVBoolean) utf8_is_current_mbcs()
{
    unsigned char result[7] = {0};
    int len = wctomb((char*)result, testchar);
    if ((len == 2) && (result[0] == 0xC3) && (result[1] == 0x9F))
	{
        return LV_TRUE;
	}
	return LV_FALSE;
}
