/*
   refnum.h -- module to handle refnums for our library objects

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
#include "zlib.h"
#include "lwstr.h"
#include "lvutil.h"
#include "zip.h"
#include "unzip.h"
#include "refnum.h"

typedef struct
{
	union
	{
		void *node;
	} u;
	uInt32 magic;
	LVRefNum refnum;
} FileNode;

MgErr LibToMgErr(int err)
{
	if (err < 0)
	{
		switch (err)
		{
			case Z_ERRNO:
			case UNZ_BADZIPFILE:
				return fNotFound;
			case UNZ_END_OF_LIST_OF_FILE:
				return fEOF;
			case UNZ_PARAMERROR:
				return mgArgErr;
			case UNZ_INTERNALERROR:
			case UNZ_CRCERROR:
				return fIOErr;
		}
	}
	return mgNoErr;
}

static MagicCookieJar gCookieJar = NULL;

static int32 lvzlipCleanup(UPtr ptr)
{
	FileNode *pNode = (FileNode*)ptr;
	MgErr err =	MCDisposeCookie(gCookieJar, pNode->refnum, (MagicCookieInfo)&pNode);
	if (!err && pNode->u.node)
	{
		LStrHandle stream = NULL;
		switch (pNode->magic)
		{
			case UnzMagic:
				err = LibToMgErr(unzClose2(pNode->u.node, (voidpf *)&stream));
				break;
			case ZipMagic:
				err = LibToMgErr(zipCloseEx(pNode->u.node, NULL, (voidpf *)&stream));
				break;
			case FileMagic:
				err = lvFile_CloseFile(pNode->u.node);
				break;
		}
		if (stream)
			DSDisposeHandle((UHandle)stream);
	}
	return err;
}

MgErr lvzlibCreateRefnum(void *node, LVRefNum *refnum, uInt32 magic, LVBoolean autoreset)
{
	FileNode *pNode;
	if (!gCookieJar)
	{
		gCookieJar = MCNewBigJar(sizeof(FileNode*));
		if (!gCookieJar)
			return mFullErr;
	}
	pNode = (FileNode*)DSNewPClr(sizeof(FileNode));
	if (!pNode)
		return mFullErr;

	*refnum = MCNewCookie(gCookieJar, (MagicCookieInfo)&pNode);
	if (*refnum)
	{
		pNode->u.node = node;
		pNode->magic = magic;
		pNode->refnum = *refnum;
		RTSetCleanupProc(lvzlipCleanup, (UPtr)pNode, autoreset ? kCleanOnIdle : kCleanExit);
	}
	else
	{
		DSDisposePtr((UPtr)pNode);
	}
	return *refnum ? mgNoErr : mFullErr;
}

MgErr lvzlibGetRefnum(LVRefNum *refnum, void **node, uInt32 magic)
{
	FileNode *pNode = NULL;
	MgErr err = MCGetCookieInfo(gCookieJar, *refnum, (MagicCookieInfo)&pNode);
	if (!err)
	{
		if (pNode->magic == magic)
		{
			if (node)
				*node = pNode->u.node;
		}
		else
		{
			err = mgArgErr;
		}
	}
	return err;
}

MgErr lvzlibDisposeRefnum(LVRefNum *refnum, void **node, uInt32 magic)
{
	FileNode *pNode = NULL;
	MgErr err = MCGetCookieInfo(gCookieJar, *refnum, (MagicCookieInfo)&pNode);
	if (!err)
	{
		if (pNode->magic == magic)
		{
			err = MCDisposeCookie(gCookieJar, *refnum, (MagicCookieInfo)&pNode);
			if (!err)
			{
				RTSetCleanupProc(lvzlipCleanup, (UPtr)pNode, kCleanRemove);
				if (node)
					*node = pNode->u.node;
				DSDisposePtr((UPtr)pNode);
			}
		}
		else
		{
			err = mgArgErr;
		}
	}
	return err;
}
