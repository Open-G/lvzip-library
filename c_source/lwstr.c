/*
   lwstr.c -- widechar/shortchar unicode string support functions for LabVIEW integration

   Copyright (C) 2017-2024 Rolf Kalbermatter

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
 #include <stdlib.h>
 #include <string.h>
 #include <locale.h>
 #ifdef HAVE_ICONV
  #include <iconv.h>
 #endif
#endif

#if usesWinPath
#define IsSeperator(c) (c == kPathSeperator || c == kPosixPathSeperator)
#else
#define IsSeperator(c) (c == kPathSeperator)
#endif
#define IsPosixSeperator(c) (c == kPosixPathSeperator)

static MgErr WideCStrToMultiByteBuf(const wchar_t *src, int32 srclen, UPtr ptr, int32 *length, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed);
static MgErr ConvertCStringBuf(ConstCStr src, int32 srclen, uInt32 srccp, UPtr ptr, int32 *length, uInt32 destcp, char defaultChar, LVBoolean *defUsed);

LWChar gNotAPath[] = LW("<Not A Path>");

static int32 LWPtrCompare(const LWChar *s1, const LWChar *s2, int32 len)
{
	register LWChar u1, u2;
	for (;len > 0; len--)
	{
		u1 = *s1++;
		u2 = *s2++;
		if (u1 != u2)
			return u1 - u2;
		if (u1 == '\0')
			break;
	}
	return 0;
}

static int32 LWPtrUNCOffset(const LWChar *ptr, int32 offset, int32 len)
{
	int32 sep = 0;
	for (ptr += offset; *ptr && offset < len; *ptr++, offset++)
	{
		if (IsSeperator(*ptr))
		{
			while (IsSeperator(ptr[1]))
			{
				ptr++;
				offset++;
			}
			if (++sep >= 2)
				return ++offset;
		}
	}
	if (sep == 1 && (!*ptr || offset == len))
	{
		return offset;
	}
	return -1;  
}

/* Returns:
   >= 0 for a valid path, with 0 being the canonical root path and > 0 indicating the length of the first path element
 */
DebugAPI(int32) LWPtrRootLen(const LWChar *ptr, int32 len, int32 offset, uInt8 *type)
{
	if (offset < 0)
		offset = HasNTFSDevicePrefix(ptr, len);

	if (len > offset)
	{
#if usesWinPath
		if ((len >= 2 + offset && ptr[offset] < 128 && isalpha(ptr[offset]) && ptr[offset + 1] == ':'))
		{
			if (type)
				*type = fAbsPath;		
			offset += 2 + (IsSeperator(ptr[offset + 2]) ? 1 : 0);
		}
		else if (len >= 3 + offset && IsSeperator(ptr[offset]) && IsSeperator(ptr[offset + 1]) && ptr[offset + 2] < 128 && isalpha(ptr[offset + 2]))
		{
			offset = LWPtrUNCOffset(ptr, offset + 2, len);
			if (type)
				*type = offset < 0 ? fNotAPath : fUNCPath;		
		}
 #if !Pharlap
		else if (offset == 8)
		{
			offset = LWPtrUNCOffset(ptr, offset, len);
			if (type)
				*type = offset < 0 ? fNotAPath : fUNCPath;		
		}
 #endif
#else
		if (IsPosixSeperator(ptr[offset]))
		{
			if (len >= 2 + offset && IsSeperator(ptr[offset + 1]))
			{
				offset = LWPtrUNCOffset(ptr, offset + 2, len);
				if (type)
					*type = offset < 0 ? fNotAPath : fUNCPath;		
			}
			else
			{
				offset++;
				if (type)
					*type = fAbsPath;		
			}
		}
#endif
		else
		{
			while (ptr[offset] == '.' && IsSeperator(ptr[offset + 1]))
			{
				offset += 2;
			}
			if (type)
				*type = fRelPath;
		}
	}
	else
	{
		/* len == offset is root path and always absolute */
		if (type)
			*type = fAbsPath;		
	}
	return offset;
}

#if Win32
static int32 WStrUNCOffset(const wchar_t *ptr, int32 offset, int32 len)
{
	int32 sep = 0;
	for (ptr += offset; *ptr && offset < len; *ptr++, offset++)
	{
		if (IsSeperator(*ptr))
		{
			while (IsSeperator(ptr[1]))
			{
				ptr++;
				offset++;
			}
			if (++sep >= 2)
				return ++offset;
		}
	}
	if (sep == 1 && (!*ptr || offset == len))
	{
		return offset;
	}
	return -1;  
}

