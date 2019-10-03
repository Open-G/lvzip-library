/*
   lwstr.c -- widechar/shortchar unicode string support functions for LabVIEW integration

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
#include "lvutil.h"
#include "lwstr.h"
#if Win32
#include <windows.h>
#elif MacOSX
 #include <sys/xattr.h>
#elif Unix
 #include <stdio.h>
 #ifdef HAVE_ICONV
  #include <iconv.h>
 #endif
#endif

MgErr LWStrRealloc(LWStrPtr *buf, int32 *bufLen, int32 retain, size_t numChar)
{
	if (!*buf || *bufLen <= (int32)numChar)
	{
		LWStrPtr ptr = (LWStrPtr)DSNewPtr(sizeof(int32) + ++numChar * sizeof(LWStrBuf(*buf)[0]));
		if (!ptr)
			return mFullErr;

		if (!*buf)
			retain = 0;
		else if (LWStrLen(*buf) < retain)
			retain = LWStrLen(*buf);

		if (retain)
		{
			MoveBlock((void*)LWStrBuf(*buf), (void*)LWStrBuf(ptr), retain * sizeof(LWStrBuf(ptr)[0]));
		}
		LWStrLenSet(ptr, retain);
		DSDisposePtr((UPtr)*buf);
		*buf = ptr;
		if (bufLen)
			*bufLen = (int32)numChar;
	}
	return noErr;
}

MgErr LWStrDispose(LWStrPtr buf)
{
	return DSDisposePtr((UPtr)buf);
}

#if Win32
MgErr LWNormalizePath(LWStrPtr lwstr)
{
#if Pharlap
	LPSTR a, s, e, t = LWStrBuf(lwstr);
#else
	LPWSTR a, s, e, t = LWStrBuf(lwstr);
#endif
	int32 unc = FALSE, len = LWStrLen(lwstr);

	a = s = t;
	e = s + len;
	if (!*s || !len)
	{
		*s = '\0';
		return noErr;
	}
/*
	\\?\UNC\server\share
	\\?\C:\
	\\.\UNC\server\share
	\\.\C:\
	\\server\share
	C:\
*/

	if (s[0] == kPathSeperator)
	{
		int32 n = 1;
		s++;
		if (s[0] == kPathSeperator)
		{
			n++;
			s++;
			if ((s[0] == '?' || s[0] == '.') && s[1] == kPathSeperator)
			{
				n += 2;
				s += 2;
				if ((s[0] == 'U' || s[0] == 'u') && (s[1] == 'N' || s[1] == 'n') && (s[2] == 'C' || s[2] == 'C') && s[3] == kPathSeperator)
				{
					unc = TRUE;
					n += 4;
					s += 4;
				}
			}
			else if (isalpha(s[0]))
			{
				unc = TRUE;
			}
			else
			{
				return mgArgErr;
			}
		}
		t += n;
		a = s;
	}
	if (isalpha(s[0]) && s[1] == ':')
	{
		/* X:\ */
		s += 2;
		t += 2;
		if (s[0] == kPathSeperator)
		{
			s++; t++;
		}
	}

	/* Canonicalize the rest of the path */
	while (s < e)
	{
		if (s[0] == '.')
		{
			if (s[1] == kPathSeperator && (s == a || s[-1] == kPathSeperator || s[-1] == ':'))
			{
				s += 2; /* Skip .\ */
			}
			else if (s[1] == '.' && (t == a || t[-1] == kPathSeperator))
			{
				/* \.. backs up a directory, over the root if it has no \ following X:.
				 * .. is ignored if it would remove a UNC server name or initial \\
				 */
				if (t != a)
				{
					if (t > a + 1 && t[-1] == kPathSeperator &&
					    (t[-2] != kPathSeperator || t > a + 2))
					{
						if (t[-2] == ':' && (t > a + 3 || t[-3] == ':'))
						{
							t -= 2;
							while (t > a && *t != kPathSeperator)
								t--;
							if (*t == kPathSeperator)
								t++; /* Reset to last '\' */
							else
								t = a; /* Start path again from new root */
						}
						else if (t[-2] != ':' && !unc)
							t -= 2;
					}
					while (t > a && *t != kPathSeperator)
						t--;
					if (t == a)
					{
						*t = kPathSeperator;
						s++;
					}
				}
				s += 2; /* Skip .. in src path */
			}
			else
				*t++ = *s++;
		}
		else
			*t++ = *s++;
	}
	/* Append \ to naked drive spec */
	if (t - a == 2 && t[-1] == ':')
		*t++ = kPathSeperator;
	*t++ = '\0';
	return noErr;
}
#elif Unix || MacOSX
MgErr LWNormalizePath(LWStrPtr lwstr)
{
	char *a, *s, *e, *t;
	int32 len = LWStrLen(lwstr);

	a = s = t = (char*)LWStrBuf(lwstr);
	e = s + len;
	if (!*s || !len)
		return noErr;
  
	/* Canonicalize the path */
	while (s < e)
	{
		if (s[0] == kPosixPathSeperator && s[1] == kPosixPathSeperator)
		{
				s++; /* Reduce // to / */ 
		}
		else if (s[0] == '.')
		{
			if (s[1] == kPosixPathSeperator && (s == a && s[-1] == kPosixPathSeperator))
			{
				s += 2; /* Skip ./ */
			}
			else if (s[1] == '.')
			{
				/* .. backs up a directory but only if it is not at the root of the path
				 */
				if (t == a || (t > a + 2 && t[-1] == kPosixPathSeperator && t[-2] == '.'))
				{
					*t++ = *s++;
					*t++ = *s++;
				}
				else if (t > a + 2 && t[-1] == kPosixPathSeperator && (t[-2] != kPosixPathSeperator))
				{
					t -= 2;
					while (t > a && *t != kPosixPathSeperator)
						t--;
					if (*t == kPosixPathSeperator)
						t++; /* Reset to last '\' */
					s += 3; /* Skip ../ in src path */
				}
				else
					return mgArgErr;
			}
			else
				*t++ = *s++;
		}
		else
			*t++ = *s++;
	}
	if (*t == kPosixPathSeperator && t > a + 1)
		t--;
	*t = '\0';
	return noErr;
}
#endif

