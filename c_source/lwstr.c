/*
   lwstr.c -- widechar/shortchar unicode string support functions for LabVIEW integration

   Version 1.2, March 22th, 2022

   Copyright (C) 2017-2022 Rolf Kalbermatter

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
#include "utf.h"
#include "refnum.h"
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

MgErr LWPathResize(LWPathHandle *lwstr, size_t numChar)
{
	int32 len = (int32)(2 * sizeof(int32) + (numChar + 1) * sizeof(LWChar));
	if (*lwstr)
	{
		if (DSGetHandleSize((UHandle)*lwstr) < len)
			return DSSetHandleSize((UHandle)*lwstr, len);
	}
	else
	{
		*lwstr = (LWPathHandle)DSNewHandle(len);
		if (!*lwstr)
			return mFullErr;
		ClearMem((UPtr)(**lwstr), 8);
	}
	return mgNoErr;
}

MgErr LWPathDispose(LWPathHandle lwstr)
{
	if (lwstr)
		return DSDisposeHandle((UHandle)lwstr);
	return mgNoErr;
}

MgErr LWPathZeroTerminate(LWPathHandle pathName, LWChar *end)
{
	int32 newLen = end ? (end - LWPathBuf(pathName)) : LWPathLenGet(pathName);
	MgErr err = LWPathResize(&pathName, newLen);
	if (!err)
	{
		LWPathLenSet(pathName, newLen);
	}
	return err;
}

MgErr LWPathNormalize(LWPathHandle lwstr)
{
	int32 len = LWPathLenGet(lwstr);
	LWChar *a, *s, *t; 
	a = s = t = LWPathBuf(lwstr);

	if (*s && len)
	{
		LWChar *e = s + len;
#if Win32
		int32 unc = FALSE;
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
		if (len == 2 && t[-1] == ':')
		{
			return LWPathAppendSeparator(lwstr);
		}
#elif Unix || MacOSX
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
					/* .. backs up a directory but only if it is not at the root of the path */
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
#endif
	}
	return LWPathZeroTerminate(lwstr, t);
}

MgErr LWPathNCat(LWPathHandle *lwstr, int32 off, const LWChar *str, int32 strLen)
{
	MgErr err;
	if (strLen == -1)
	    strLen = lwslen(str);
	if (off == -1)
		off = LWPathLenGet(*lwstr);

	err = LWPathResize(lwstr, off + strLen);
	if (!err)
	{
		lwsncpy(LWPathBuf(*lwstr) + off, str, strLen);
		LWPathLenSet(*lwstr, off + strLen);
	}
	return err;
}

MgErr LWPathAppendSeparator(LWPathHandle lwstr)
{
	MgErr err = mgNoErr;
	int32 len = LWPathLenGet(lwstr);
	if (LWPathBuf(lwstr)[len - 1] != kNativePathSeperator)
	{
		err =  LWPathResize(&lwstr, len + 1);
		if (!err)
		{
			LWPathBuf(lwstr)[len] = kNativePathSeperator;
			LWPathLenSet(lwstr, ++len);
			LWPathBuf(lwstr)[len] = 0;
		}
	}
	return err;
}


static int32 StrUNCOffset(const char *ptr, int32 offset)
{
	int32 sep = 0;
	for (ptr += offset; *ptr; offset++)
	{
		if (*ptr++ == kNativePathSeperator)
		{
			sep++;
			offset++;
			if (sep >= 3)
				return offset;
			if (*ptr++ == kNativePathSeperator)
				return -1;
		}
	}
	return -1;  
}

