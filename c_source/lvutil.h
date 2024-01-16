/*
   lvutil.h -- support functions for LabVIEW integration

   Copyright (C) 2002-2024 Rolf Kalbermatter

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
#ifndef _lvUtil_H
#define _lvUtil_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvtypes.h"
#include "lwstr.h"

/* Our exported functions */
/**************************/

LibAPI(MgErr) InitializeFileFuncs(LStrHandle filefunc_def);
LibAPI(MgErr) InitializeStreamFuncs(LStrHandle  filefunc_def, LStrHandle *memory);

LibAPI(MgErr) ZeroTerminateLString(LStrHandle *dest);

typedef union
{
	int64 q;
#if BigEndian
	struct
	{
		int32 hi;
		uInt32 lo;
	} l;
#else
	struct
	{
		uInt32 lo;
		int32 hi;
	} l;
#endif
} FileOffset;

/* Copy and Rename flags */
#define kFileOpReplaceNever     0
#define kFileOpReplaceAlways    1
#define kFileOpReplaceIfNewer   2
#define kFileOpReplaceMask		3
#define kFileOpNoRecursion		4
#define kFileOpMaintainAttrs	8
#define kFileOpShallowCopy		16

/* Delete flags */
#define kDelIgnoreReadOnly		1
#define kDelMoveToTrash			2
#define kDelNoRecursion			4

/* ListDirectory flags */
#define kListArchiveNone		0
#define kListArchiveTopLvl		1
#define kListArchiveAll			2


/* Check if the file pathName points to has a resource fork, only on Mac */
LibAPI(MgErr) LVFile_HasResourceFork(LWPathHandle *pathName, LVBoolean *hasResFork, FileOffset *size);

/* List the directory contents with an additional array with flags and file type for each file in the names array */
LibAPI(MgErr) LVFile_ListDirectory(LWPathHandle *folderPath, LStrArrHdl *nameArr, FileTypeArrHdl *typeArr, uInt32 llbMode, int32 resolveDepth);

LibAPI(MgErr) LVFile_Delete(LWPathHandle *path, uInt32 flags);
LibAPI(MgErr) LVFile_Rename(LWPathHandle *pathFrom, LWPathHandle *pathTo, uInt32 flags);
LibAPI(MgErr) LVFile_Copy(LWPathHandle *pathFrom, LWPathHandle *pathTo, uInt32 flags);
LibAPI(MgErr) LVFile_CreateDirectories(LWPathHandle *path, int16 permissions);

LibAPI(void) ATimeToFileTime(ATime128 *pt, uInt64 *pft);
LibAPI(void) FileTimeToATime(uInt64 *pft, ATime128 *pt);

/* Windows portion of the flags parameter */
#define kWinFileInfoReadOnly             0x00000001  
#define kWinFileInfoHidden               0x00000002  
#define kWinFileInfoSystem               0x00000004  
#define kWinFileInfoDirectory            0x00000010  
#define kWinFileInfoArchive              0x00000020  
#define kWinFileInfoDevice               0x00000040  
#define kWinFileInfoNormal               0x00000080  
#define kWinFileInfoTemporary            0x00000100  
//#define kWinFileInfoSparseFile           0x00000200  
#define kWinFileInfoReparsePoint         0x00000400  
#define kWinFileInfoCompressed           0x00000800  
#define kWinFileInfoOffline              0x00001000  
#define kWinFileInfoNotIndexed           0x00002000  
//#define kWinFileInfoEncrypted            0x00004000  

/* Mac extended flags */
#define kMacFileInfoNoDump               0x00000001	   /* do not dump file, opposite from Windows archive flag */
#define	kMacFileInfoImmutable		     0x00000002	   /* file may not be changed */
#define kMacFileInfoCompressed           0x00000020    /* file is hfs-compressed (Mac OS X 10.6+) */
//#define kMacFileInfoSystem             0x00000080    /* Windows system file bit */
//#define kMacFileInfoSparseFile         0x00000100    /* sparse file */
//#define kMacFileInfoOffline	         0x00000200    /* file is offline */
//#define kMacFileInfoArchive            0x00000800    /* file needs to be archived */
#define kMacFileInfoHidden               0x00008000    /* hint that this item should not be displayed in a GUI (Mac OS X 10.5+) */

typedef struct {				/* off */
	ResType type;				/*  0: handled by LabVIEW Type & Creator */
	ResType creator;			/*  4: handled by LabVIEW Type & Creator */
	uInt32 uid;					/*  8: Unix user id */
	uInt32 gid;					/* 12: Unix group id */
	uInt64 size;				/* 16: file size or file count for directories */
	uInt64 rfSize;				/* 24: resource fork size, 0 on non MacOS platforms */
	ATime128 cDate;				/* 32: Creation date */
	ATime128 mDate;				/* 48: Modification date */
	ATime128 aDate;				/* 64: ast access date */
	uInt16 winFlags;			/* 80: Windows compatible flags */
	uInt16 unixFlags;			/* 82: Unix compatible flags */
	uInt32 macFlags;			/* 84: MacOSX extra file flags */
	uInt32 fileType;			/* 88: LabVIEW file type flags */
	uInt32 nLink;				/* 92: LabVIEW file type flags */
} FileInfoRec, *FileInfoPtr;	/* 96: Total length */

/* Retrieve file information from the path */
LibAPI(MgErr) LVFile_FileInfo(LWPathHandle *path, uInt8 write, FileInfoPtr fileInfo);

/* Creation flags */
#define kLinkSoft       0x00
#define kLinkHard       0x01
#define kLinkDir		0x02

/* Resolution flags */
#define kRecursive		-1

/* Create and read a link */
LibAPI(MgErr) LVFile_CreateLink(LWPathHandle *path, LWPathHandle *target, uInt32 flags);
LibAPI(MgErr) LVFile_ReadLink(LWPathHandle *path, LWPathHandle *target, int32 resolveDepth, int32 *resolveCount, uInt32 *fileFlags);

enum { /* values for rsrc parameter */
	kOpenFileRsrcData,
	kOpenFileRsrcResource,
	kOpenFileRsrcInfo,
	kOpenFileRsrcDesktop,
	kOpenFileRsrcIndex,
	kOpenFileRsrcComment
};

LibAPI(MgErr) LVFile_OpenFile(LVRefNum *refnum, LWPathHandle *path, uInt32 rsrc, uInt32 openMode, uInt32 accessMode, uInt32 denyMode, uInt32 flags);
LibAPI(MgErr) LVFile_CloseFile(LVRefNum *refnum);
LibAPI(MgErr) LVFile_GetSize(LVRefNum *refnum, LVBoolean remain, FileOffset *size);
LibAPI(MgErr) LVFile_SetSize(LVRefNum *refnum, FileOffset *size);
LibAPI(MgErr) LVFile_GetFilePos(LVRefNum *refnum, FileOffset *offset);
LibAPI(MgErr) LVFile_SetFilePos(LVRefNum *refnum, uInt32 mode, FileOffset *offset);
LibAPI(MgErr) LVFile_LockFile(LVRefNum *refnum, uInt32 mode, FileOffset *offset, FileOffset *length);
LibAPI(MgErr) LVFile_Read(LVRefNum *refnum, uInt32 inCount, uInt32 *outCount, UPtr buffer);
LibAPI(MgErr) LVFile_Write(LVRefNum *refnum, uInt32 inCount, uInt32 *outCount, UPtr buffer);

/* For the refnum auto cleanup */
MgErr lvFile_CloseFile(FileRefNum refnum);

#ifdef __cplusplus
}
#endif

#endif /* _lvUtil_H */