#if Unix || MacOSX || Pharlap
MgErr LWStrNCat(LWStrPtr lstr, int32 off, int32 bufLen, const char *str, int32 strLen)
{
	if (strLen == -1)
	    strLen = strlen(str);
	if (off == -1)
		off = LWStrLen(lstr);
	if (off + strLen < bufLen)
	{
		strncpy(SStrBuf(lstr) + off, str, strLen);
		LWStrBuf(lstr)[off + strLen] = 0;
		LWStrLenSet(lstr, off + strLen);
		return noErr;
	}
	return mgArgErr;
}
#elif Win32
MgErr LWStrNCat(LWStrPtr lstr, int32 off, int32 bufLen, const wchar_t *str, int32 strLen)
{
	if (strLen == -1)
	    strLen = (int32)wcslen(str);
	if (off == -1)
		off = LWStrLen(lstr);
	if (off + strLen < bufLen)
	{
		wcsncpy(LWStrBuf(lstr) + off, str, strLen);
		LWStrBuf(lstr)[off + strLen] = 0;
		LWStrLenSet(lstr, off + strLen);
		return noErr;
	}
	return mgArgErr;
}
#endif

Bool32 LStrIsAbsPath(LStrHandle filePath)
{
	int32 len = LStrLenH(filePath);
	if (len)
	{
		const char *ptr = (const char*)LStrBufH(filePath);
#if usesWinPath
#if !Pharlap
		int32 offset = HasDOSDevicePrefix(ptr, len);
		if (offset == 8 || (!offset && ptr[0] == kPathSeperator && ptr[1] == kPathSeperator && ptr[2] < 128 && isalpha(ptr[2])) ||
			(len >= 3 + offset && ptr[offset] < 128 && isalpha(ptr[offset]) && ptr[offset + 1] == ':' && ptr[offset + 2] == kPathSeperator))
#else
		if ((len >= 3 && (ptr[0] < 128 && isalpha(ptr[0]) && ptr[1] == ':' && ptr[2] == kPathSeperator) || 
				         (ptr[0] == kPathSeperator && ptr[1] == kPathSeperator && ptr[2] < 128 && isalpha(ptr[2]))))
#endif
#else
		if (ptr[0] == kPosixPathSeperator)
#endif
			return LV_TRUE;
	}
	else
	{
		/* len == 0 is root path and always absolute */
		return LV_TRUE;
	}
	return LV_FALSE;
}

Bool32 LWStrIsAbsPath(LWStrPtr filePath)
{
	int32 len = filePath ? LWStrLen(filePath) : 0;
	if (len)
	{
#if usesWinPath
#if !Pharlap
		wchar_t *ptr = LWStrBuf(filePath);
		int32 offset = HasDOSDevicePrefix(LWStrBuf(filePath), len);
		if (offset == 8 || (!offset && ptr[0] == kPathSeperator && ptr[1] == kPathSeperator && ptr[2] < 128 && isalpha(ptr[2])) ||
			(len >= 3 + offset && ptr[offset] < 128 && isalpha(ptr[offset]) && ptr[offset + 1] == ':' && ptr[offset + 2] == kPathSeperator))
#else
		char *ptr = LWStrBuf(filePath);
		if ((len >= 3 && (ptr[0] < 128 && isalpha(ptr[0]) && ptr[1] == ':' && ptr[2] == kPathSeperator) || 
				         (ptr[0] == kPathSeperator && ptr[1] == kPathSeperator && ptr[2] < 128 && isalpha(ptr[2]))))
#endif
#else
		if (LWStrBuf(filePath)[0] == kPosixPathSeperator)
#endif
			return LV_TRUE;
	}
	else
	{
		/* len == 0 is root path and always absolute */
		return LV_TRUE;
	}
	return LV_FALSE;
}

