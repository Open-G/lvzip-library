#include "zlib.h"
#include "lvutil.h"
#include "utf.h"

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

__inline uInt8 utf_mask8(uInt8 oc)
{
    return (uInt8)(0xff & oc);
}

__inline uInt16 utf_mask16(uInt16 oc)
{
    return (uInt16)(0xffff & oc);
}

__inline LVBoolean utf_is_trail8(uInt8 oc)
{
    return ((utf_mask8(oc) >> 6) == 0x2);
}

__inline LVBoolean utf_is_lead_surrogate16(uInt16 cp)
{
    return (cp >= LEAD_SURROGATE_MIN && cp <= LEAD_SURROGATE_MAX);
}

__inline LVBoolean utf_is_trail_surrogate16(uInt16 cp)
{
    return (cp >= TRAIL_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX);
}

__inline LVBoolean utf_is_surrogate32(uInt32 cp)
{
    return (cp >= LEAD_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX);
}

__inline LVBoolean utf_is_code_point_valid32(uInt32 cp)
{
    return (cp <= CODE_POINT_MAX && !utf_is_surrogate32(cp));
}

__inline int32 utf_sequence_length8(uInt8 oc)
{
    uInt8 lead = utf_mask8(oc);
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

__inline LVBoolean is_overlong_sequence32(uInt32 cp, int32 length)
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

static MgErr utf_append(uInt32 cp, uInt8 *buffer, int32 *offset, int32 length)
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

static MgErr utf_next(uInt8 *buffer, int32 *offset, int32 length, uInt32 *cp)
{
	if (*offset < length)
	{
		uInt8 *ptr = buffer + *offset;
		int32 len = utf_sequence_length8(*ptr);
		if (len <= 0 || *offset + len > length)
			return mgArgErr;
		*cp = utf_mask8(*ptr++);
	    switch (len)
	    {
            case 1:
                break;
            case 2:
                *cp = ((*cp << 6) & 0x7ff) + (*ptr++ & 0x3f);
                break;
            case 3:
                *cp = ((*cp << 12) & 0xffff) + ((utf_mask8(*ptr++) << 6) & 0xfff);
                *cp += (*ptr++ & 0x3f);
                break;
            case 4:
                *cp = ((*cp << 18) & 0x1fffff) + ((utf_mask8(*ptr++) << 12) & 0x3ffff);                
			    *cp += (utf_mask8(*ptr++) << 6) & 0xfff;
			    *cp += (*ptr++ & 0x3f); 
                break;
        }
        *offset = (int32)(ptr - buffer); 
	    return noErr;        
	}
	return mgArgErr;
}

MgErr utf16to8 (uInt16 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length)
{       
	MgErr err = noErr;
	uInt16 *send = src + slen;
	uInt32 cp;
    while (!err && src < send)
	{
        cp = utf_mask16(*src++);
        // Take care of surrogate pairs first
        if (utf_is_lead_surrogate16(cp) && src < send)
		{
            uInt32 trail_surrogate = utf_mask16(*src++);
            cp = (cp << 10) + trail_surrogate + SURROGATE_OFFSET;
        }
        err = utf_append(cp, dest, offset, length);
    }
    return err;         
}

MgErr utf8to16(uInt8 *src, int32 slen, uInt16 *dest, int32 *offset, int32 length)
{
 	MgErr err = noErr;
	int32 off = 0;
	uInt32 cp;
    while (!err && off < slen)
	{
        err = utf_next(src, &off, slen, &cp);
		if (!err)
		{
			if (cp > 0xffff && *offset + 2 <= length)
		    {
				//make a surrogate pair
				if (dest)
				{
					*dest++ = (uInt16)((cp >> 10)   + LEAD_OFFSET);
					*dest++ = (uInt16)((cp & 0x3ff) + TRAIL_SURROGATE_MIN);
				}
				*offset += 2;
			}
			else if (*offset < length)
			{
				if (dest)
				{
					*dest++ = (uInt16)(cp);
				}
				*offset += 1;
			}
			else
			{
				err = mgArgErr;
			}
		}
	}
    return err;
}

MgErr utf32to8(uInt32 *src, int32 slen, uInt8 *dest, int32 *offset, int32 length)
{
 	MgErr err = noErr;
	uInt32 *send = src + slen;
	int32 off = 0;
    while (!err && src < send)
	{
        err = utf_append(*src++, dest, offset, length);
	}
    return err;
}

MgErr utf8to32(uInt8 *src, int32 slen, uInt32 *dest, int32 *offset, int32 length)
{
 	MgErr err = noErr;
	int32 off = 0;
	uInt32 cp;
    while (!err && off < slen && *offset < length)
	{
        err = utf_next(src, &off, slen, &cp);
		if (!err)
		{
			if (dest)
			{
				*dest++ = cp;
			}
			(*offset)++;
		}
	}
    return err;
}