LibAPI(int32) LStrRootPathLen(LStrHandle filePath, int32 offset, uInt8 *type)
{
	int32 len = LStrLenH(filePath);
	if (len)
	{
		const char *ptr = (const char*)LStrBuf(*filePath);

		if (offset < 0)
			offset = HasDOSDevicePrefix(ptr, len);
#if usesWinPath && !Pharlap
		if (len == offset || (offset == 8 && len <= 8))
		{
			if (type)
				*type = fAbsPath;		
			offset = 0;
		}
		else if ((len >= 3 + offset && ptr[offset] < 128 && isalpha(ptr[offset]) && ptr[offset + 1] == ':' && ptr[offset + 2] == kPathSeperator))
		{
			if (type)
				*type = fAbsPath;		
			offset += 3;
		}
		else if (len > 8 && offset == 8)
		{
			offset = StrUNCOffset(ptr, offset);
			if (type)
				*type = offset < 0 ? fNotAPath : fUNCPath;		
		}
		else if (len >= 3 + offset && ptr[0] == kPathSeperator && ptr[1] == kPathSeperator && ptr[2] < 128 && isalpha(ptr[2]))
		{
			offset = StrUNCOffset(ptr, offset + 2);
			if (type)
				*type = offset < 0 ? fNotAPath : fUNCPath;		
		}
#elif usesWinPath && Pharlap
		if (len >= 3 + offset && (ptr[offset] < 128 && isalpha(ptr[offset]) && ptr[offset + 1] == ':' && ptr[offset + 2] == kPathSeperator))
		{
			if (type)
				*type = fAbsPath;		
			offset += 3;
		}
		else if (len >= 3 + offset && ptr[offset] == kPathSeperator && ptr[offset + 1] == kPathSeperator && ptr[offset + 2] < 128 && isalpha(ptr[offset + 2]))
		{
			offset = StrUNCOffset(ptr, offset + 2);
			if (type)
				*type = offset < 0 ? fNotAPath : fUNCPath;		
		}
#else
		if (ptr[offset] == kPosixPathSeperator)
		{
			if (len >= 2 + offset && ptr[offset + 1] == kPosixPathSeperator)
			{
				offset = StrUNCOffset(ptr, offset + 2);
				if (type)
					*type = offset < 0 ? fNotAPath : fUNCPath;		
			}
			else
			{
				if (type)
					*type = fAbsPath;		
				offset += 1;
			}
		}
#endif
		else
		{
			if (type)
				*type = fRelPath;		
		}
	}
	else
	{
		/* len == 0 is root path and always absolute */
		if (type)
			*type = fAbsPath;		
	}
	return offset;
}

Bool32 LStrIsAPathOfType(LStrHandle pathName, int32 offset, uInt8 isType)
{
	uInt8 type = fNotAPath;
	int32 off = offset < 0 ? HasDOSDevicePrefix(LStrBufH(pathName), LStrLenH(pathName)) : offset;
	
	LStrRootPathLen(pathName, off, &type);
	return (type == isType);
}

Bool32 LStrIsAbsPath(LStrHandle pathName, int32 offset)
{
	uInt8 type = fNotAPath;
	int32 off = offset < 0 ? HasDOSDevicePrefix(LStrBufH(pathName), LStrLenH(pathName)) : offset;
	
	LStrRootPathLen(pathName, off, &type);
	return ((type == fAbsPath) || (type == fUNCPath));
}

static int32 ParentPathInternal(uChar *string, int32 rootLen, int32 end)
{
	while (end > rootLen && string[--end] != kNativePathSeperator);
	return end;
}

LibAPI(MgErr) LStrParentPath(LStrHandle filePath, LStrHandle *fileName, LVBoolean *empty)
{
	MgErr err = mgNoErr;
	uInt8 srcType = fNotAPath;
	uChar *srcPtr = LStrBufH(filePath);
	int32 dstLen, srcLen = LStrLenH(filePath),
		  srcOff = HasDOSDevicePrefix(srcPtr, srcLen),
	      srcRoot = LStrRootPathLen(filePath, srcOff, &srcType);

	if (LStrLenH(*fileName))
		LStrLen(**fileName) = 0;

	if (srcLen <= srcOff || srcType == fNotAPath)
		return mgArgErr;

	if (srcPtr[srcLen - 1] == kNativePathSeperator)
		LStrLen(*filePath) = --srcLen;

/*                         parent                 name             srcLen  srcOff  srcRoot  R<L  srcRoot+1  empty  srcLen+1  srcOff+1  destLen
C:\data\test               C:\data                test               12       0       3      T      7         F       7         8         4
C:\data\                   C:\                    data              7(8)      0       3      T      3         F       3         3         4
C:\data                    C:\                    data                7       0       3      T      3         F       3         3         4
C:\                        <empty>                c                   3       0       3    F                T       0       (0)         1       
\\?\C:\data                \\?\C:\                data               11       4       7      T      7         F       7         7         4
\\?\C:\                    <empty>                c                 6(7)      4       7    F                T       0       (4)         1          
\\server\share\data        \\server\share         data               19       0      14      T     14         F      14        15         4       
\\server\share             <empty>                \\server\share     14       0      14    F                T       0       (0)        14          
\\?\UNC\server\share\data  \\?\UNC\server\share   data               25       8      20      T     20         F      10        21         4
\\?\UNC\server\share\      <empty>                \\server\share  20(21)      8      20    F                T       0        6         14
data\test                  data                   test                9       0       0      T      4         F       4         5         4
data                       <empty>                data                4       0       0      T      0         T       0         0         4

/data/test/                /data                  test            10(11)      0       1      T      5         F       5         6         4
/data/test                 /data                  test               10       0       1      T      5         F       5         6         4
/data                      <empty>                data                5       0       1      T      0         T       0         1         4
//server/share/data        //server/share         data               19       0      14      T     14         F      14        15         4
//server/share             <empty>                //server/share     14       0      14    F                T       0       (0)        14       
data/test                  data                   test                9       0       0      T      4         F       4         5         4
data                       <empty>                data                4       0       0      T      0         T       0         0         4

Volume:Data:Test
:Data:Test
*/
	if (srcRoot < srcLen)
	{
		srcRoot = ParentPathInternal(srcPtr, srcRoot, srcLen); 
		if (empty)
			*empty = (LVBoolean)(srcRoot <= srcOff);
		srcOff = srcRoot;
		if (srcPtr[srcOff] == kNativePathSeperator)
			srcOff++;
		dstLen = srcLen - srcOff;
		srcLen = srcRoot;
	}
	else
	{
		if (empty)
			*empty = LV_TRUE;
		if (srcType == fUNCPath && srcOff == 8)
			srcOff = 6;
		dstLen = (srcType == fAbsPath) ? 1 : (srcLen - srcOff);
		srcLen = 0;
	}
	err = NumericArrayResize(uB, 1, (UHandle*)fileName, dstLen);
	if (!err)
	{
		MoveBlock(srcPtr + srcOff, LStrBuf(**fileName), dstLen);
#if usesWinPath && !Pharlap
		if (!srcLen && srcOff == 6)
			LStrBuf(**fileName)[0] = kNativePathSeperator;
#endif
		LStrLen(**fileName) = dstLen;
	}
	LStrLen(*filePath) = srcLen;
	return err;
}