MgErr LWAppendPathSeparator(LWStrPtr pathName, int32 bufLen)
{
	if (LWStrBuf(pathName)[LWStrLen(pathName) - 1] != kPathSeperator)
	{
		if (LWStrLen(pathName) + 1 >= bufLen)
		{
			return bufferFull;
		}
		LWStrBuf(pathName)[LWStrLen(pathName)] = kPathSeperator;
		LWStrLenSet(pathName, LWStrLen(pathName) + 1);
		LWStrBuf(pathName)[LWStrLen(pathName)] = 0;
	}
	return noErr;
}

int32 LWStrParentPath(LWStrPtr pathName, int32 end)
{
	if (end < 0)
		end = LWStrLen(pathName);
	while (end && LWStrBuf(pathName)[--end] != kNativePathSeperator);
	return end;
}

int32 LWStrPathDepth(LWStrPtr pathName, int32 end)
{
	int32 i = 0, 
		  offset = HasDOSDevicePrefix(LWStrBuf(pathName), LWStrLen(pathName));
	while (end > offset)
	{
		end = LWStrParentPath(pathName, end);
		i++;
	}
	return i;
}

MgErr LWStrAppendPath(LWStrPtr pathName, int32 end, LWStrPtr relPath)
{
	MgErr err = mgArgErr;
	if (!LWStrIsAbsPath(relPath))
		err = LWStrNCat(pathName, 0, end, LWStrBuf(relPath), LWStrLen(relPath));
	return err;
}

int32 LWStrRelativePath(LWStrPtr fromPath, LWStrPtr toPath)
{
	return 0;
}

static FMFileType LWStrFileTypeFromExt(LWStrPtr pathName)
{
	int32 len = LWStrLen(pathName),
		  k = 1;
#if Win32 && !Pharlap
    wchar_t *ptr = LWStrBuf(pathName);
#else
	char *ptr = LWStrBuf(pathName);
#endif
	/* look backwards	*/
	for (ptr += len; k < len && k < kMaxFileExtLength; --ptr, k++)
	{
		if (*ptr == '.')
		{
			uChar str[16];
            PStrLen(str) = (uChar)k - 1;
            for (len = 1; k; k--)
				str[len++] = (uChar)*ptr++;
			return PStrHasRezExt(str);
		}
	}
	return 0;
}

MgErr LWStrGetFileTypeAndCreator(LWStrPtr pathName, FMFileType *fType, FMFileType *fCreator)
{
	/* Try to determine LabVIEW file types based on file ending? */
	uInt32 creator = kUnknownCreator,
		   type = LWStrFileTypeFromExt(pathName);
	if (!type)
	{
#if MacOSX
		struct attrlist alist;
		fileinfobuf finfo;
		finderinfo *finder = (finderinfo*)(&finfo.data);

		bzero(&alist, sizeof(struct attrlist));
		alist.bitmapcount = ATTR_BIT_MAP_COUNT;
		if (objIndex == MACOSX_RSRCLENGTH_ATTRIBUTE)
		{
			alist.fileattr = ATTR_FILE_RSRCLENGTH;
		}
		else
		{
			alist.commonattr = ATTR_CMN_FNDRINFO;
		}
	    if (!getattrlist(SStrBuf(pathName), &alist, &finfo, sizeof(fileinfobuf), FSOPT_NOFOLLOW))
		{
			type = ConvertBE32(finder.type);
			creator = ConvertBE32(finder.creator);
		}
		else
#else
			type = kUnknownFileType;
#endif
	}
	else
	    creator = kLVCreatorType;

	if (fType)
		*fType = type;
	if (fCreator)
		*fCreator = creator;

	return noErr;
}

