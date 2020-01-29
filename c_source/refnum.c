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
		switch (pNode->magic)
		{
			case UnzMagic:
				err = LibToMgErr(unzClose(pNode->u.node));
				break;
			case ZipMagic:
				err = LibToMgErr(zipClose(pNode->u.node, NULL));
				break;
			case FileMagic:
				err = lvfile_CloseFile(pNode->u.node);
				break;
		}
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
	FileNode *pNode;
	MgErr err = MCGetCookieInfo(gCookieJar, *refnum, (MagicCookieInfo)&pNode);
	if (!err)
	{
		if (pNode->magic == magic)
		{
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
	FileNode *pNode;
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