LibAPI(MgErr) LStrAppendPath(LStrHandle *filePath, LStrHandle relPath)
{
	MgErr err = mgNoErr;
	uChar *srcPtr = LStrBufH(*filePath), *relPtr = LStrBufH(relPath);
	uInt8 srcType = fNotAPath, relType = fNotAPath;
	int32 srcLen = LStrLenH(*filePath),
		  srcOff = HasDOSDevicePrefix(srcPtr, srcLen),
		  srcRoot = LStrRootPathLen(*filePath, srcOff, &srcType),
		  relLen = LStrLenH(relPath),
		  relOff = HasDOSDevicePrefix(relPtr, relLen),
		  relRoot = LStrRootPathLen(relPath, relOff, &relType);

	/* If empty relative path, there is nothing to append */
	if (relLen <= relOff)
		return mgNoErr;

	/* If start path is not empty and relative path is not relative, we have a problem */
	if (srcLen > srcOff && relType != fRelPath)
		return mgArgErr;

	/* Remove trailing path seperator */
	if (srcLen > srcRoot && srcPtr[srcLen - 1] == kNativePathSeperator)
		srcLen--;

	if (relLen > relRoot && relPtr[relLen - 1] == kNativePathSeperator)
		relLen--;

	err = NumericArrayResize(uB, 1, (UHandle*)filePath, srcLen + relLen + 1);
	if (!err)
	{
		srcPtr = LStrBufH(*filePath);
		
		if (relPtr[0] != kNativePathSeperator && srcLen > srcRoot)
			srcPtr[srcLen++] = kNativePathSeperator;
		MoveBlock(relPtr, srcPtr + srcLen, relLen);
		LStrLen(**filePath) = srcLen + relLen;
	}
	return err;
}

LibAPI(MgErr) LRefParentPath(LWPathHandle *filePath, LStrHandle *fileName, LVBoolean *empty)
{
	MgErr err = mgNoErr;
	LWChar *srcPtr = LWPathBuf(*filePath);
	uInt8 srcType = fNotAPath;
	int32 srcLen = LWPathLenGet(*filePath),
		  srcOff = HasDOSDevicePrefix(srcPtr, srcLen),
	      srcRoot = LWPathRootLen(*filePath, srcOff, &srcType);

	if (LStrLenH(*fileName))
		LStrLen(**fileName) = 0;

	if (srcLen <= srcOff || srcType == fNotAPath)
		return mgArgErr;

	if (srcPtr[srcLen - 1] == kNativePathSeperator)
		LWPathLenSet(*filePath, --srcLen);

	if (srcRoot < srcLen)
	{
		srcRoot = LWPathParentInternal(*filePath, srcRoot, srcLen); 
		if (empty)
			*empty = (LVBoolean)(srcRoot <= srcOff);
		srcOff = srcRoot;
		if (srcPtr[srcOff] == kNativePathSeperator)
			srcOff++;
		srcLen = srcRoot;
	}
	else
	{
		if (empty)
			*empty = LV_TRUE;
		if (srcType == fUNCPath && srcOff == 8)
			srcOff = 6;
		if (srcType == fAbsPath)
			LWPathLenSet(*filePath, srcOff + 1);
		srcLen = 0;
	}
	err = LStrFromLWPath(fileName, CP_UTF8, *filePath, srcOff, LV_TRUE);
	if (!err)
	{
#if usesWinPath && !Pharlap
		if (!srcLen && srcOff == 6)
			LStrBuf(**fileName)[0] = kNativePathSeperator;
#endif
		LWPathLenSet(*filePath, srcLen);
	}
	return err;
}