static MgErr LStrPathToLWStr(LStrHandle string, uInt32 codePage, LWStrPtr *lwstr, int32 *reserve)
{
	MgErr err = noErr;
	int32 bufLen = 0;
#if Unix || MacOSX || Pharlap
	LStrHandle dest = NULL;
	if (LStrLenH(string))
	{
#if usesHFSPath
		err = ConvertToPosixPath(string, codePage, &dest, CP_ACP, '?', NULL, FALSE);
#else
		err = ConvertLString(string, codePage, &dest, CP_ACP, '?', NULL);
#endif
	}
#if Unix || MacOSX
	else
	{
		bufLen = 1;
	}
#endif
	if (!err)
	{
		bufLen += LStrLenH(dest) + 1 + (reserve ? *reserve : 0);
		err = LWStrReallocBuf(lwstr, reserve, 0, bufLen);
		if (!err)
		{
			if (LStrLenH(string))
			{
				MoveBlock((UPtr)*dest, (UPtr)*lwstr, sizeof(int32) + LStrLenH(dest));
			}
#if Unix || MacOSX
			else
			{
				LStrBuf(*lwstr)[0] = kPathSeperator;
				LStrLen(*lwstr) = 1;
			}
#endif
			LStrBuf(*lwstr)[LStrLen(*lwstr)] = 0;
		}
		DSDisposeHandle((UHandle)dest);
	}
#else /* Windows and not Pharlap */
	LPCSTR buf = (LPCSTR)LStrBufH(string);
	int32 off = 0, len = 0;
	
	if (LStrLenH(string))
	{
		bufLen = MultiByteToWideChar(codePage, 0, buf, LStrLenH(string), NULL, 0);
		if (bufLen <= 0)
			return mgArgErr;

		/* Create DOS Device paths for absolute paths that aren't that yet */ 
		if (buf[0] < 128 && isalpha(buf[0]) && buf[1] == ':')
		{
			/* Absolute path with drive letter */
			len = 4;
		}
		else if (buf[0] == kPathSeperator && buf[1] == kPathSeperator && buf[2] < 128 && isalpha(buf[2]))
		{
			/* Absolute UNC path */
			len = 7;
			off = 1;
		}
	}
	bufLen += len + 1 + (reserve ? *reserve : 0);
	err = LWStrRealloc(lwstr, reserve, 0, bufLen);
	if (err)
		return err;

	if (LStrLenH(string))
	{
		if (len == 4)
		{
			LWStrNCat(*lwstr, 0, bufLen, L"\\\\?\\", len);
		}
		else if (len == 7)
		{
			LWStrNCat(*lwstr, 0, bufLen, L"\\\\?\\UNC", len);
		}
		bufLen = MultiByteToWideChar(CP_ACP, 0, buf + off, LStrLenH(string) - off, WStrBuf(*lwstr) + len, bufLen - len);
		if (bufLen <= 0)
			return mgArgErr;
		len += bufLen;
	}
	WStrLenSet(*lwstr, len);
	WStrBuf(*lwstr)[len] = 0;
#endif
	return LWNormalizePath(*lwstr);
}

/* Windows: path is an UTF8 encoded path string and lwstr is filled with a Windows UTF16LE string from the path
   MacOSX, Unix, VxWorks, Pharlap: path is an UTF8 encoded path string and lwstr is filled with a local encoded SBC or
   MBC string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
MgErr UPathToLWStr(LStrHandle path, LWStrPtr *lwstr, int32 *reserve)
{
	return LStrPathToLWStr(path, CP_UTF8, lwstr, reserve);
}

/* Windows: path is a LabVIEW path and lwstr is filled with a Windows UTF16LE string from this path
   MacOSX, Unix, VxWorks, Pharlap: path is a LabVIEW path and lwstr is filled with a local encoded SBC or MBC
   string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
MgErr LPathToLWStr(Path path, LWStrPtr *lwstr, int32 *reserve)
{
	LStrPtr lstr;
    int32 bufLen = -1;
    MgErr err = FPathToText(path, (LStrPtr)&bufLen);
    if (!err)
    {
		bufLen += 1 + (reserve ? *reserve : 0);
        lstr = (LStrPtr)DSNewPClr(sizeof(int32) + bufLen);
        if (!lstr)
            return mFullErr;
        LStrLen(lstr) = bufLen;
        err = FPathToText(path, lstr);
        if (!err)
			err = LStrPathToLWStr(&lstr, CP_ACP, lwstr, reserve);

		DSDisposePtr((UPtr)lstr);
    }
    return err;
}

#if MacOSX
static CFStringEncoding ConvertCodepageToEncoding(uInt32 codePage)
{
	CFStringEncoding encoding = CFStringConvertWindowsCodepageToEncoding(codePage);
	if (encoding == kCFStringEncodingInvalidId)
	{
		encoding = CFStringGetSystemEncoding();
	}
	return encoding;
}
#endif

LibAPI(uInt32) GetCurrentCodePage(LVBoolean acp)
{
#if Win32
	return acp ? GetACP() : GetOEMCP();
#elif MacOSX
    Unused(acp);
	CFStringEncoding encoding = CFStringGetSystemEncoding();
	return CFStringConvertEncodingToWindowsCodepage(encoding);
#else
    if (utf8_is_current_mbcs())
		return CP_UTF8;
	return acp ? CP_ACP : CP_OEMCP;
#endif
}

LibAPI(LVBoolean) HasExtendedASCII(LStrHandle string)
{
	if (string)
	{
		int32 i = LStrLen(*string) - 1;
		UPtr ptr = LStrBuf(*string);

		for (; i >= 0; i--)
		{
			if (*ptr++ >= 0x80)
			{
				return LV_TRUE;
			}
		}
	}
	return LV_FALSE;
}

LibAPI(MgErr) ConvertCString(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed)
{
	MgErr err = noErr;
	if (srccp != destcp)
	{
		WStrHandle ustr = NULL;
		err = MultiByteCStrToWideString(src, srclen, &ustr, srccp);
		if (!err && ustr)
		{
			err = WideStringToMultiByte(ustr, dest, destcp, defaultChar, defUsed);
			DSDisposeHandle((UHandle)ustr);
			return err;
		}
	}
	else
	{
		if (srclen == -1)
			srclen = StrLen(src);
		if (srclen > 0)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)dest, srclen);
			if (!err)
			{
				MoveBlock(src, LStrBuf(**dest), srclen);
				LStrLen(**dest) = srclen;
				return err;
			}
		}
	}
	if (*dest)
		LStrLen(**dest) = 0;
	return err;
}

/* Converts a Unix style path to a LabVIEW platform path */
LibAPI(MgErr) ConvertFromPosixPath(ConstCStr src, int32 srclen, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir)
{
    MgErr err = mgNotSupported;
#if usesWinPath
    Unused(isDir);
    err = ConvertCString(src, srclen, srccp, dest, destcp, defaultChar, defUsed);
	if (!err && *dest)
	{
		int32 len = LStrLen(**dest);
		UPtr buf = LStrBuf(**dest);
		if (src[0] == '/' && src[1] != '/')
		{
			*buf++ = src[1];
			*buf++ = ':';
			len -= 2;
		}
		for (; len; len--, buf++)
		{
			if (*buf == '/')
			{
				*buf = '\\';
			}
		}
	}
#elif usesPosixPath
    Unused(isDir);
	err = ConvertCString(src, srclen, srccp, dest, destcp, defaultChar, defUsed);
#elif usesHFSPath
    Unused(defUsed);
	CFStringEncoding encoding = ConvertCodepageToEncoding(srccp);
	if (encoding != kCFStringEncodingInvalidId)
	{
		CFURLRef urlRef = NULL;
		CFStringRef hfsRef, fileRef = CFStringCreateWithBytes(kCFAllocatorDefault, src, srclen, encoding, false);
		if (!fileRef)
		{
			return mFullErr;
		}
		urlRef = CFURLCreateWithFileSystemPath(NULL, fileRef, kCFURLPOSIXPathStyle, isDir);
		CFRelease(fileRef);
		if (!urlRef)
		{
			return mFullErr;
		}
		hfsRef = CFURLCopyFileSystemPath(urlRef, kCFURLHFSPathStyle);
		CFRelease(urlRef);
		if (hfsRef)
		{
			CFIndex len;
			CFRange range = CFRangeMake(0, CFStringGetLength(hfsRef));
			if (CFStringGetBytes(hfsRef, range, encoding, defaultChar, false, NULL, 0, &len) > 0)
			{
				if (len > 0)
				{
					err = NumericArrayResize(uB, 1, (UHandle*)dest, len + 1);
					if (!err)
					{
						encoding = ConvertCodepageToEncoding(destcp);
						if (encoding != kCFStringEncodingInvalidId)
						{
							if (CFStringGetBytes(hfsRef, range, encoding, 0, false, LStrBuf(**dest), len, &len) > 0)
							{
								LStrBuf(**dest)[len] = 0;
								LStrLen(**dest) = len;
								err = mgNoErr;
							}
							else
							{	
								err = bogusError;
							}
						}
					}
				}
			}
			CFRelease(hfsRef);
		}
		else
		{
			err = mFullErr;
		}
    }
#endif
	return err;
}

