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

__inline uInt8 utf8_mask(uInt8 oc)
{
    return (uInt8)(0xff & oc);
}

__inline uInt16 utf16_mask(uInt16 oc)
{
    return (uInt16)(0xffff & oc);
}

__inline LVBoolean utf8_is_trail(uInt8 oc)
{
    return ((utf8_mask(oc) >> 6) == 0x2);
}

__inline LVBoolean utf16_is_lead_surrogate(uInt16 cp)
{
    return (LVBoolean)(cp >= LEAD_SURROGATE_MIN && cp <= LEAD_SURROGATE_MAX);
}

__inline LVBoolean utf16_is_trail_surrogate(uInt16 cp)
{
    return (LVBoolean)(cp >= TRAIL_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX);
}

__inline LVBoolean utf32_is_surrogate(uInt32 cp)
{
    return (LVBoolean)(cp >= LEAD_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX);
}

__inline LVBoolean utf32_is_code_point_valid(uInt32 cp)
{
    return (LVBoolean)(cp <= CODE_POINT_MAX && !utf32_is_surrogate(cp));
}

__inline int32 utf8_sequence_length(uInt8 oc)
{
    uInt8 lead = utf8_mask(oc);
    if (lead < 0x80)
        return 1;
    else if ((lead >> 5) == 0x6)
        return 2;
    else if ((lead >> 4) == 0xe)
        return 3;
    else if ((lead >> 3) == 0x1e)
        return 4;
    return 0;
}

__inline LVBoolean utf32_is_overlong_sequence(uInt32 cp, int32 length)
{
    if (cp < 0x80)
    {
        if (length != 1)
            return 1;
    }
    else if (cp < 0x800)
    {
        if (length != 2)
            return 1;
    }
    else if (cp < 0x10000)
    {
        if (length != 3)
            return 1;
    }
    return 0;
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

    return noErr;
}

#define UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length) {MgErr ret = utf8_increase_safely(src, offset, length); if (ret) return ret;}

// utf8_get_sequence_x functions decode utf-8 sequences of the length x
static MgErr utf8_get_sequence_1(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    if (*offset >= length)
        return NOT_ENOUGH_ROOM;

    *code_point = utf8_mask(src[*offset]);
	++(*offset);
    return noErr;
}

static MgErr utf8_get_sequence_2(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    if (*offset >= length)
        return NOT_ENOUGH_ROOM;

    *code_point = utf8_mask(src[*offset]);

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point = ((*code_point << 6) & 0x7c0) + (src[*offset] & 0x3f);
	++(*offset);
    return noErr;
}

static MgErr utf8_get_sequence_3(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    if (*offset >= length)
        return NOT_ENOUGH_ROOM;

    *code_point = utf8_mask(src[*offset]);

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point = ((*code_point << 12) & 0xf000) + ((utf8_mask(src[*offset]) << 6) & 0xfc0);

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point += src[*offset] & 0x3f;
	++(*offset);
    return noErr;
}

static MgErr utf8_get_sequence_4(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    if (*offset >= length)
        return NOT_ENOUGH_ROOM;

    *code_point = utf8_mask(src[*offset]);

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point = ((*code_point << 18) & 0x1c0000) + ((utf8_mask(src[*offset]) << 12) & 0x3f000);

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point += (utf8_mask(src[*offset]) << 6) & 0xfc0;

    UTF8_INCREASE_AND_RETURN_ON_ERROR(src, offset, length)

    *code_point += src[*offset++] & 0x3f;

    return noErr;
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
        if (buffer)
        {
            if (length < *offset + 1)
                return mgArgErr;
            *(buffer++) = (uInt8)(cp & 0x7F);
        }
        *offset += 1;
    }
    else if (cp < 0x800)
    {
        // two octets
        if (buffer)
        {
            if (length < *offset + 2)
                return mgArgErr;
            *(buffer++) = (uInt8)((cp >> 6)          | 0xc0);
            *(buffer++) = (uInt8)((cp & 0x3f)        | 0x80);
        }
        *offset += 2;
    }
    else if (cp < 0x10000)
    {
        // three octets
        if (buffer)
        {
            if (length < *offset + 3)
                return mgArgErr;
            *(buffer++) = (uInt8)((cp >> 12)         | 0xe0);
            *(buffer++) = (uInt8)(((cp >> 6) & 0x3f) | 0x80);
            *(buffer++) = (uInt8)((cp & 0x3f)        | 0x80);
        }
        *offset += 3;
    }
    else
    {
        // four octets
        if (buffer)
        {
            if (length < *offset + 4)
                return mgArgErr;
            *(buffer++) = (uInt8)((cp >> 18)         | 0xf0);
            *(buffer++) = (uInt8)(((cp >> 12) & 0x3f)| 0x80);
            *(buffer++) = (uInt8)(((cp >> 6) & 0x3f) | 0x80);
            *(buffer++) = (uInt8)((cp & 0x3f)        | 0x80);
        }
        *offset += 4;
    }
    return noErr;
}