LibAPI(MgErr) LRefAppendPath(LWPathHandle *filePath, LStrHandle relString)
{
	MgErr err = mgNoErr;
	LWPathHandle relPath = NULL;
	uInt8 relType = fNotAPath;
	int32 srcLen = LWPathLenGet(*filePath),
		  srcOff = HasDOSDevicePrefix(LWPathBuf(*filePath), srcLen),
		  relLen = LStrLen(*relString),
		  relOff = HasDOSDevicePrefix(LStrBuf(*relString), relLen);

	/* If empty relative path, there is nothing to append */
	if (relLen <= relOff)
		return mgNoErr;

	LStrRootPathLen(relString, relOff, &relType);

	/* If start path is not empty and relative path is not relative, we have a problem */
	if (srcLen > srcOff && relType != fRelPath)
		return mgArgErr;

	err = LStrToLWPath(relString, CP_UTF8, &relPath, kDefaultPath, 0);
	if (!err)
	{
		err = LWPathAppend(*filePath, srcLen, filePath, relPath);
		LWPathDispose(relPath);
	}
	return err;
}

uInt16 PathDepth(uChar *string, int32 offset, int32 rootLen, int32 end)
{
	uInt16 i = 0;
	while (end && end > rootLen)
	{
		end = ParentPathInternal(string, rootLen, end);
		i++;
	}
	return (i + (uInt16)(end > offset));
}

/* Return values:
   -2: invalid path
   -1: no path segment left
    0: empty path, canonical root
   1..: next segment offset
 */
static int32 LStrNextPathSegment(LStrHandle filePath, int32 start, int32 end)
{
	if (end < 0)
		end = LStrLenH(filePath);

	if (LStrBufH(filePath)[end - 1] == kNativePathSeperator)
		end--;

	if (start <= 0)
	{
		int32 rootLen = LStrRootPathLen(filePath, -1, NULL);
		if (rootLen == -2)
			return -2;
		if (rootLen == -1)
			start = 0;
		else
			return rootLen;
	}

	if (start >= end)
		return -1;

	for (;start < end && LStrBufH(filePath)[start] != kNativePathSeperator; start++);

	return start;
}

static int32 LWStrUNCOffset(LWPathHandle lwstr, int32 offset)
{
	int32 sep = 0, len = LWPathLenGet(lwstr);
	const LWChar *ptr = LWPathBuf(lwstr);

	for (ptr += offset; offset < len; offset++)
	{
		if (*ptr++ == kNativePathSeperator)
		{
			sep++;
			offset++;
			if (sep >= 3)
				return offset;
			if (*ptr++ == kNativePathSeperator)
				return -1;
		}
	}
	return -2;  
}

/* Returns:
   >= 0 for a valid absolute path, with 0 being the canonical root path and > 0 indicating the length of the first path element
   -1 for a relative path
   -2 when there is an invalid path
 */
int32 LWPathRootLen(LWPathHandle lwstr, int32 offset, uInt8 *type)
{
	int32 len = LWPathLenGet(lwstr);
	if (len)
	{
		LWChar *ptr = LWPathBuf(lwstr);

		if (offset < 0)
			offset = HasDOSDevicePrefix(ptr, len);
#if usesWinPath && !Pharlap
		if (len == offset || (offset == 8 && len <= 8))
		{
			if (type)
				*type = fAbsPath;		
			offset = 0;
		}
		else if ((len >= 3 + offset && ptr[offset] < 128 && isalpha(ptr[offset]) && ptr[offset + 1] == ':' && ptr[offset + 2] == kPathSeperator))
		{
			if (type)
				*type = fAbsPath;		
			offset += 3;
		}
		else if (len > 8 && offset == 8)
		{
			offset = LWStrUNCOffset(lwstr, offset);
			if (type)
				*type = (offset < 0) ? fNotAPath : fUNCPath;		
		}
		else if (len >= 3 + offset && ptr[offset] == kPathSeperator && ptr[offset + 1] == kPathSeperator && ptr[offset + 2] < 128 && isalpha(ptr[2]))
		{
			offset = LWStrUNCOffset(lwstr, offset + 2);
			if (type)
				*type = (offset < 0) ? fNotAPath : fUNCPath;		
		}
#elif usesWinPath && Pharlap 
		if (len >= 3 + offset && (ptr[offset] < 128 && isalpha(ptr[offset]) && ptr[offset + 1] == ':' && ptr[offset + 2] == kPathSeperator))
		{
			if (type)
				*type = fAbsPath;		
			offset += 3;
		}
		if (len >= 3 + offset && ptr[offset] == kPathSeperator && ptr[offset + 1] == kPathSeperator && ptr[offset + 2] < 128 && isalpha(ptr[2]))
		{
			offset = LWStrUNCOffset(lwstr, offset + 2);
			if (type)
				*type = offset < 0 ? fNotAPath : fUNCPath;		
		}
#else
		if (len >= 1 + offset && ptr[offset] == kPosixPathSeperator)
		{
			if (len >= 2 + offset && ptr[offset + 1] == kPosixPathSeperator)
			{
				offset = LWStrUNCOffset(lwstr, offset + 2);
				if (type)
					*type = offset < 0 ? fNotAPath : fUNCPath;		
			}
			else
			{
				if (type)
					*type = fAbsPath;		
				offset += 1;
			}
		}
#endif
		else
		{
			if (type)
				*type = fRelPath;
		}
	}
	else
	{
		if (type)
			*type = fAbsPath;		
	}
	return offset;
}