LibAPI(MgErr) ConvertLString(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed)
{
	MgErr err;
	if (srccp != destcp)
	{
		WStrHandle wstr = NULL;
		if (LStrLen(*src) > 0)
		{
			err = MultiByteToWideString(src, &wstr, srccp);
			if (!err && wstr)
			{
				err = WideStringToMultiByte(wstr, dest, destcp, defaultChar, defUsed);
				DSDisposeHandle((UHandle)wstr);
			}
			return err;
		}
	}
	else
	{
		err = NumericArrayResize(uB, 1, (UHandle*)dest, LStrLen(*src));
		if (!err)
		{
			MoveBlock((ConstCStr)*src, (CStr)**dest, LStrLen(*src) + sizeof(int32));
		}
		return err;
	}
	if (*dest)
		LStrLen(**dest) = 0;

	return mgNoErr;
}

#if !Unix || !defined(HAVE_ICONV)
static void TerminateLStr(LStrHandle *dest, int32 numBytes)
{
	LStrLen(**dest) = numBytes;
	LStrBuf(**dest)[numBytes] = 0;
}
#endif

/* Converts a LabVIEW platform path to Unix style path */
LibAPI(MgErr) ConvertToPosixPath(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir)
{
    MgErr err = mgNotSupported;
#if usesHFSPath
    Unused(defUsed);
	CFStringEncoding encoding = ConvertCodepageToEncoding(srccp);
	if (encoding != kCFStringEncodingInvalidId)
	{
		CFURLRef urlRef = NULL;
		CFStringRef posixRef, fileRef = CFStringCreateWithBytes(kCFAllocatorDefault, LStrBuf(*src), LStrLen(*src), encoding, false);
		if (!fileRef)
		{
			return mFullErr;
		}
		urlRef = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, fileRef, kCFURLHFSPathStyle, isDir);
		CFRelease(fileRef);
		if (!urlRef)
		{
			return mFullErr;
		}
		posixRef = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);
		CFRelease(urlRef);
		if (posixRef)
		{
			CFIndex len;
			CFRange range = CFRangeMake(0, CFStringGetLength(posixRef));
			if (CFStringGetBytes(posixRef, range, encoding, defaultChar, false, NULL, 0, &len) > 0)
			{
				if (len > 0)
				{
					err = NumericArrayResize(uB, 1, (UHandle*)dest, len + 1);
					if (!err)
					{
						encoding = ConvertCodepageToEncoding(destcp);
						if (encoding != kCFStringEncodingInvalidId)
						{
							if (CFStringGetBytes(posixRef, range, encoding, 0, false, LStrBuf(**dest), len, &len) > 0)
							{
								TerminateLStr(dest, len);
								err = mgNoErr;
							}
							else
							{	
								err = bogusError;
							}
						}
					}
				}
			}
			CFRelease(posixRef);
		}
		else
		{
			err = mFullErr;
		}
	}