static MgErr utf8_next(const uInt8 *buffer, int32 *offset, int32 length, uInt32 *cp)
{
    if (*buffer && (length < 0 || *offset < length))
    {
        const uInt8 *ptr = buffer + *offset;
        int32 len = utf8_sequence_length(*ptr);
        if (len <= 0 || (length >= 0 && *offset + len > length))
            return mgArgErr;
        *cp = utf8_mask(*ptr++);
        switch (len)
        {
            case 1:
                break;
            case 2:
                *cp = ((*cp << 6) & 0x7ff) + (*ptr++ & 0x3f);
                break;
            case 3:
                *cp = ((*cp << 12) & 0xffff) + ((utf8_mask(*ptr++) << 6) & 0xfff);
                *cp += (*ptr++ & 0x3f);
                break;
            case 4:
                *cp = ((*cp << 18) & 0x1fffff) + ((utf8_mask(*ptr++) << 12) & 0x3ffff);
                *cp += (utf8_mask(*ptr++) << 6) & 0xfff;
                *cp += (*ptr++ & 0x3f);
                break;
        }
        *offset = (int32)(ptr - buffer);
        return noErr;
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
    MgErr err = noErr;
    int32 i = 0;
    uInt32 cp;
    for (; !err && i < distance; ++i)
    {
        err = utf8_next(src, offset, length, &cp);
    }
    return err;
}

MgErr utf8_distance(const uInt8 *src, int32 *offset, int32 length)
{
    MgErr err = noErr;
    int32 i = 0;
    uInt32 cp;
    for (; !err && src[*offset] && (length < 0 || *offset < length); i++)
    {
        err = utf8_next(src, offset, length, &cp);
    }
    return err;
}

MgErr utf8_validate_next(const uInt8 *src, int32 *offset, int32 length, uInt32 *code_point)
{
    MgErr err = noErr;
    uInt32 cp = 0;

    // Save the original value of offset so we can go back in case of failure
    // Of course, it does not make much sense with i.e. stream iterators
    int32 off = *offset;

    // Determine the sequence length based on the lead octet
    int32 len = utf8_sequence_length(src[*offset]);

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
                return noErr;
            }
            else
                err = OVERLONG_SEQUENCE;
        }
        else
            err = INVALID_CODE_POINT;
    }

    // Restore the original value of the offset
    *offset = off;
    return err;
}

MgErr utf8_replace_invalid(const uInt8 *src, int32 *soff, int32 slen, uInt8 *dest, int32 *doff, int32 dlen, uInt32 replacement)
{
	MgErr err = noErr;
    while (src[*soff] && (slen < 0 || *soff < slen))
    {
        int32 off = *soff;
        err = utf8_validate_next(src, soff, slen, NULL);
        switch (err)
		{
            case noErr:
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

MgErr utf8_is_valid(const uInt8 *src, int32 *offset, int32 length)
{
	MgErr err = noErr;
	while (!err && src[*offset] && (length < 0 || *offset < length))
	{
		err = utf8_validate_next(src, offset, length, NULL);
	}
	return err;
}

MgErr utf16to8(const uInt16 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length)
{
    MgErr err = noErr;
	int32 soff = 0, doff = offset ? *offset : 0;
    uInt32 cp = 0;
    while (!err && src[soff] && (slen < 0 || soff < slen))
    {
        cp = utf16_mask(src[soff++]);
        // Take care of surrogate pairs first
        if (utf16_is_lead_surrogate((uInt16)cp) && (slen < 0 || soff < slen))
        {
            uInt32 trail_surrogate = utf16_mask(src[soff++]);
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
    MgErr err = noErr;
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
    MgErr err = noErr;
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
    MgErr err = noErr;
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

LibAPI(LVBoolean) utf8_is_current_mbcs()
{
    static wchar_t test = 0x00B0;
    LVBoolean is_utf8 = LV_FALSE;
    char *result = malloc(MB_CUR_MAX);
    if (result)
    {
        int len = wctomb(NULL, test);
        len = wctomb(result, test);
        if ((len == 2) && (result[0] == (char)0xC2) && (result[1] == (char)0xB0))
            is_utf8 = LV_TRUE;
        free(result);
    }
    return is_utf8;
}