Bool32 LWPathIsOfType(LWPathHandle pathName, int32 offset, uInt8 isType)
{
	uInt8 type = fNotAPath;

	if (offset < 0)
		offset = HasDOSDevicePrefix(LWPathBuf(pathName), LWPathLenGet(pathName));
	
	LWPathRootLen(pathName, offset, &type);
	return (type == isType) || (isType == fAbsPath && type == fUNCPath);
}

int32 LWPathParentInternal(LWPathHandle lwstr, int32 rootLen, int32 end)
{
	if (end < 0)
		end = LWPathLenGet(lwstr);
	while (end && end > rootLen && LWPathBuf(lwstr)[--end] != kNativePathSeperator);
	return end;
}

int32 LWPathParent(LWPathHandle lwstr, int32 end)
{
	int32 rootLen = LWPathRootLen(lwstr, -1, NULL);
	if (rootLen >= -1)
		return LWPathParentInternal(lwstr, rootLen, end);
	return -1;
}

int32 LWPathDepth(LWPathHandle lwstr, int32 end)
{
	int32 i = 0, 
		  offset = HasDOSDevicePrefix(LWPathBuf(lwstr), LWPathLenGet(lwstr)),
		  rootLen = LWPathRootLen(lwstr, offset, NULL);
	if (end < 0)
		end = LWPathLenGet(lwstr);
	while (end && end > rootLen)
	{
		end = LWPathParentInternal(lwstr, rootLen, end);
		i++;
	}
	return end > offset ? i + 1 : i;
}

MgErr LWPathAppend(LWPathHandle srcPath, int32 end, LWPathHandle *newPath, LWPathHandle relPath)
{
	MgErr err = mgNoErr;
	LWPathHandle tmpPath = newPath ? *newPath : srcPath;
	uInt8 relType = fNotAPath;
	LWChar *srcPtr = LWPathBuf(srcPath), *relPtr = LWPathBuf(relPath);
	int32 srcLen = end < 0 ? LWPathLenGet(srcPath) : end,
		  srcOff = HasDOSDevicePrefix(srcPtr, srcLen),
		  relLen = LWPathLenGet(relPath),
		  relOff = HasDOSDevicePrefix(relPtr, relLen),
		  relRoot = LWPathRootLen(relPath, relOff, &relType);

	/* If empty relative path, there is nothing to append */
	if (relLen <= relOff)
		return mgNoErr;

	/* If start path is not empty and relative path is not relative, we have a problem */
	if (srcLen > srcOff && relType != fRelPath)
		return mgArgErr;
	
	/* Remove trailing path seperator */
	if (srcLen > LWPathRootLen(srcPath, srcOff, NULL) && srcPtr[srcLen - 1] == kNativePathSeperator)
		srcLen--;

	if (relLen > relRoot && relPtr[relLen - 1] == kNativePathSeperator)
		relLen--;

	if (srcPath != tmpPath)
	{
		err = LWPathNCat(&tmpPath, 0, srcPtr, srcLen);
	}
	if (!err)
	{
		if (relPtr[0] != kNativePathSeperator)
			err = LWPathAppendSeparator(tmpPath);
		if (!err)
			err = LWPathNCat(&tmpPath, -1, relPtr, relLen);
		if (!err && newPath && *newPath != tmpPath)
		{
			*newPath = tmpPath;
		}
	}
	return err;
}

/* Return values:
   -2: invalid path
   -1: no path segment left
    0: empty path, canonical root
   1..: next segment offset
 */