#else
    Unused(isDir);
	err = ConvertLString(src, srccp, dest, destcp, defaultChar, defUsed);
#if usesWinPath
	if (!err && *dest)
	{
		int32 len = LStrLen(**dest);
		UPtr buf = LStrBuf(**dest);
		if (buf[1] == ':' && buf[2] == '\\')
		{
			*buf++ = '/';
			*buf++ = LStrBuf(*src)[0];
			len -= 2;
		}
		for (; len; len--, buf++)
		{
			if (*buf == '\\')
			{
				*buf = '/';
			}
		}
	}
#endif
#endif
	return err;
}

#if Unix
#ifdef HAVE_ICONV
static char *iconv_getcharset(uInt32 codePage)
{
	static char cp[10];
	switch (codePage)
	{
		case CP_ACP:
			return "CHAR";
		case CP_OEMCP:
			/* FIXME: determine current local and return according OEM */
			return "CP437";
		case CP_UTF8:
			return "UTF-8";
		default:
			snprintf(cp, 10, "CP%d", codePage);
			return cp;
	}
	return NULL;
}

static MgErr unix_convert_mbtow(const char *src, int32 len, WStrHandle *dest, uInt32 codePage)
{
	iconv_t cd = iconv_open("WCHAR_T", iconv_getcharset(codePage));
	if (cd != (iconv_t)-1)
	{
		MgErr err;
		if (len < 0)
			len = strlen(src);
		err = NumericArrayResize(uB, 1, (UHandle*)dest, len * sizeof(wchar_t));
		if (!err)
		{
			char *inbuf = (char*)src, *outbuf = (char*)LStrBuf(**dest);
			size_t retval, inleft = len, outleft = len * sizeof(wchar_t);
			retval = iconv(cd, &inbuf, &inleft, &outbuf, &outleft);
			if (retval == (size_t)-1)
				err = UnixToLVFileErr();
			else
			    WStrLenSet(**dest, (len * sizeof(wchar_t) - outleft) / sizeof(uInt16));
		}
		if (iconv_close(cd) != 0 && !err)
			err = UnixToLVFileErr();
		return err;
	}
	return UnixToLVFileErr();
}

static MgErr unix_convert_wtomb(const wchar_t *src, int32 srclen, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	MgErr err;
	iconv_t cd = iconv_open(iconv_getcharset(codePage), "WCHAR_T");
	Unused(defaultChar);
	Unused(defaultCharWasUsed);
	if (cd != (iconv_t)-1)
	{
		if (srclen == -1)
			srclen = wcslen(src);

		err = NumericArrayResize(uB, 1, (UHandle*)dest, srclen * sizeof(uInt16) * 2);
		if (!err)
		{
			char *inbuf = (char*)src, *outbuf = (char*)LStrBuf(**dest);
			size_t retval, inleft = srclen * sizeof(uInt16), outleft = srclen * sizeof(uInt16) * 2;
			retval = iconv(cd, &inbuf, &inleft, &outbuf, &outleft);
			if (retval == (size_t)-1)
				err = UnixToLVFileErr();
			else
				LStrLen(**dest) = srclen * 2 - outleft;
		}
		if (iconv_close(cd) != 0 && !err)
			err = UnixToLVFileErr();
		return err;
	}
	return UnixToLVFileErr();
}
#else // HAVE_ICONV
/* We don't have a good way of converting from an arbitrary character set to wide char and back.
   Just do default mbcs to wide char and vice versa ??? */