DebugAPI(int32) WStrRootLen(const wchar_t *ptr, int32 len, int32 offset, uInt8 *type)
{
	if (offset < 0)
		offset = HasNTFSDevicePrefix(ptr, len);

	if (len > offset)
	{
#if usesWinPath
		if ((len >= 2 + offset && ptr[offset] < 128 && isalpha(ptr[offset]) && ptr[offset + 1] == ':'))
		{
			if (type)
				*type = fAbsPath;		
			offset += 2 + (IsSeperator(ptr[offset + 2]) ? 1 : 0);
		}
		else if (len >= 3 + offset && IsSeperator(ptr[offset]) && IsSeperator(ptr[offset + 1]) && ptr[offset + 2] < 128 && isalpha(ptr[offset + 2]))
		{
			offset = WStrUNCOffset(ptr, offset + 2, len);
			if (type)
				*type = offset < 0 ? fNotAPath : fUNCPath;		
		}
 #if !Pharlap
		else if (offset == 8)
		{
			offset = WStrUNCOffset(ptr, offset, len);
			if (type)
				*type = offset < 0 ? fNotAPath : fUNCPath;		
		}
 #endif
#else
		if (IsPosixSeperator(ptr[offset]))
		{
			if (len >= 2 + offset && IsSeperator(ptr[offset + 1]))
			{
				offset = WStrUNCOffset(ptr, offset + 2, len);
				if (type)
					*type = offset < 0 ? fNotAPath : fUNCPath;		
			}
			else
			{
				offset++;
				if (type)
					*type = fAbsPath;		
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
		/* len == offset is root path and always absolute */
		if (type)
			*type = fAbsPath;		
	}
	return offset;
}
#endif

/* Return values:
   -2: invalid path
   -1: no path segment left
    0: empty path, canonical root
   1..: next segment offset
 */
int32 LWPtrNextElement(const LWChar *ptr, int32 len, int32 off)
{
	if (off <= 0)
	{
		uInt8 type = fNotAPath;

		off = LWPtrRootLen(ptr, len, -1, &type);
		if (type == fNotAPath)
			return -2;
		if (type != fRelPath)
			return off;
	}

	if (IsSeperator(ptr[off]))
		off++;

	if (off >= len)
		return -1;

	if (IsSeperator(ptr[len - 1]))
		len--;

	for (;off < len && ptr[off] && !IsSeperator(ptr[off]); off++);

	return off;
}

int32 LWPtrUpMatches(const LWChar *ptr, int32 len, int32 off)
{
	while ((off + 2) <= len && ptr[off] == '.' && ptr[off + 1] == '.' && ((off + 2) == len || IsSeperator(ptr[off + 2])))
	{
		off += 3;
	}
	return off <= len ? off : len;
}

int32 LWPtrParent(const LWChar *ptr, int32 len, int32 rootLen)
{
	if (rootLen < 0)
		rootLen = LWPtrRootLen(ptr, len, -1, NULL);
	while (--len > rootLen && !IsSeperator(ptr[len]));
	return len;
}

int32 LWPtrDepth(const LWChar *ptr, int32 len, int32 offset, int32 rootLen)
{
	int32 i = 0;

	while (len && len > rootLen)
	{
		len = LWPtrParent(ptr, len, rootLen);
		i++;
	}
#if Win32
	return (len > offset) ? i + 1 : i;
#else
	return (len > (offset + 1)) ? i + 1 : i;
#endif
}

Bool32 LWPtrIsOfType(const LWChar *ptr, int32 len, int32 offset, uInt8 isType)
{
	uInt8 type = fNotAPath;
	LWPtrRootLen(ptr, len, offset, &type);
	return (type == isType);
}

#if DEBUG
int32 LWPathLenSet(LWPathHandle h, int32 numChar)
{
	int32 len = LWPathHdlSzNumChr(numChar);
	if (!h || !*h || DSGetHandleSize((UHandle)h) < len)
	{
		DoDebugger();
	}
	(*(h))->size = LWPathByteSize(numChar);
	(*(h))->str[numChar] = 0;
	return len;
}
#endif

DebugAPI(MgErr) LWPtrToLWPath(const LWChar *srcPtr, int32 srcLen, LWPathHandle *lwstr, int32 extraChar)
{
	MgErr err = noErr;
	int32 len = ((sizeof(gNotAPath) / sizeof(LWChar)) - 1);
	uInt8 type = fNotAPath;
	if (srcLen != len || LWPtrCompare(gNotAPath, srcPtr, srcLen))
	{
		int32 xtrLen = 0,
			  srcOff = HasNTFSDevicePrefix(srcPtr, srcLen),
			  srcRoot = LWPtrRootLen(srcPtr, srcLen, srcOff, &type);

#if Unix || MacOSX
		if (!srcLen || (srcLen == 1 && srcPtr[srcOff] == kPathSeperator))
		{
			srcLen = 0;
			// we want to make the path at least contain a seperater
			xtrLen = 1;
		}
#elif Win32
#if Pharlap
		// Remove any DOS device prefix on Pharlap
		if (srcOff)
		{
			if (type == fUNCPath)
				xtrLen = 2;
			srcPtr += srcOff;
			srcLen -= srcOff;
		}
#else /* Windows and not Pharlap */
		if (!srcOff)
		{
			/* Create DOS Device paths for absolute paths that aren't that yet */ 
			if (type == fAbsPath && srcLen)
			{
				/* Absolute path with drive letter */
				xtrLen = 4;
			}
			else if (type == fUNCPath)
			{
				/* Absolute UNC path */
				xtrLen = 8;
				srcPtr += 2;
				srcLen -= 2;
				srcRoot -= 2;
			}
		}
#endif
#endif
		err = LWPathResize(lwstr, srcLen + xtrLen, extraChar);
		if (!err)
		{
			if (xtrLen)
			{
#if Win32
#if Pharlap
				LWPathNCat(lwstr, 0, "\\\\", xtrLen);
#else
				LWPathNCat(lwstr, 0, L"\\\\?\\UNC\\", xtrLen);
#endif
#else
				LWPathNCat(lwstr, 0, "//", xtrLen);
#endif
			}

			if (srcLen)
			{
				return LWPtrNormalize(srcPtr, srcLen, srcRoot, lwstr, xtrLen, type);
			}
			srcLen += xtrLen;
		}
	}
	else
	{
		srcLen = 0;
		err = LWPathResize(lwstr, 0, 0);
	}
	if (!err)
	{
		LWPathLenSet(*lwstr, srcLen);
		LWPathFlagsSet(*lwstr, 0);
		LWPathTypeSet(*lwstr, type);
		LWPathCntSet(*lwstr, 0);
	}
	return err;
}

DebugAPI(MgErr) LWPathResize(LWPathHandle *lwstr, size_t numChar, size_t extraChar)
{
	int32 tmp, len = LWPathHdlSzNumChr(numChar + extraChar);
	if (*lwstr)
	{
		tmp = DSGetHandleSize((UHandle)*lwstr);
		if (tmp < len)
			DSSetHandleSize((UHandle)*lwstr, len);
	}
	else
	{
		*lwstr = (LWPathHandle)DSNewHandle(len);
		if (!*lwstr)
			return mFullErr;
		/* only initialize the path header */
		ClearMem((UPtr)(**lwstr), offsetof(LWPathRec, str));
	}
	return mgNoErr;
}

DebugAPI(MgErr) LWPathCopy(LWPathHandle *dst, LWPathHandle src)
{
	MgErr err = mgNoErr;
	size_t size = LWPathHdlSzHandle(src);
	if (size >= LWPathByteLength(0))
	{
		if (*dst)
		{
			err = DSSetHandleSize((UHandle)*dst, size);
		}
		else
		{
			*dst = (LWPathHandle)DSNewHandle(size);
			if (!*dst)
				err = mFullErr;
		}
		if (!err)
		{
			MoveBlock(*src, **dst, size - sizeof(LWChar));
		}
		LWPathBuf(*dst)[LWPathLenGet(*dst)] = 0;
	}
	else if (*dst)
	{
#if DEBUG
		if (size > 0)
			DoDebugger();
#endif
		DSDisposeHandle((UHandle)*dst);
		*dst = NULL;
	}
	return err;
}

DebugAPI(MgErr) LWPathDispose(LWPathHandle *lwstr)
{
	MgErr err = mgNoErr;
	if (lwstr && *lwstr)
	{
		err = DSDisposeHandle((UHandle)*lwstr);
		*lwstr = NULL;
	}
	return err;
}

MgErr LWPathZeroTerminate(LWPathHandle *pathName, int32 len)
{
	int32 newLen = len < 0 ? LWPathLenGet(*pathName) : len;
	MgErr err = LWPathResize(pathName, newLen, 0);
	if (!err && newLen >= 0)
	{
		LWPathLenSet(*pathName, newLen);
	}
	return err;
}

DebugAPI(MgErr) LWPtrNormalize(const LWChar *srcPtr, int32 srcLen, int32 srcRoot, LWPathHandle *pathName, int32 tgtOff, uInt8 type)
{
	LWChar *tgtPtr = LWPathBuf(*pathName);
	int32 srcOff = 0, tgtRoot = tgtOff + srcRoot;
	uInt16 cnt = 0;

	/*
	 srcLen  srcPtr[0]    
       0        x         0
       1        .         0
       1        /         0   Non-Windows
       1        x         1
	  >=2       x         1
	*/

	if (srcPtr && srcPtr[0] && (srcLen > 1 || (srcLen == 1 && srcPtr[0] != '.')))
	{
		/*								prefix	srcRoot	type   cnt
		rtest\file						   0	   0	 rel	2	+1
		.\rtest\file					   0	   0	 rel	2	+1
		C:\								   0       3	 abs    1
		C:rtest\file					   0	   2	 abs	3	+2
		C:\rtest\file					   0	   3	 abs	3	+2
		\\server\share\rtest\file		   0	  15	 unc	3	+2
		\\?\rtest\file					   4	   4	 rel	3	+1
		\\?\rtest\file					   4	   4	 rel	3	+1
		\\?\C:\							   4	   7	 abs	1
		\\?\C:rtest\file				   4	   6	 abs	3	+2
		\\?\C:\rtest\file				   4       7	 abs	3	+2
		\\?\UNC\server\share\rtest\file	   8	  21	 unc	3	+2

		rtest/file						   0	   0	 rel	2	+1
		./rtest/file					   0	   0	 rel	2	+1
		/                                  0       1     abs    0   
		/dir/rtest/file					   0	   1	 abs	3   +1
		//server/share/rtest/file		   0	  15	 unc	3	+2
		*/

		if ((srcLen > srcRoot || type == fUNCPath) && IsSeperator(srcPtr[srcLen - 1]))
			srcLen--;
		
		if (srcLen > 1 || srcPtr[0] != kPosixPathSeperator)
			cnt++;

		/* Canonicalize the path */
		while (srcLen > srcOff)
		{
			if (srcOff < srcRoot)
			{
#if usesWinPath
				if (srcPtr[srcOff] == kPosixPathSeperator)
				{
					tgtPtr[tgtOff++] = kPathSeperator;
					srcOff++;
				}
				else
#endif
				{
					tgtPtr[tgtOff++] = srcPtr[srcOff++];
				}
			}
			else if (IsSeperator(srcPtr[srcOff]) && IsSeperator(srcPtr[srcOff + 1]))
			{
				srcOff++; /* Reduce // to / */ 
			}
			else if (srcPtr[srcOff] == '.')
			{
				if (srcOff == srcLen - 1)
				{
					// trailing period at end of path
#if usesPosixPath
					tgtPtr[tgtOff++] = srcPtr[srcOff++];
#else
					srcOff++; /* Skip trailing . */
#endif
				}
				else if (IsSeperator(srcPtr[srcOff + 1]))
				{
					if (tgtOff == srcRoot || IsSeperator(srcPtr[srcOff - 1]))
					{
						// embedded current directory, skip it
						srcOff += 2; /* Skip ./ */
					}
					else
					{
						// trailing period at end of path element
#if usesPosixPath
						tgtPtr[tgtOff++] = srcPtr[srcOff++];
#else
						srcOff++; /* Skip trailing . */
#endif
					}
				}
				else if (srcPtr[srcOff + 1] == '.')
				{
					/* .. backs up a directory but only if it is not at the root of the path */
					if (type == fRelPath && srcOff == srcRoot)
					{
						tgtPtr[tgtOff++] = srcPtr[srcOff++];
						tgtPtr[tgtOff++] = srcPtr[srcOff++];
						tgtPtr[tgtOff++] = srcPtr[srcOff++];
						srcRoot += 3;
						cnt++;
					}
					else
					{
						if (tgtOff > tgtRoot && tgtPtr[tgtOff - 1] == kPathSeperator)
						{
							tgtOff -= 2;
							while (tgtOff > tgtRoot && tgtPtr[tgtOff] != kPathSeperator)
								tgtOff--;
							if (tgtPtr[tgtOff] == kPathSeperator)
								tgtOff++; /* Reset to last '\' */
							cnt--;
						}
						srcOff += 3; /* Skip ../ in src path */
					}
				}
				else
					tgtPtr[tgtOff++] = srcPtr[srcOff++];
			}
			else
			{
				if (IsSeperator(srcPtr[srcOff]) && (tgtOff && tgtPtr[tgtOff - 1] != kPathSeperator))
				{
					cnt++;
				}
				if (!IsSeperator(srcPtr[srcOff]) || (tgtOff && tgtPtr[tgtOff - 1] != kPathSeperator))
				{
					tgtPtr[tgtOff++] = IsSeperator(srcPtr[srcOff]) ? kPathSeperator : srcPtr[srcOff];
				}
				srcOff++;
			}
		}
#if usesWinPath
		if (type == fAbsPath)
		{
			int prefix = HasNTFSSessionPrefix(tgtPtr, tgtOff);
			if (prefix)
				tgtPtr[1] = kPathSeperator;

			if (tgtPtr[tgtOff - 1] == ':')
			{	
				/* Append \ to naked drive spec */
				LWPathAppendSeperator(*pathName, tgtOff++);
			}
			if (tgtOff > tgtRoot)
				cnt++;
		}
		else
#endif
		if (type == fUNCPath)
		{
			if (tgtOff > tgtRoot)
				cnt++;
		}
		if (cnt > 2 && tgtPtr[tgtOff - 1] == kPathSeperator && tgtOff > tgtRoot)
		{
			tgtOff--;
			cnt--;
		}
	}
	LWPathCntSet(*pathName, cnt);
	LWPathLenSet(*pathName, tgtOff);
	LWPathTypeSet(*pathName, type);
	return noErr;
}

MgErr LWPathNCat(LWPathHandle *lwstr, int32 offset, const LWChar *str, int32 strLen)
{
	MgErr err;
	LWChar *srcPtr = LWPathBuf(*lwstr);
	int32 xtrLen = 0;

	if (strLen == -1)
	    strLen = lwslen(str);
	if (offset == -1)
		offset = LWPathLenGet(*lwstr);

	if (offset && !IsSeperator(srcPtr[offset - 1]) && !IsSeperator(str[0]))
		xtrLen = 1;

	err = LWPathResize(lwstr, offset + strLen + xtrLen, 0);
	if (!err)
	{
		uInt8 type;
		int32 rootLen;

		srcPtr = LWPathBuf(*lwstr);
		if (xtrLen)
			srcPtr[offset++] = kPathSeperator;
		lwsncpy(srcPtr + offset, str, strLen);
		strLen += offset;

		offset = HasNTFSDevicePrefix(srcPtr, strLen);
		rootLen = LWPtrRootLen(srcPtr, strLen, offset, &type);

		LWPathLenSet(*lwstr, strLen);
		LWPathCntSet(*lwstr, LWPtrDepth(srcPtr, strLen, offset, rootLen));
		LWPathTypeSet(*lwstr, type);
	}
	return err;
}

MgErr LWPathAppendSeperator(LWPathHandle lwstr, int32 len)
{
	MgErr err = mgNoErr;
	
	if (len < 0) 
		len = LWPathLenGet(lwstr);

	if (!IsSeperator(LWPathBuf(lwstr)[len - 1]))
	{
		err =  LWPathResize(&lwstr, len, 1);
		if (!err)
		{
			LWPathBuf(lwstr)[len++] = kPathSeperator;
			LWPathLenSet(lwstr, len);
		}
	}
	return err;
}

LibAPI(MgErr) LWPathGetPathType(LWPathHandle *pathName, int32 *type, int32 *depth)
{
	int32 size = pathName ? LWPathLenGet(*pathName) : -1;
	if (size >= 0)
	{
		if (type)
			*type = LWPathTypeGet(*pathName);
		if (depth)
			*depth = LWPathCntGet(*pathName);
	}
	else
	{
		// We make it the root path so that we can pass in an empty string buffer
		if (type)
			*type = fAbsPath;
		if (depth)
			*depth = 0;	
	}
	return mgNoErr;
}

LibAPI(MgErr) LWPathParentPath(LWPathHandle *filePath, LStrHandle *fileName, LVBoolean *empty)
{
	MgErr err = mgNoErr;
	LWChar *srcPtr = LWPathBuf(*filePath);
	uInt8 srcType = fNotAPath;
	uInt16 srcCnt = LWPathCntGet(*filePath);
	int32 offset, srcLen = LWPathLenGet(*filePath),
		  srcOff = HasNTFSDevicePrefix(srcPtr, srcLen),
	      srcRoot = LWPtrRootLen(srcPtr, srcLen, srcOff, &srcType);

	// Remove path seperator at end
	if (srcLen > srcRoot && IsSeperator(srcPtr[srcLen - 1]))
		LWPathLenSet(*filePath, --srcLen);

	// If the path is empty, invalid or just containing the relative path prefix, there is nothing we can do with this path
	if ((srcLen <= srcOff) || (srcType == fNotAPath) || !srcCnt ||
		((srcType == fRelPath) && (srcLen == srcRoot + 1) && (srcPtr[srcRoot] == kRelativePathPrefix)))
		return mgArgErr;

	if (fileName && LStrLenH(*fileName))
		LStrLen(**fileName) = 0;

	srcCnt--;
	if (srcRoot < srcLen)
	{
		offset = LWPtrParent(srcPtr, srcLen, srcRoot);
		if (empty)
#if usesWinPath
			*empty = (LVBoolean)(srcOff >= offset);
#else
			*empty = (LVBoolean)(srcRoot >= offset);
#endif
		/*															cntin	root	cntout
			C:\test				C:\				test				  2		 3		   1
			C:\test\test		C:\test			test				  3		 3		   2
			\\server\share\test	\\server\share	test				  2		15		   1

			/test				<EmptyPath>		test				  1      1		   0
			/test/test			/test			test				  2		 1		   1
		*/
		srcLen = offset;
		if (IsSeperator(srcPtr[offset]))
		{
			offset++;
			if (srcCnt == 1 && srcType == fAbsPath)
			{
				srcLen++;
			}
		}
#if usesWinPath
		else if (IsSeperator(srcPtr[offset - 1]) && (srcCnt > 1 || srcType != fAbsPath))
#else
		else if (IsSeperator(srcPtr[offset - 1]))
#endif
		{
			srcLen--;
		}
	}
	else
	{
		/*															cntin	root	cntout
		    <EmptyPath>         <NotAPath>                            0      0		  -1
			C:\					<EmptyPath>		C 					  1		 3		   0
			\\server\share		<NotAPath>		\\server\share		  1     14		  -1

			/					<NotAPath>							  0		 1		  -1
		*/
		if (empty)
			*empty = LV_TRUE;
		offset = srcOff;
#if usesWinPath
#if !Pharlap
		if (srcType == fUNCPath)
		{
			if (offset == 8)
				offset = 6;
			srcType = fAbsPath;
		}
		else
#endif
		if (srcType == fAbsPath)
		{
			LWPathLenSet(*filePath, 1);
		}
		else if (!srcCnt)
#endif
		{
			srcType = fNotAPath;
		}
		srcLen = 0;
	}
	err = LStrFromLWPath(fileName, CP_UTF8, filePath, offset, kDefaultPath);
	if (!err)
	{
		LWPathHandle temp = NULL;
		err = LWPathCopy(&temp, *filePath);
		if (!err)
		{
#if usesWinPath && !Pharlap
			if (!srcLen && srcOff == 6)
				LStrBuf(*temp)[0] = kPathSeperator;
#endif
			LWPathTypeSet(temp, srcType);
			LWPathLenSet(temp, srcLen);
			LWPathCntSet(temp, srcCnt);
			*filePath = temp;
		}
	}
	return err;
}

MgErr LWPathAppend(LWPathHandle srcPath, int32 end, LWPathHandle *newPath, LWChar *relPtr, int32 relLen)
{
	MgErr err = mgNoErr;
	LWPathHandle tmpPath = newPath ? *newPath : srcPath;
	LWChar *srcPtr = LWPathBuf(srcPath);
	uInt8 relType = fNotAPath, srcType = LWPathTypeGet(srcPath);
	int32 xtrLen = 0,
		  srcLen = end >= 0 ? end : LWPathLenGet(srcPath),
		  srcOff = HasNTFSDevicePrefix(srcPtr, srcLen),
		  srcRoot = LWPtrRootLen(srcPtr, srcLen, srcOff, NULL),
		  srcCnt = end >= 0 ? LWPtrDepth(srcPtr, end, srcOff, srcRoot) : LWPathCntGet(srcPath),
		  relOff = HasNTFSDevicePrefix(relPtr, relLen),
		  relRoot = LWPtrRootLen(relPtr, relLen, relOff, &relType),
		  relCnt = LWPtrDepth(relPtr, relLen, relOff, relRoot);

	if (relRoot >= relLen)
	{
		/* if the relative path is empty we do want to consider it definitely an empty relative path */
		relType = fRelPath;
	}
	else if (relType == fRelPath)
	{
		relOff = LWPtrUpMatches(relPtr, relLen, relRoot);
		if (relOff > relRoot)
		{
			int32 i, cnt = (relOff - relRoot + 1) / 3;
			for (i = 0; i < cnt && srcLen > srcRoot; i++)
			{
				srcLen = LWPtrParent(srcPtr, srcLen, srcRoot);
			}
			if (i < cnt)
				return mgArgErr;
			srcCnt -= cnt;
			relCnt -= cnt;
		}
	}

	/* If either path is invalid or start path is not empty and relative path is not relative, we have a problem */
	if ((srcType == fNotAPath) || (relType == fNotAPath) || (srcCnt && relType != fRelPath))
	{
		return mgArgErr;
	}

#if usesWinPath
	if (relCnt && srcCnt && !IsSeperator(srcPtr[srcLen - 1]))
	{
		xtrLen = 1;
	}
	else if (!srcCnt && srcType == fAbsPath && relType == fRelPath)
	{
		xtrLen = 2;
	}
#else
	if ((srcType != fNotAPath && srcLen && !IsSeperator(srcPtr[srcLen - 1])) ||
		 srcType == fAbsPath && !srcLen)
	{
		xtrLen = 1;
	}
#endif

	err = LWPathResize(&tmpPath, srcLen + relLen - relOff + xtrLen, 0);
	if (!err)
	{
		if (srcPath != tmpPath && (srcCnt || relType == fRelPath))
		{
			// We have a new destination path with a valid source path, copy it over
			MoveBlock(*srcPath, *tmpPath, srcLen * sizeof(LWChar) + offsetof(LWPathRec, str));
		}
		else if (!srcCnt && relType != fRelPath)
		{
			// No source path and the incoming path is not a relative path, so copy over 
			MoveBlock(relPtr, *tmpPath, relLen + sizeof(int32));
		}

		/* If empty relative path, there is nothing to append */
		if (relCnt)
		{
			srcPtr = LWPathBuf(tmpPath);
			if (xtrLen == 1)
			{
				srcPtr[srcLen++] = kPathSeperator;
			}
			relLen -= relOff;
			MoveBlock(relPtr + relOff, srcPtr + srcLen, relLen * sizeof(LWChar));
			srcLen += relLen;
#if usesWinPath
			if (!err && xtrLen == 2)
			{
				srcPtr[srcLen++] = ':';
				srcPtr[srcLen++] = kPathSeperator;
			}
#endif
		}
		LWPathCntSet(tmpPath, srcCnt + relCnt);
		LWPathLenSet(tmpPath, srcLen);
		if (newPath && *newPath != tmpPath)
			*newPath = tmpPath;
	}
	return err;
}

DebugAPI(MgErr) LWPathAppendUStr(LWPathHandle *filePath, int32 end, const LStrHandle relString)
{
	LWPathHandle relPath = NULL;
	MgErr err = LStrPtrToLWPath(LStrBuf(*relString), LStrLen(*relString), CP_UTF8, &relPath, 0);
	if (!err)
	{
		err = LWPathAppend(*filePath, end, filePath, LWPathBuf(relPath), LWPathLenGet(relPath));
		LWPathDispose(&relPath);
	}
	return err;
}

LibAPI(MgErr) LWPathAppendLWPath(LWPathHandle *filePath, const LWPathHandle *relPath)
{
	return LWPathAppend(*filePath, -1, filePath, LWPathBuf(*relPath), LWPathLenGet(*relPath));
}

int32 LWPathNameCompare(LWChar *path1, LWChar *path2, int32 len)
{
#if Win32
	return _wcsnicmp(path1, path2, len);
#else
	return strncmp(path2, path2, len);
#endif
}

LibAPI(MgErr) LWPathRelativePath(LWPathHandle *startPath, LWPathHandle *endPath, LWPathHandle *relPath)
{
	MgErr err = mgArgErr;
	if (startPath && endPath)
	{
		int16 startCnt = LWPathCntGet(*startPath),
			  endCnt = LWPathCntGet(*endPath),
			  level, notchesUp;
		uInt8 startType = fNotAPath, endType = fNotAPath;
		LWChar *startp = LWPathBuf(*startPath),
			   *endp = LWPathBuf(*endPath);
		int32 startLen = LWPathLenGet(*startPath),
			  startOff = HasNTFSDevicePrefix(startp, startLen),
			  startRoot = LWPtrRootLen(startp, startLen, startOff, &startType),
			  endLen = LWPathLenGet(*endPath),
			  endOff = HasNTFSDevicePrefix(endp, endLen),
			  endRoot = LWPtrRootLen(endp, endLen, endOff, &endType); 

		if (startType == fAbsPath && endType == fAbsPath && relPath)
		{
			for (level = startCnt; level > 0; level--)
			{
				if (level < startCnt)
				{
					startRoot = LWPtrNextElement(startp, startLen, startOff);
					endRoot = LWPtrNextElement(endp, endLen, endOff);
				}
				if (startRoot < 0 || endRoot < 0 || startRoot - startOff != endRoot - endOff)
					break;
				if (LWPathNameCompare(startp + startOff, endp + endOff, startRoot - startOff))
					break;
				startOff = startRoot + 1;
				endOff = endRoot + 1;
			}
			if (level < startCnt)
			{
				notchesUp = level;
				endCnt -= startCnt - level;
		
				err = LWPathResize(relPath, notchesUp * 3 + endLen - endOff, 0);
				if (!err)
				{
					MoveBlock(endp + endOff, LWPathBuf(*relPath) + notchesUp * 3, endLen - endOff);
					for (level = 0; level < notchesUp; level++)
					{
						*endp++ = '.';
						*endp++ = '.';
						*endp++ = kPathSeperator;
					}
					LWPathCntSet(*relPath, endCnt + notchesUp);
					LWPathLenSet(*relPath, notchesUp * 3 + endLen - endOff);
					LWPathTypeSet(*relPath, fRelPath);
				}
			}
			else
			{
				// We can't create a relative path between two absolute paths on different volumes/shares
			}
		}
	}
	return err;
}

#define kFlatPathCode	RTToL('P','T','H','0')

/*
Write the information in path p to memory pointed to by fp and return the size used.
Call with fp==NULL to preflight for size information.
	flat format:
		int32 ver		'PTH0' magic code to indicate flatten version
		int32 size		handle size needed for path
		uint8 flags		highest significant bit set for UTF8 encoded path, otherwise it is using the user specific 8-bit encoding
		uInt8 type		fAbsPath, fRelPath, fNotAPath, fUNCPath
		int16 cnt		# of p strings
		pStrings		concatenated pstrings
		[pad byte]		to make total size even
*/
LibAPI(MgErr) LWPathFlatten(LWPathHandle *pathName, uInt32 flags, UPtr dst, int32 *length)
{
	MgErr err = noErr;
	LWChar *srcPtr = LWPathBuf(*pathName);
	int32 srcLen = LWPathLenGet(*pathName);
	int32 srcOff = 0, srcRoot = 0, extLen = 0, bufLen = length && *length ? *length - 12 : 0;
	uInt8 pathFlags = 0;
	uInt8 pathType = LWPathTypeGet(*pathName);
	uInt16 cnt = 0;
	
	if (srcLen)
	{
		srcOff = HasNTFSDevicePrefix(srcPtr, srcLen);
		srcRoot = LWPtrRootLen(srcPtr, srcLen, srcOff, &pathType);

		if (flags & kFlattenUnicode)
		{
#if Win32 && !Pharlap
			pathFlags = kFlagUnicode;
#else
			pathFlags = LWPathFlagsGet(*pathName);
#endif
		}
		/* Preflight the path */
		switch (pathType)
		{
			case fAbsPath:
				/* C:\path\path => \01C\04path, nothing needed now but C will need to move one up */
				/* /path/path => \04path\04path, do nothing */
				break;
			case fRelPath:
				/* for each up level add an extra byte */
				srcOff = LWPtrUpMatches(srcPtr, srcLen, srcRoot);
				extLen += (srcOff - srcRoot + 1) / 3;
				/* path\path => \04path\04path, add an extra len for the first Pascal length */
				/* path/path => \04patsh\04path, add an extra len for the first Pascal length */
				extLen++;
				break;
			case fNotAPath:
				bufLen = -1;
				break;
			case fUNCPath:
				extLen = 3;
#if Win32 && !Pharlap
				if (!srcOff)
#endif
				{
					/* \\server\share\path => \016//server/share\04path, add an extra len for first Pascal length */
					/* //server/share/path => \016//server/share\04path, add an extra len for first Pascal length */
					srcOff = 2;
				}
				break;
		}	
		if (!bufLen)
		{
#if Win32 && !Pharlap
			err = WideCStrToMultiByteBuf(srcPtr + srcOff, srcLen - srcOff, NULL, &bufLen, pathFlags ? CP_UTF8 : GetCurrentCodePage(CP_ACP), 0, NULL);
#else
			err = ConvertCStringBuf(srcPtr + srcOff, srcLen - srcOff, GetCurrentCodePage(CP_ACP), NULL, &bufLen, pathFlags ? CP_UTF8 : GetCurrentCodePage(CP_ACP), 0, NULL);
#endif
		}
	}
	if (dst)
	{
		UPtr ptr = dst;

		SetALong(ptr, kFlatPathCode);
		ptr += 4;
		SetALong(ptr, 4 + extLen + bufLen);
		LToStd(ptr);
		ptr += 4;
		*ptr++ = pathFlags;
		*ptr++ = pathType;

		if (srcLen && bufLen > 0)
		{
			UPtr end, lPtr;

			// Skip cnt for now
			ptr += 2;

			if (extLen)
			{
				switch (pathType)
				{
					case fRelPath:
						ClearMem(ptr, extLen * sizeof(LWChar));
						ptr += extLen;
						break;
					case fUNCPath:
						ptr++; // Skip length byte for now
						*ptr++ = kPathSeperator;
						*ptr++ = kPathSeperator;
						break;
					default:
						ptr++; // Skip length byte for now
				}
			}
#if Win32 && !Pharlap
			err = WideCStrToMultiByteBuf(srcPtr + srcOff, srcLen - srcOff, ptr, &bufLen, pathFlags ? CP_UTF8 : GetCurrentCodePage(CP_ACP), 0, NULL);
#else
			err = ConvertCStringBuf(srcPtr + srcOff, srcLen - srcOff, GetCurrentCodePage(CP_ACP), ptr, &bufLen, pathFlags ? CP_UTF8 : GetCurrentCodePage(CP_ACP), 0, NULL);
#endif
#if Win32
			if (!extLen)
			{
				ptr[1] = ptr[0];
			}
#endif
			ptr = dst + 4;
			SetALong(ptr, 4 + extLen + bufLen);
			LToStd(ptr);
			
			ptr = dst + 12;
			end = ptr + extLen + bufLen;

			if (pathType == fRelPath)
			{
				cnt = (uInt16)(extLen - 1);
				ptr += (extLen - 1);
			}
			else if (pathType == fUNCPath)
			{
				lPtr = ptr;
				ptr += extLen; // Skip over the length byte and the two seperators
				// search two separators for UNC server and share
				for (;ptr < end && *ptr && !IsSeperator(*ptr); ptr++);
				*ptr++ = kPathSeperator;
				srcOff++;
				for (;ptr < end && *ptr && !IsSeperator(*ptr); ptr++);
				*lPtr = (uInt8)(ptr - lPtr - 1);
				cnt++;
			}
				
			while (ptr < end)
			{
				lPtr = ptr++; // Skip over the length byte
				for (;ptr < end && *ptr && !IsSeperator(*ptr); ptr++);
				*lPtr = (uInt8)(ptr - lPtr - 1);
				cnt++;
			}
			ptr = dst + 10;
		}
		SetAWord(ptr, cnt);
		WToStd(ptr);
	}
	if (length)
		*length = 12 + extLen + bufLen;
	return noErr;
}

LibAPI(MgErr) LWPathUnflatten(UPtr ptr, int32 length, LWPathHandle *pathName, uInt32 flags)
{
	int32 len;
	if (length < 8 || GetALong(ptr) != kFlatPathCode)
		return mgArgErr;

	ptr += 4;
	len = GetALong(ptr);
	StdToL(&len);
	if (len)
	{
		MgErr err = noErr;
		uInt8 pathFlags, pathType;
		uInt16 cnt;

		ptr += 4;
		pathFlags = *ptr++;
		pathType = *ptr++;
		cnt = GetAWord(ptr);
		StdToW(&cnt);
		ptr += 2;


		return err;
	}
	return LWPathResize(pathName, 0, 0);
}

static ResType LWPathFileTypeFromExt(LWPathHandle lwstr)
{
	int32 len = LWPathLenGet(lwstr), k = 0;
    LWChar *ptr = LWPathBuf(lwstr);

	/* look backwards	*/
	for (ptr += len - 1; k < len && k < kMaxFileExtLength; --ptr, k++)
	{
		if (*ptr == '.')
		{
			uChar str[kMaxFileExtLength + 1];
            PStrLen(str) = (uChar)k;
            for (len = 1; k; k--)
				str[len++] = (uChar)*ptr++;
			return PStrHasRezExt(str);
		}
	}
	return kUnknownFileType;
}

MgErr LWPathGetFileTypeAndCreator(LWPathHandle lwstr, ResType *fType, ResType *fCreator)
{
	/* Try to determine LabVIEW file types based on file ending? */
	uInt32 creator = kUnknownCreator,
		   type = LWPathFileTypeFromExt(lwstr);
	if (type != kUnknownFileType)
	{
		creator = kLVCreatorType;
	}

	if (fType)
		*fType = type;
	if (fCreator)
		*fCreator = creator;

	return noErr;
}

/* Windows: path is an ACP mulibyte encoded path string and lwstr is filled with a Windows UTF16LE string from the path
   MacOSX, Unix, VxWorks, Pharlap: path is a platform encoded path string and lwstr is filled with a local encoded SBC or
   MBC string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
DebugAPI(MgErr) LStrPtrToLWPath(const UPtr string, int32 len, uInt32 codePage, LWPathHandle *lwstr, int32 reserve)
{
	MgErr err = noErr;
	LWStrHandle temp = NULL;

	if (string && len > 0)
	{
#if usesHFSPath
		err = CStrToPosixPath(string, len, codePage, (LStrHandle*)&temp, CP_UTF8, '?', NULL, FALSE);
#elif Unix || MacOSX || Pharlap
		err = ConvertCString(string, len, codePage, (LStrHandle*)&temp, CP_UTF8, '?', NULL);
#else
		err = MultiByteCStrToWideString(string, len, codePage, (WStrHandle*)&temp);
#endif
	}
	if (!err)
	{
		err = LWPtrToLWPath(LWStrBuf(temp), LWStrLen(temp), lwstr, reserve);
		DSDisposeHandle((UHandle)temp);
	}
	return err;
}

/* Windows: path is an UTF8 encoded path string and lwstr is filled with a Windows UTF16LE string from the path
   MacOSX, Unix, VxWorks, Pharlap: path is an UTF8 encoded path string and lwstr is filled with a local encoded SBC or
   MBC string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
LibAPI(MgErr) UStrToLWPath(const LStrHandle path, LWPathHandle *lwstr, int32 reserve)
{
	return LStrPtrToLWPath(LStrBufH(path), LStrLenH(path), CP_UTF8, lwstr, reserve);
}

/* Windows: path is a LabVIEW path and lwstr is filled with a Windows UTF16LE string from this path
   MacOSX, Unix, VxWorks, Pharlap: path is a LabVIEW path and lwstr is filled with a local encoded SBC or MBC
   string from the path. It could be UTF8 if the local encoding of the platform is set as such (Linux + MacOSX) */ 
LibAPI(MgErr) LPathToLWPath(const Path pathName, LWPathHandle *lwstr, int32 reserve)
{
	int32 type = fNotAPath;
	MgErr err = FGetPathType(pathName, &type);
	if (!err)
	{
		if (type != fNotAPath && FDepth(pathName))
		{
			LStrPtr lstr;
		    int32 bufLen = -1;
			
			err = FPathToText(pathName, (LStrPtr)&bufLen);
			if (!err)
			{
				lstr = (LStrPtr)DSNewPtr(sizeof(int32) + bufLen);
				if (!lstr)
					return mFullErr;
				LStrLen(lstr) = bufLen;
				err = FPathToText(pathName, lstr);
				if (!err)
					err = LStrPtrToLWPath(LStrBuf(lstr), LStrLen(lstr), GetCurrentCodePage(CP_ACP), lwstr, reserve);

				DSDisposePtr((UPtr)lstr);
			}
		}
		else
		{
			err = LWPathResize(lwstr, 0, 0);
			if (!err)
			{
				LWPathLenSet(*lwstr, 0);
				LWPathFlagsSet(*lwstr, 0);
				LWPathTypeSet(*lwstr, type);
				LWPathCntSet(*lwstr, 0);
			}
		}
    }
    return err;
}

DebugAPI(MgErr) LStrFromLWPath(LStrHandle *pathName, uInt32 codePage, const LWPathHandle *lwstr, int32 offset, uInt32 flags)
{
	MgErr err = noErr;
	uInt16 type = lwstr ? LWPathTypeGet(*lwstr) : fNotAPath;
	if (type != fNotAPath)
	{
		int32 len = LWPathLenGet(*lwstr);
		if (len)
		{
			LWChar *ptr = LWPathBuf(*lwstr);
#if usesHFSPath
			err = ConvertFromPosixPath(ptr + offset, len - offset, CP_UTF8, pathName, codePage, '?', FALSE);
#elif Unix || MacOSX || Pharlap
			err = ConvertCString(ptr + offset, len - offset, CP_UTF8, pathName, codePage, '?', NULL);
#else /* Windows and not Pharlap */
			if (!offset && !(flags & kKeepDOSDevice))
			{
				offset = HasNTFSDevicePrefix(ptr, len);
				if (offset == 8)
					offset = 6;
			}
			err = WideCStrToMultiByte(ptr + offset, len - offset, pathName, 0, codePage, '?', NULL);
			if (!err && type == fUNCPath && LStrBuf(**pathName)[1] == kPathSeperator)
			{
				LStrBuf(**pathName)[0] = kPathSeperator;
			}
#endif
		}
		else if (pathName && *pathName)
		{
			LStrLen(**pathName) = 0;
		}
	}
	else if (lwstr)
	{
#if usesWinPath
		err = WideCStrToMultiByte((const wchar_t*)gNotAPath, -1, pathName, 0, codePage, 0, NULL);
#else
		err = ConvertCString(gNotAPath, -1, CP_UTF8, pathName, codePage, '?', NULL);
#endif
	}
	else if (pathName && *pathName)
	{
		LStrLen(**pathName) = 0;
	}
	return err;
}

LibAPI(MgErr) UStrFromLWPath(LStrHandle *pathName, const LWPathHandle *lwstr, uInt32 flags)
{
	return LStrFromLWPath(pathName, CP_UTF8, lwstr, 0, flags);
}

LibAPI(MgErr) LPathFromLWPath(Path *pathName, const LWPathHandle *lwstr)
{
	LStrHandle dest = NULL;
	MgErr err = LStrFromLWPath(&dest, GetCurrentCodePage(CP_ACP), lwstr, 0, kDefaultPath);
	if (!err)
	{
		err = FTextToPath(LStrBuf(*dest), LStrLen(*dest), pathName);
		DSDisposeHandle((UHandle)dest);
	}
	return err;
}

/* 
   These two APIs will use the platform specific path syntax except for the
   MacOSX 32-bit plaform where it will use posix format
*/
LibAPI(MgErr) LPathToText(Path path, LVBoolean isUtf8, LStrHandle *str)
{
	int32 pathLen = -1;
	MgErr err = FPathToText(path, (LStrPtr)&pathLen);
	if (!err)
	{
		err = NumericArrayResize(uB, 1, (UHandle*)str, pathLen + 1);
		if (!err)
		{
			LStrLen(**str) = pathLen;
			err = FPathToText(path, **str);
			if (!err)
			{
#if usesHFSPath
				err = CStrToPosixPath(LStrBufH(*str), LStrLenH(*str), GetCurrentCodePage(CP_ACP), str, isUtf8 ? CP_UTF8 : GetCurrentCodePage(CP_ACP), '?', NULL, false);
#else
				if (isUtf8)
					err = ConvertLString(*str, GetCurrentCodePage(CP_ACP), str, isUtf8 ? CP_UTF8 : GetCurrentCodePage(CP_ACP), '?', NULL);
#endif
				if (!err && IsSeperator(LStrBuf(**str)[LStrLen(**str) - 1]))
				{
					LStrLen(**str)--;
				}
			}
		}
	}
	return err;
}

LibAPI(MgErr) LPathFromText(CStr str, int32 len, Path *path, LVBoolean isDir)
{
	MgErr err = mgNoErr;
#if usesHFSPath
	LStrHandle hfsPath = NULL;
	/* Convert the posix path to an HFS path */
	err = ConvertFromPosixPath(str, len, GetCurrentCodePage(CP_ACP), &hfsPath, GetCurrentCodePage(CP_ACP), '?', NULL, isDir);
	if (!err && hfsPath)
	{
		err = FTextToPath(LStrBuf(*hfsPath), LStrLen(*hfsPath), path);
		DSDisposeHandle((UHandle)hfsPath);
	}
#else
	Unused(isDir);
	err = FTextToPath(str, len, path);
#endif
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

static LVBoolean IsExtendedByteCode(UPtr ptr, int32 len)
{
	for (; len > 0; len--)
	{
		if (*ptr++ >= 0x80)
		{
			return LV_TRUE;
		}
	}
	return LV_FALSE;
}

LibAPI(LVBoolean) HasExtendedASCII(LStrHandle string)
{
	if (string)
	{
		return IsExtendedByteCode(LStrBuf(*string), LStrLen(*string));
	}
	return LV_FALSE;
}

#ifdef HAVE_ICONV
static MgErr unix_convert_mbtomb(const char *src, int32 srcLen, uInt32 srccp, UPtr dest, int32 *outLen, uInt32 dstcp, char defaultChar, LVBoolean *defaultCharWasUsed);

static uInt32 codePage125xOEM[] = {852, 866, 850, 737, 857, 862, 720, 775}; 
#endif

LibAPI(uInt32) GetCurrentCodePage(uInt32 codePage)
{
	if (codePage == CP_ACP || codePage == CP_OEMCP)
	{		
#if Win32
		return codePage == CP_ACP ? GetACP() : GetOEMCP();
#elif MacOSX
		Unused(acp);
		CFStringEncoding encoding = CFStringGetSystemEncoding();
		return CFStringConvertEncodingToWindowsCodepage(encoding);
#else
		if (utf8_is_current_mbcs())
			return CP_UTF8;
#ifdef HAVE_ICONV
		char *lc = setlocale(LC_CTYPE, NULL);
		if (lc)
		{
			char *tc = strrchr(lc, '.');
			if (tc)
			{
				tc++;
			}
			else
			{
				tc = lc;
			}

			if (!strcasecmp(tc, "UTF-8"))
			{
				return CP_UTF8;
			}

			if (!strncasecmp(tc, "CP", 2))
			{
				int n = sscanf(tc + 2, "%u", &codePage);
				if (codePage == CP_OEMCP && n > 0)
				{
					if (codePage >= 1250 && codePage <= 1257)
					{
						codePage = codePage125xOEM[codePage - 1250];
					}
				}
				return codePage;
			}
		}
#endif
		return codePage == CP_ACP ? 1252 : 437;
#endif
	}
	return codePage;
}

static MgErr ConvertCStringBuf(ConstCStr src, int32 srcLen, uInt32 srccp, UPtr ptr, int32 *length, uInt32 dstcp, char defaultChar, LVBoolean *defUsed)
{
	MgErr err = noErr;
	srccp = GetCurrentCodePage(srccp);
	dstcp = GetCurrentCodePage(dstcp);
	if (srccp != dstcp)
	{
#ifdef HAVE_ICONV
		err = unix_convert_mbtomb(src, srcLen, srccp, ptr, length, dstcp, defaultChar, defUsed);
#else
		WStrHandle ustr = NULL;
		err = MultiByteCStrToWideString(src, srcLen, srccp, &ustr);
		if (!err && ustr)
		{
			err = WideCStrToMultiByteBuf(LWStrBuf(ustr), LWStrLen(ustr), ptr, length, dstcp, defaultChar, defUsed);
			DSDisposeHandle((UHandle)ustr);
		}
#endif
		return err;
	}

	if (srcLen == -1)
		srcLen = StrLen(src);
	if (ptr && srcLen > 0)
	{
		if (srcLen > *length)
			srcLen = *length;
		MoveBlock(src, ptr, srcLen);
	}
	*length = srcLen;
	return err;
}

LibAPI(MgErr) ConvertCString(ConstCStr src, int32 srcLen, uInt32 srccp, LStrHandle *dest, uInt32 dstcp, char defaultChar, LVBoolean *defUsed)
{
	MgErr err = noErr;
	srccp = GetCurrentCodePage(srccp);
	dstcp = GetCurrentCodePage(dstcp);

	if (srcLen == -1)
		srcLen = StrLen(src);

	if (srccp != dstcp)
	{
		// The source and destination locale are not the same, convert it
#ifdef HAVE_ICONV
		// ICONV can directly convert from one locale into a different one, so avoid the roundtrip from locale to widechar and back
		int32 length = 0;
		err = unix_convert_mbtomb(src, srcLen, srccp, NULL, &length, dstcp, defaultChar, defUsed);
		if (!err)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)dest, length);
			if (!err)
			{
				err = unix_convert_mbtomb(src, srcLen, srccp,  LStrBuf(**dest), &length, dstcp, defaultChar, defUsed);
				if (!err)
					LStrLen(**dest) = length;
			}
		}
#else
		WStrHandle ustr = NULL;
		err = MultiByteCStrToWideString(src, srcLen, srccp, &ustr);
		if (!err && ustr)
		{
			err = WideStringToMultiByte(ustr, dest, dstcp, defaultChar, defUsed);
			DSDisposeHandle((UHandle)ustr);
		}
#endif
		return err;
	}

	if (srcLen > 0)
	{
		err = NumericArrayResize(uB, 1, (UHandle*)dest, srcLen);
		if (!err)
		{
			MoveBlock(src, LStrBuf(**dest), srcLen);
		}
		else
		{
			srcLen = 0;
		}
	}
	if (*dest)
		LStrLen(**dest) = srcLen;
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
		if (IsPosixSeperator(src[0]) && !IsPosixSeperator(src[1]))
		{
			*buf++ = src[1];
			*buf++ = ':';
			len -= 2;
		}
		for (; len; len--, buf++)
		{
			if (IsPosixSeperator(*buf))
			{
				*buf = kPathSeperator;
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

static void TerminateLStr(LStrHandle *dest, int32 numBytes)
{
	LStrLen(**dest) = numBytes;
	LStrBuf(**dest)[numBytes] = 0;
}

#define kPosix		0
#define kFlatten	1

#if usesWinPath
static int32 ConvertToPosixWString(wchar_t *src, int32 srcLen, uInt8 *type, wchar_t *dst)
{
	wchar_t *ptr = dst;
	int32 len = 0, offset = HasNTFSDevicePrefix(src, srcLen);

	WStrRootLen(src, srcLen, offset, type);

	if (*type == fNotAPath)
		return -1;

	if (offset == 8)
	{
		if (ptr)
		{
			*ptr++ = kPosixPathSeperator;
		}
		offset = 7;
		len++;
	}
#if Win32
	else if (src[offset + 1] == ':' && IsSeperator(src[offset + 2]))
	{
		if (ptr)
		{
			*ptr++ = kPosixPathSeperator;
			*ptr++ = src[offset];
		}
		offset = 3;
		len = 2;
	}
#endif
	if (ptr)
	{
		wchar_t *end = src + srcLen;

		for (src += offset; src < end; src++, ptr++)
		{
			if (IsSeperator(*src))
			{	
				*ptr = kPosixPathSeperator;
			}
			else
			{
				*ptr = *src;
			}
		}
	}
	return srcLen - offset + len;
}
#endif

/* Converts a LabVIEW platform path to Unix style path */
MgErr CStrToPosixPath(ConstCStr src, int32 len, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir)
{
    MgErr err = mgArgErr;
#if usesHFSPath
    Unused(defUsed);
	CFStringEncoding encoding = ConvertCodepageToEncoding(srccp);
	if (encoding != kCFStringEncodingInvalidId)
	{
		CFURLRef urlRef = NULL;
		CFStringRef posixRef, fileRef = CFStringCreateWithBytes(kCFAllocatorDefault, src, len, encoding, false);
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
	else
	{
		err = mgNotSupported;
	}
#elif usesWinPath
    Unused(isDir);
	if (src && len && dest)
	{
		WStrHandle temp = NULL;
		MgErr err = MultiByteCStrToWideString(src, len, srccp, &temp);
		if (!err)
		{
			uInt8 type = fNotAPath;
			int32 needed = ConvertToPosixWString(LWStrBuf(temp), LWStrLen(temp), &type, NULL);
			if (needed >= 0)
			{
				err = NumericArrayResize(uW, 1, (UHandle*)&temp, needed);
				if (!err)
				{
					LStrLen(*temp) = ConvertToPosixWString(LWStrBuf(temp), LWStrLen(temp), &type, LWStrBuf(temp));
					err = WideStringToMultiByte(temp, dest, destcp, defaultChar, defUsed);
				}
			}
			else
			{
				err = mgArgErr;
			}
			DSDisposeHandle((UHandle)temp);
		}
	}
#else
    Unused(isDir);
	err = ConvertCString(src, len, srccp, dest, destcp, defaultChar, defUsed);
#endif
	return err;
}

LibAPI(MgErr) ConvertToPosixPath(const LStrHandle src, uInt32 srccp, LStrHandle *dest, uInt32 destcp, char defaultChar, LVBoolean *defUsed, LVBoolean isDir)
{
	return CStrToPosixPath(LStrBufH(src), LStrLenH(src), srccp, dest, destcp, defaultChar, defUsed, isDir);
}

#if Unix
extern MgErr UnixToLVFileErr(void);

#ifdef HAVE_ICONV
static char *iconv_getcharset(uInt32 codePage)
{
	char *cp = NULL;
	if (codePage == CP_UTF8)
		return strdup("UTF-8");
	
	cp = malloc(10);
	if (cp)
	{
		snprintf(cp, 10, "CP%d", GetCurrentCodePage(codePage));
	}
	return cp;
}

#define BUF_LENGTH 1024

static MgErr unix_convert_mbtow(const char *src, int32 srcLen, WStrHandle *dest, uInt32 codePage)
{
	MgErr err =  mgNotSupported;
	char *cp = iconv_getcharset(codePage);
	if (cp)
	{
		iconv_t cd = iconv_open("WCHAR_T", cp);
		if (cd != (iconv_t)-1)
		{
			MgErr err;
			if (srcLen < 0)
				srcLen = strlen(src);
			err = NumericArrayResize(uB, 1, (UHandle*)dest, srcLen * sizeof(wchar_t));
			if (!err)
			{
				char *inbuf = (char*)src; 
				wchar_t *outbuf = WStrBuf(*dest);
				size_t retval, inleft = srcLen, outleft = srcLen * sizeof(wchar_t);
				retval = iconv(cd, &inbuf, &inleft, (char**)&outbuf, &outleft);
				if (retval == (size_t)-1)
					err = UnixToLVFileErr();
				else
					WStrLenSet(*dest, (srcLen * sizeof(wchar_t) - outleft) / sizeof(wchar_t));
			}
			if (iconv_close(cd) != 0 && !err)
			{
				err = UnixToLVFileErr();
			}
		}
		else
		{
		    err = UnixToLVFileErr();
		}
		free(cp);
	}
	return err;
}

static MgErr unix_convert(iconv_t cd, const char *src, int32 srcLen, UPtr dest, int32 *outLen)
{
	MgErr err = noErr;
	char *inbuf = (char*)src, *outbuf;
	size_t retval, cbinleft, cboutleft;

	cbinleft = srcLen;

	if (!dest || !*outLen)
	{
		char buffer[BUF_LENGTH],

		*outLen = 0;
		do
		{
			cboutleft = BUF_LENGTH;
			outbuf = buffer;

			retval = iconv(cd, &inbuf, &cbinleft, &outbuf, &cboutleft);
			if (retval == (size_t)-1)
			{
				err = UnixToLVFileErr();
				if (err == mFullErr)
				{
					*outLen += BUF_LENGTH - cboutleft;
				}
			}
			else
			{
				*outLen += BUF_LENGTH - cboutleft;
			}
		}
		while (err == mFullErr);
	}
	else
	{
		cboutleft = *outLen;
		outbuf = (char*)dest;

		retval = iconv(cd, &inbuf, &cbinleft, &outbuf, &cboutleft);
		if (retval == (size_t)-1)
			err = UnixToLVFileErr();
		else
			*outLen -= cboutleft;
	}
	return err;
}

static MgErr unix_convert_wtomb(const wchar_t *src, int32 srcLen, UPtr dest, int32 *outLen, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	MgErr err =  mgNotSupported;
	char *cp = iconv_getcharset(codePage);
	Unused(defaultChar);
	Unused(defaultCharWasUsed);
	if (cp)
	{
		iconv_t cd = iconv_open(cp, "WCHAR_T");
		if (cd != (iconv_t)-1)
		{
			if (srcLen == -1)
				srcLen = wcslen(src);

			err = unix_convert(cd, (const char*)src, srcLen * sizeof(wchar_t), (char*)dest, outLen);
			if (iconv_close(cd) != 0 && !err)
				err = UnixToLVFileErr();
		}
		else
		{
		    err = UnixToLVFileErr();
		}
		free(cp);
	}
	return err;
}

static MgErr unix_convert_mbtomb(const char *src, int32 srcLen, uInt32 srccp, UPtr dest, int32 *outLen, uInt32 dstcp, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	MgErr err =  mgNotSupported;
	char *scp = iconv_getcharset(srccp),
	     *dcp = iconv_getcharset(dstcp);
	Unused(defaultChar);
	Unused(defaultCharWasUsed);
	if (dcp && scp)
	{
		if (srcLen == -1)
			srcLen = strlen(src);
		if (strcasecmp(scp, dcp))
		{
			iconv_t cd = iconv_open(dcp, scp);
			if (cd != (iconv_t)-1)
			{
				err = unix_convert(cd, src, srcLen, (char*)dest, outLen);
				if (iconv_close(cd) != 0 && !err)
					err = UnixToLVFileErr();
			}
			else
			{
				err = UnixToLVFileErr();
			}
		}
		else
		{
			
		}
	}
	free(scp);
	free(dcp);
	return err;
}

#else // HAVE_ICONV
/* We don't have a good way of converting from an arbitrary character set to wide char and back.
   Just do default mbcs to wide char and vice versa ??? */
static MgErr unix_convert_mbtow(const char *src, int32 srcLen, WStrHandle *dest, uInt32 codePage)
{
	size_t max, mLen;
	const char *sPtr = src;
	int32 wLen = 0;
	MgErr err = noErr;

	if (codePage == CP_UTF8)
	{
        err = utf8towchar(src, srcLen, NULL, &wLen, 0);
	}
	else
	{
#ifdef HAVE_MBRTOWC
		mbstate_t mbs;
		mbrtowc(NULL, NULL, 0, &mbs);
#else
		mbtowc(NULL, NULL, 0);
#endif
		if (srcLen < 0)
			srcLen = strlen(src);
		max = srcLen;
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
				err = utf8towchar(src, srcLen, WStrBuf(*dest), NULL, wLen + 1);
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
				max = srcLen;
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
	    WStrLenSet(*dest, wLen); 
	return err;
}

static MgErr unix_convert_wtomb(const wchar_t *src, int32 srclen, UPtr dest, int32 *outLen, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
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
		err = NumericArrayResize(uB, 1, (UHandle*)dest, offset + dLen + 1);
		if (!err)
		{
			if (codePage == CP_UTF8)
			{
				err = wchartoutf8(src, sLen, LStrBuf(**dest) + offset, NULL, dLen + 1);
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
	    LStrLen(**dest) = offset + dLen; 
	return err;
}
#endif
#endif

LibAPI(MgErr) MultiByteCStrToWideString(ConstCStr src, int32 srclen, uInt32 codePage, WStrHandle *dest)
{
	if (*dest)
		WStrLenSet(*dest, 0);
	if (src && src[0] && srclen > 0)
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

LibAPI(MgErr) MultiByteToWideString(const LStrHandle src, uInt32 codePage, WStrHandle *dest)
{
	return MultiByteCStrToWideString(LStrBuf(*src), LStrLen(*src), codePage, dest);
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
	return WideCStrToMultiByte(WStrBuf(src), WStrLen(src), dest, 0, codePage, defaultChar, defaultCharWasUsed);
}

LibAPI(MgErr) WideCStrToMultiByte(const wchar_t *src, int32 srclen, LStrHandle *dest, int32 offset, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	if (defaultCharWasUsed)
		*defaultCharWasUsed = LV_FALSE;
	if (dest && *dest && **dest)
		LStrLen(**dest) = 0;
	
	if (srclen < 0)
		srclen = (int32)wcslen(src);
	
	if (srclen > 0)
	{
		int32 numBytes = 0;
		MgErr err = WideCStrToMultiByteBuf(src, srclen, NULL, &numBytes, codePage, defaultChar, defaultCharWasUsed);
		if (!err && numBytes > 0)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)dest, offset + numBytes + 1);
			if (!err)
			{
				err = WideCStrToMultiByteBuf(src, srclen, LStrBufH(*dest) + offset, &numBytes, codePage, defaultChar, defaultCharWasUsed);
				TerminateLStr(dest, offset + numBytes);
			}
		}
		return err;
	}
	return mgNoErr;
}

static MgErr WideCStrToMultiByteBuf(const wchar_t *src, int32 srclen, UPtr ptr, int32 *length, uInt32 codePage, char defaultChar, LVBoolean *defaultCharWasUsed)
{
	MgErr err = mgNoErr;

	if (defaultCharWasUsed)
		*defaultCharWasUsed = LV_FALSE;

	if (srclen < 0)
		srclen = (int32)wcslen(src);

	if (srclen > 0)
	{
#if Win32 
		BOOL defUsed = FALSE;
		BOOL utfCp = ((codePage == CP_UTF8) || (codePage == CP_UTF7));
		int32 numBytes = WideCharToMultiByte(codePage, 0, src, srclen, (LPSTR)ptr, *length, utfCp ? NULL : &defaultChar, utfCp ? NULL : &defUsed);
		if (numBytes > 0)
		{ 
			*length = numBytes;
			if (defaultCharWasUsed)
				*defaultCharWasUsed = (LVBoolean)(defUsed != FALSE);
		}
#elif MacOSX
		CFStringRef cfpath = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, src, srclen, kCFAllocatorNull);
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
					CFIndex numBytes = *length;
					if (CFStringGetBytes(cfpath2, CFRangeMake(0, CFStringGetLength(cfpath2)), encoding, defaultChar, false, (UInt8*)LStrBuf(**dest), numBytes, &numBytes) > 0)
					{
						*length = (int32)numBytes;
					}
				}
				CFRelease(cfpath2);
			}
		}
#elif Unix
		err = unix_convert_wtomb(src, srclen, ptr, length, codePage, defaultChar, defaultCharWasUsed);
#endif
	}
	return err;
}