int32 LWPathNextElement(LWPathHandle lwstr, int32 start, int32 end)
{
	LWChar *ptr = LWPathBuf(lwstr);

	if (start <= 0)
	{
		uInt8 type = fNotAPath;
		int32 rootLen = LWPathRootLen(lwstr, -1, &type);
		if (type == fNotAPath)
			return -2;
		if (type == fRelPath)
			start = 0;
		else
			return rootLen;
	}

	if (end < 0)
		end = LWPathLenGet(lwstr);

	if (ptr[start] == kNativePathSeperator)
		start++;

	if (start >= end)
		return -1;

	if (ptr[end - 1] == kNativePathSeperator)
		end--;

	for (;start < end && ptr[start] && ptr[start] != kNativePathSeperator; start++);

	return start;
}

MgErr LWStrRelativePath(LWPathHandle startPath, LWPathHandle endPath, LWPathHandle *relPath)
{
	int32 startCount, endCount, level, notchesUp;
	int16 length, totalLength, remainingEndCount, i, err;
	uInt8 startType = fNotAPath, endType = fNotAPath;
	LWChar *startp = LWPathBuf(startPath),
		   *endp = LWPathBuf(endPath);
	int32 startLen = LWPathLenGet(startPath),
		  startOff = HasDOSDevicePrefix(startp, startLen),
		  startRoot = LWPathRootLen(startPath, startOff, &startType),
		  endLen = LWPathLenGet(endPath),
		  endOff = HasDOSDevicePrefix(endp, endLen),
		  endRoot = LWPathRootLen(endPath, endOff, &endType); 

	if (startType != fAbsPath && endType != fAbsPath && relPath)
		return mgArgErr;

	startCount = LWPathDepth(startPath, -1);
	endCount = LWPathDepth(endPath, -1);
	for (level = startCount; level > 0; level--)
	{

	}
	return 0;
}

/*
TH_REENTRANT MgErr FRelPath(Path start, Path end, Path relPath)
	{
	int16 startCount, endCount, level, notchesUp;
	int16 length, totalLength, remainingEndCount, i, err;
	uChar *startp, *endp;

	if (!(FIsAbsPath(start) && FIsAbsPath(end) && relPath))
		return mgArgErr;
	startCount = PathCnt(start);
	endCount = PathCnt(end);
	startp = PathBuf(start);
	endp = PathBuf(end);
	for(level=startCount; level>0; level--) {
		if(!FileNameEqual(startp, endp))
			break;
		startp += PStrLen(startp) + 1;
		endp += PStrLen(endp) + 1;
		}
	notchesUp = level;
	length = 0;
	remainingEndCount = endCount-(startCount-level);
	for(level = remainingEndCount; level>0; level--) {
		length += *endp+1;
		endp += *endp+1;
		}
	totalLength = length + notchesUp + sizeof(PathType(start)) + sizeof(PathCnt(start));
	if(totalLength > AZGetHandleSize(relPath)) {
		endp -= (int32)PathBuf(end);
		if(err= AZSetHandleSize(relPath, totalLength))
			return err;
		endp += (int32)PathBuf(end);
		MoveBlock(endp-length, PathBuf(relPath)+notchesUp, length);
		}
	else {
		MoveBlock(endp-length, PathBuf(relPath)+notchesUp, length);
		AZSetHandleSize(relPath, totalLength);
		}
	PathType(relPath) = fRelPath;
	PathCnt(relPath) = remainingEndCount+notchesUp;
	for(i=0, endp=PathBuf(relPath); i<notchesUp; i++)
		*endp++ = 0;
	return 0;
	}
*/