static MgErr unix_convert_mbtow(const char *src, int32 sLen, WStrHandle *dest, uInt32 codePage)
{
	size_t max, mLen;
	const char *sPtr = src;
	int32 wLen = 0;
	MgErr err = noErr;

	if (codePage == CP_UTF8)
	{
        err = utf8towchar(src, sLen, NULL, &wLen, 0);
	}
	else
	{
#ifdef HAVE_MBRTOWC
		mbstate_t mbs;
		mbrtowc(NULL, NULL, 0, &mbs);
#else
		mbtowc(NULL, NULL, 0);
#endif
		if (sLen < 0)
			sLen = strlen(src);
		max = sLen;
	    while (!err && max > 0)
	    {
#ifdef HAVE_MBRTOWC
		    mLen = mbrtowc(NULL, sPtr, max, &mbs);
#else
		    mLen = mbtowc(NULL, sPtr, max);
#endif
		    if (mLen == (size_t)-1 || mLen == (size_t)-2)
				err = kAppCharsetConvertErr;

			if (!mLen)
				break;

		    wLen++;
			sPtr += mLen;
			max -= mLen;
		}
    }
	if (!err && wLen > 0)
	{
		err = NumericArrayResize(uB, 1, (UHandle*)dest, (wLen + 1) * sizeof(wchar_t));
		if (!err)
		{
			if (codePage == CP_UTF8)
			{
				err = utf8towchar(src, sLen, WStrBuf(**dest), NULL, wLen + 1);
			}
			else
			{
				wchar_t *wPtr = WStrBuf(**dest);
#ifdef HAVE_MBRTOWC
				mbstate_t mbs;
				mbrtowc(NULL, NULL, 0, &mbs);
#else
				mbtowc(NULL, NULL, 0);
#endif
				max = sLen;
				sPtr = src;
				wLen = 0;
				while (!err && max > 0)
				{
#ifdef HAVE_MBRTOWC
					mLen = mbrtowc(wPtr++, sPtr, max, &mbs);
#else
					mLen = mbtowc(wPtr++, sPtr, max);
#endif
					if (mLen == (size_t)-1 || mLen == (size_t)-2)
					   err = kAppCharsetConvertErr;

					if (!mLen)
						break;

					wLen++;
					sPtr += mLen;
					max -= mLen;
				}
				if (!err && !max)
					WStrBuf(**dest)[wLen] = 0;
			}
		}
	}
	if (!err && *dest)
	    WStrLenSet(**dest, wLen * sizeof(wchar_t) / sizeof(uInt16)); 
	return err;
}

static MgErr unix_convert_wtomb(const wchar_t *src, int32 sLen, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	size_t max, mLen = 0, dLen = 0;
	const wchar_t *wPtr = src;
	MgErr err = noErr;

	if (defaultCharWasUsed)
		*defaultCharWasUsed = LV_FALSE;

	if (codePage == CP_UTF8)
	{
		err = wchartoutf8(src, sLen, NULL, &dLen, 0);
	}
	else
	{
#ifdef HAVE_WCRTOMB
		mbstate_t mbs;
		mLen = wcrtomb(NULL, 0, &mbs);
#else
		mLen = wctomb(NULL, 0);
#endif
		if (sLen < 0)
			sLen = wcslen(src);
		max = sLen;
	    while (max > 0)
	    {
#ifdef HAVE_MBRTOWC
			mLen = wcrtomb(NULL, *wPtr++, &mbs);
#else
			mLen = wctomb(NULL, *wPtr++);
#endif
			if (!mLen)
				break;

			if (mLen == (size_t)-1 || mLen == (size_t)-2)
			{
#ifdef HAVE_MBRTOWC
				mLen = wcrtomb(NULL, 0, &mbs);
#else	
				mLen = wctomb(NULL, 0);
#endif
				dLen++;
			}
			else
			{
				dLen += mLen;
			}
			max--;
		}
	}
	if (!err && dLen > 0)
	{
		err = NumericArrayResize(uB, 1, (UHandle*)dest, dLen + 1);
		if (!err)
		{
			if (codePage == CP_UTF8)
			{
				err = wchartoutf8(src, sLen, LStrBuf(**dest), NULL, dLen + 1);
			}
			else
			{
				char *dPtr = LStrBuf(**dest);
#ifdef HAVE_WCRTOMB
				mbstate_t mbs;
				mLen = wcrtomb(NULL, 0, &mbs);
#else
				mLen = wctomb(NULL, 0);
#endif
				wPtr = src;
				max = sLen;
				while (max > 0)
				{
#ifdef HAVE_MBRTOWC
					mLen = wcrtomb(dPtr, *wPtr++, &mbs);
#else
					mLen = wctomb(dPtr, *wPtr++);
#endif
					if (!mLen)
						break;

					if (mLen == (size_t)-1 || mLen == (size_t)-2)
					{
#ifdef HAVE_MBRTOWC
						mLen = wcrtomb(NULL, 0, &mbs);
#else	
						mLen = wctomb(NULL, 0);
#endif
						if (defaultCharWasUsed)
							*defaultCharWasUsed = LV_TRUE;
						*dPtr++ = defaultChar;
						dLen++;
					}
					else
					{
						dPtr += mLen;
						dLen += mLen;
					}
					max--;
				}
				if (!err && !max)
					LStrBuf(**dest)[dLen] = 0;
			}
		}
	}
	if (!err && *dest)
	    LStrLen(**dest) = dLen; 
	return err;
}
#endif
#endif