static FMFileType LWStrFileTypeFromExt(LWPathHandle lwstr)
{
	int32 len = LWPathLenGet(lwstr), k = 1;
    LWChar *ptr = LWPathBuf(lwstr);

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

MgErr LWPathGetFileTypeAndCreator(LWPathHandle lwstr, FMFileType *fType, FMFileType *fCreator)
{
	/* Try to determine LabVIEW file types based on file ending? */
	uInt32 creator = kUnknownCreator,
		   type = LWStrFileTypeFromExt(lwstr);
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
	    if (!getattrlist(SStrBuf(lwstr), &alist, &finfo, sizeof(fileinfobuf), FSOPT_NOFOLLOW))
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

MgErr LStrToLWPath(LStrHandle string, uInt32 codePage, LWPathHandle *lwstr, uInt32 flags, int32 reserve)
{
	MgErr err = noErr;
	uChar *srcPtr = LStrBufH(string);
	uInt8 srcType = fNotAPath;
	int32 dstLen = 0, srcLen = LStrLenH(string),
		  srcOff = HasDOSDevicePrefix(srcPtr, srcLen),
		  srcRoot = LStrRootPathLen(string, srcOff, &srcType);

#if Unix || MacOSX || Pharlap
	LStrHandle temp = NULL;
	if (srcLen)
	{
#if usesHFSPath
		if (!(flags & kLeaveHFSPath))
			err = ConvertToPosixPath(string, codePage, &temp, CP_UTF8, '?', NULL, FALSE);
		else
#endif
			if (codePage == CP_UTF8)
			{
				temp = string;
			}
			else
			{
				err = ConvertLString(string, codePage, &temp, CP_UTF8, '?', NULL);
				if (err)
					return err;
			}
		dstLen = LStrLenH(temp);
	}
#if Unix || MacOSX
	else
	{
		dstLen = 1;
	}
#endif
	err = LWPathResize(lwstr, dstLen + reserve);
	if (!err)
	{
		if (LStrLenH(temp))
		{
			MoveBlock((UPtr)LStrBufH(temp), (UPtr)LWPathBuf(*lwstr), dstLen);
		}
#if Unix || MacOSX
		else
		{
			LWPathBuf(*lwstr)[0] = flags & kCvtHFSToPosix ? kNativePathSeperator : kPathSeperator;
			dstLen = 1;
		}
#endif
	}
	if (temp && temp != string)
		DSDisposeHandle((UHandle)temp);
#else /* Windows and not Pharlap */
	int32 rlen = 0;
	
	if (!srcOff)
	{
		/* Create DOS Device paths for absolute paths that aren't that yet */ 
		if (!srcLen || (srcLen >= 3) && srcPtr[0] < 128 && isalpha(srcPtr[0]) && srcPtr[1] == ':' && srcPtr[2] == kPathSeperator)
		{
			/* Absolute path with drive letter */
			rlen = 4;
		}
		else if (srcPtr[0] == kPathSeperator && srcPtr[1] == kPathSeperator && srcPtr[2] < 128 && isalpha(srcPtr[2]))
		{
			/* Absolute UNC path */
			rlen = 7;
		}
	}
	dstLen = MultiByteToWideChar(codePage, 0, (LPCSTR)srcPtr, srcLen, NULL, 0);
	if (dstLen <= 0)
		return mgArgErr;
	
	err = LWPathResize(lwstr, rlen + dstLen + reserve);
	if (err)
		return err;

	switch (rlen)
	{
		case 4:
			LWPathNCat(lwstr, 0, L"\\\\?\\", rlen);
			break;
		case 7:
			LWPathNCat(lwstr, 0, L"\\\\?\\UNC", rlen);
			break;
	}
	dstLen = MultiByteToWideChar(codePage, 0, (LPCSTR)srcPtr + (rlen == 7), srcLen - (rlen == 7), LWPathBuf(*lwstr) + rlen, dstLen + reserve);
	if (dstLen <= 0)
		return mgArgErr;
	dstLen += rlen;
#endif
	LWPathLenSet(*lwstr, dstLen);
	LWPathTypeSet(*lwstr, srcType);
	LWPathCntSet(*lwstr, PathDepth(srcPtr, srcOff, srcRoot, srcLen));
	return LWPathNormalize(*lwstr);
}

/* Windows: path is an UTF8 encoded path string and lwstr is filled with a Windows UTF16LE string from the path
   MacOSX, Unix, VxWorks, Pharlap: path is an UTF8 encoded path string and lwstr is filled with a local encoded SBC or
   MBC string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
LibAPI(MgErr) UPathToLWPath(LStrHandle path, LWPathHandle *lwstr, uInt32 flags)
{
	return LStrToLWPath(path, CP_UTF8, lwstr, flags, 0);
}

/* Windows: path is a LabVIEW path and lwstr is filled with a Windows UTF16LE string from this path
   MacOSX, Unix, VxWorks, Pharlap: path is a LabVIEW path and lwstr is filled with a local encoded SBC or MBC
   string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
LibAPI(MgErr) LPathToLWPath(Path path, LWPathHandle *lwstr, uInt32 flags, int32 reserve)
{
	LStrPtr lstr;
    int32 bufLen = -1;
    MgErr err = FPathToText(path, (LStrPtr)&bufLen);
    if (!err)
    {
		bufLen += 1 + reserve;
        lstr = (LStrPtr)DSNewPClr(sizeof(int32) + bufLen + 1);
        if (!lstr)
            return mFullErr;
        LStrLen(lstr) = bufLen;
        err = FPathToText(path, lstr);
        if (!err)
			err = LStrToLWPath(&lstr, CP_ACP, lwstr, flags, reserve);

		DSDisposePtr((UPtr)lstr);
    }
    return err;
}

MgErr LStrFromLWPath(LStrHandle *pathName, uInt32 codePage, LWPathHandle lwstr, int32 offset, uInt32 flags)
{
	MgErr err = noErr;
	int32 len = LWPathLenGet(lwstr);
	if (len)
	{
		LWChar *ptr = LWPathBuf(lwstr);
#if Unix || MacOSX || Pharlap
#if usesHFSPath
		if (!(flags & kLeaveHFSPath))
			err = ConvertFromPosixPath(ptr + offset, len - offset, CP_UTF8, pathName, codePage, '?', FALSE);
		else
#endif
			err = ConvertCString(ptr + offset, len - offset, CP_UTF8, pathName, codePage, '?', NULL);
#else /* Windows and not Pharlap */
		if (!offset && !(flags & kCvtKeepDOSDevice))
		{
			offset = HasDOSDevicePrefix(ptr, len);
			if (offset == 8)
				offset = 6;
		}
		err = WideCStrToMultiByte(ptr + offset, len - offset, pathName, codePage, '?', NULL);
		if (!err && offset == 6 && LStrBuf(**pathName)[1] == kPathSeperator)
		{
			LStrBuf(**pathName)[0] = kPathSeperator;
		}
#endif
	}
	else if (pathName && *pathName)
	{
		LStrLen(**pathName) = 0;
	}
	return err;
}

LibAPI(MgErr) UPathFromLWPath(LStrHandle *pathName, LWPathHandle *lwstr, uInt32 flags)
{
	return LStrFromLWPath(pathName, CP_UTF8, *lwstr, 0, flags);
}

LibAPI(MgErr) LPathFromLWPath(Path *pathName, LWPathHandle *lwstr, uInt32 flags)
{
	LStrHandle dest = NULL;
	MgErr err = LStrFromLWPath(&dest, CP_ACP, *lwstr, 0, flags);
	if (!err)
	{
		err = FTextToPath(LStrBuf(*dest), LStrLen(*dest), pathName);
		DSDisposeHandle((UHandle)dest);
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

LibAPI(MgErr) ConvertLString(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed)
{
	return ConvertCString(LStrBuf(*src), LStrLen(*src), srccp, dest, destcp, defaultChar, defUsed);
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
		UPtr buf = LStrBufH(*dest);
		int32 len = LStrLenH(*dest),
			  offset = HasDOSDevicePrefix(buf, len);
		if (offset == 8)
		{
			offset = 7;
			*buf++ = kPosixPathSeperator;
			len--;
		}
		else if (buf[offset + 1] == ':' && buf[offset + 2] == kPathSeperator)
		{
			buf[1] = buf[offset];
			buf[0] = kPosixPathSeperator;
			buf += 2;
			len -= 2;
		}
		if (offset)
		{
			UPtr ptr = buf + offset;
			for (len -= offset; len; len--, ptr++, buf++)
			{
				if (*ptr == kPathSeperator)
				{	
					*buf = kPosixPathSeperator;
				}
				else
				{
					*buf = *ptr;
				}
			}
			LStrLen(**dest) -= offset;
		}
		else
		{
			for (;len; len--, buf++)
			{
				if (*buf == kPathSeperator)
				{	
					*buf = kPosixPathSeperator;
				}
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
			char *inbuf = (char*)src, 
				 *outbuf = (char*)WStrBuf(*dest);
			size_t retval, inleft = len, outleft = len * sizeof(wchar_t);
			retval = iconv(cd, &inbuf, &inleft, &outbuf, &outleft);
			if (retval == (size_t)-1)
				err = UnixToLVFileErr();
			else
			    WStrLenSet(*dest, (len * sizeof(wchar_t) - outleft) / sizeof(uInt16));
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
				err = utf8towchar(src, sLen, WStrBuf(*dest), NULL, wLen + 1);
			}
			else
			{
				wchar_t *wPtr = WStrBuf(*dest);
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
				{
					wchar_t *ptr = WStrBuf(*dest);
					ptr[wLen] = 0;
				}
			}
		}
	}
	if (!err && *dest)
	    WStrLenSet(*dest, wLen * sizeof(wchar_t) / sizeof(uInt16)); 
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
		WStrLenSet(*dest, 0);
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
				WStrLenSet(*dest, MultiByteToWideChar(codePage, 0, (LPCSTR)src, srclen, WStrBuf(*dest), numChars));	
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
	return WideCStrToMultiByte(WStrBuf(src), WStrLen(src), dest, codePage, defaultChar, defaultCharWasUsed);
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
			BOOL defUsed = FALSE;
			BOOL utfCp = ((codePage == CP_UTF8) || (codePage == CP_UTF7));
			err = NumericArrayResize(uB, 1, (UHandle*)dest, numBytes + 1);
			if (!err)
			{
				numBytes = WideCharToMultiByte(codePage, 0, src, srclen, (LPSTR)LStrBuf(**dest), numBytes, utfCp ? NULL : &defaultChar, utfCp ? NULL : &defUsed);
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