LibAPI(MgErr) MultiByteCStrToWideString(ConstCStr src, int32 srclen, WStrHandle *dest, uInt32 codePage)
{
	if (*dest)
		WStrLenSet(**dest, 0);
	if (src && src[0])
	{
		MgErr err = mgNotSupported;
#if Win32
		int32 numChars = MultiByteToWideChar(codePage, 0, (LPCSTR)src, srclen, NULL, 0);
		if (numChars > 0)
		{ 
			err = NumericArrayResize(uW, 1, (UHandle*)dest, numChars);
			if (!err)
			{
				LStrLen(**dest) = MultiByteToWideChar(codePage, 0, (LPCSTR)src, srclen, LStrBuf(**dest), numChars);	
			}
		}
#elif MacOSX
		CFStringEncoding encoding = ConvertCodepageToEncoding(codePage);
		if (encoding != kCFStringEncodingInvalidId)
		{
			CFStringRef cfpath = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)src, srclen, encoding, false);
			if (cfpath)
			{
				CFMutableStringRef cfpath2 = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, cfpath);
				CFRelease(cfpath);
				if (cfpath2)
				{
					CFIndex numChars;
					
					CFStringNormalize(cfpath2, kCFStringNormalizationFormC);
    
					numChars = CFStringGetLength(cfpath2);
					if (numChars > 0)
					{ 
						err = NumericArrayResize(uB, 1, (UHandle*)dest, numChars * sizeof(UniChar));
						if (!err)
						{
							CFStringGetCharacters(cfpath2, CFRangeMake(0, numChars), (UniChar*)LStrBuf(**dest));
							LStrLen(**dest) = numChars;
						}
					}
					CFRelease(cfpath2); 
				}
			}
		}
#elif Unix
	    err = unix_convert_mbtow((const char*)src, srclen, dest, codePage);
#endif
		return err;
	}
	return mgNoErr;
}

LibAPI(MgErr) MultiByteToWideString(const LStrHandle src, WStrHandle *dest, uInt32 codePage)
{
	return MultiByteCStrToWideString(LStrBuf(*src), LStrLen(*src), dest, codePage);
}

LibAPI(MgErr) ZeroTerminateLString(LStrHandle *dest)
{
	int32 size = LStrLenH(*dest);
	MgErr err = NumericArrayResize(uB, 1, (UHandle*)dest, size + 1);
	if (!err)
		LStrBuf(**dest)[size] = 0;
	return err;
}

LibAPI(MgErr) WideStringToMultiByte(const WStrHandle src, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	return WideCStrToMultiByte(WStrBuf(*src), WStrLen(*src), dest, codePage, defaultChar, defaultCharWasUsed);
}

LibAPI(MgErr) WideCStrToMultiByte(const wchar_t *src, int32 srclen, LStrHandle *dest, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	if (defaultCharWasUsed)
		*defaultCharWasUsed = LV_FALSE;
	if (dest && *dest)
		LStrLen(**dest) = 0;

	if (srclen < 0)
		srclen = (int32)wcslen(src);

	if (srclen > 0)
	{
		MgErr err = mgNotSupported;
#if Win32 
		int32 numBytes = WideCharToMultiByte(codePage, 0, src, srclen, NULL, 0, NULL, NULL);
		if (numBytes > 0)
		{ 
			BOOL defUsed;
			err = NumericArrayResize(uB, 1, (UHandle*)dest, numBytes + 1);
			if (!err)
			{
				numBytes = WideCharToMultiByte(codePage, 0, src, srclen, (LPSTR)LStrBuf(**dest), numBytes, &defaultChar, &defUsed);
				TerminateLStr(dest, numBytes);
				if (defaultCharWasUsed)
					*defaultCharWasUsed = (LVBoolean)(defUsed != FALSE);
			}
		}
#elif MacOSX
		CFStringRef cfpath = CFStringCreateWithCharacters(kCFAllocatorDefault, src, srclen);
		if (cfpath)
		{
			CFMutableStringRef cfpath2 = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, cfpath);
			CFRelease(cfpath);
			if (cfpath2)
			{
				CFStringNormalize(cfpath2, kCFStringNormalizationFormD);
				CFStringEncoding encoding = ConvertCodepageToEncoding(codePage);
				if (encoding != kCFStringEncodingInvalidId)
				{
					CFIndex numBytes;
					if (CFStringGetBytes(cfpath2, CFRangeMake(0, CFStringGetLength(cfpath2)), encoding, defaultChar, false, NULL, 0, &numBytes) > 0)
					{
						err = NumericArrayResize(uB, 1, (UHandle*)dest, numBytes + 1);
						if (!err)
						{
							CFStringGetBytes(cfpath2, CFRangeMake(0, CFStringGetLength(cfpath2)), encoding, defaultChar, false, (UInt8*)LStrBuf(**dest), numBytes, &numBytes);
							TerminateLStr(dest, numBytes);
						}
					}
				}
				CFRelease(cfpath2);
			}
		}
#elif Unix
		err = unix_convert_wtomb(src, srclen, dest, codePage, defaultChar, defaultCharWasUsed);
#endif
		return err;
	}
	return mgNoErr;
}
