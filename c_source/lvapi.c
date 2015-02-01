#define ZLIB_INTERNAL
#include "zlib.h"
#include "lvutil.h"
#include "zip.h"
#include "unzip.h"
#include <string.h>

#ifndef VERSIONMADEBY
# define VERSIONMADEBY   (0x0) /* platform depedent */
#endif

#define FLAGS_UTF8  0x0800

static MgErr LibToMgErr(int err)
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

typedef struct
{
	union
	{
		voidp node;
	} u;
	uInt32 magic;
	LVRefNum refnum;
} FileNode;
	
#define ZipMagic RTToL('Z','I','P','!')
#define UnzMagic RTToL('U','N','Z','!')

static MagicCookieJar gCookieJar = NULL;

#if 0
LibAPI(const char *) lvzlib_disposeRefnum(uptr_t *refnum)
{

}
#endif
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
		}
	}
	return err;
}

static MgErr lvzlibCreateRefnum(voidp node, LVRefNum *refnum, uInt32 magic, LVBoolean autoreset)
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

static MgErr lvzlibGetRefnum(LVRefNum *refnum, voidp *node, uInt32 magic)
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

static MgErr lvzlibDisposeRefnum(LVRefNum *refnum, voidp *node, uInt32 magic)
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

LibAPI(const char *) lvzlib_zlibVersion(void)
{
	return zlibVersion();
}

LibAPI(int) lvzlib_compress(Bytef *dest, uLongf *destLen,
                             const Bytef *source, uLong sourceLen, int level)
{
	return compress2(dest, destLen, source, sourceLen, level);
}

LibAPI(int) lvzlib_uncompress(Bytef *dest, uLongf *destLen,
                             const Bytef *source, uLong sourceLen)
{
	return uncompress(dest, destLen, source, sourceLen);
}

LibAPI(uInt32) lvzlib_crc32(uInt32 crc, const Bytef *buf, uInt32 len)
{
	return crc32(crc, buf, len);
}

LibAPI(MgErr) lvzlib_zipOpen(const void *pathname, int append, LStrHandle *globalcomment,
                             zlib_filefunc64_def* pzlib_filefunc_def, LVRefNum *refnum)
{
	zipcharpc comment;
	zipFile node = zipOpen2_64(pathname, append, &comment, pzlib_filefunc_def);
	*refnum = kNotARefNum;
	if (*globalcomment)
		LStrLen(**globalcomment) = 0;
	if (node)
	{
		MgErr err = lvzlibCreateRefnum(node, refnum, ZipMagic, LV_FALSE);
		if (err)
		{
			zipClose(node, NULL);
		}
		else if (comment)
		{
			err = ConvertCString((ConstCStr)comment, (int32)strlen(comment), CP_OEMCP, globalcomment, CP_ACP, 0, NULL);
			if (err)
			{
				zipClose(node, NULL);
				lvzlibDisposeRefnum(refnum, NULL, ZipMagic);		
			}
		}
		return err;
	}
	return fNotFound;
}

static uInt32 determine_codepage(uLong *flags, LStrHandle string)
{
	uInt32 cp = Hi16(*flags);
	if (!cp)
	{
		int32 i = LStrLen(*string) - 1;
		UPtr ptr = LStrBuf(*string);

		cp = CP_OEMCP;
		// No codepage defined, try to find out if we have extended characters and set UTF_* in that case
		for (; i >= 0; i--)
		{
			if (ptr[i] >= 0x80)
			{
				cp = CP_UTF8;
				break;
			}
		}
	}
	return cp;
}

LibAPI(MgErr) lvzlib_zipOpenNewFileInZip(LVRefNum *refnum, const LStrHandle filename, const zip_fileinfo* zipfi,
						   const LStrHandle extrafield_local, const LStrHandle extrafield_global,
						   const LStrHandle comment, int method, int level, int raw, int windowBits,
						   int memLevel, int strategy, const char* password, uLong crcForCrypting, uLong flags, int zip64)
{
	LStrHandle tempfilename = NULL, tempcomment = NULL;
	uInt32 cp1 = determine_codepage(&flags, filename); 
	uInt32 cp2 = determine_codepage(&flags, comment);
	MgErr err;

	if (cp1 == CP_UTF8 || cp2 == CP_UTF8)
	{
		cp1 = cp2 = CP_UTF8;
		flags = Lo16(flags) | FLAGS_UTF8;
	}
	else
	{
		flags = Lo16(flags) & ~FLAGS_UTF8;
	}

	err = ConvertLPath(filename, CP_ACP, &tempfilename, cp1, 0, NULL);
	if (!err && LStrLen(*comment))
	{
	    err = ConvertLString(comment, CP_ACP, &tempcomment, cp1, 0, NULL);
	}
	if (!err)
	{
		zipFile node;
		err = lvzlibGetRefnum(refnum, &node, ZipMagic);
		if (!err)
		{
			err = LibToMgErr(zipOpenNewFileInZip4_64(node, (const char*)LStrBufH(tempfilename), zipfi,
			                 LStrBufH(extrafield_local), LStrLenH(extrafield_local),
	                         LStrBufH(extrafield_global), LStrLenH(extrafield_global),
							 (const char*)LStrBufH(tempcomment), method, level, raw, windowBits, memLevel,
							 strategy, password[0] ? password : NULL, crcForCrypting, VERSIONMADEBY, flags, zip64));
		}
	}
	if (tempcomment)
		DSDisposeHandle((UHandle)tempcomment);
	if (tempfilename)
		DSDisposeHandle((UHandle)tempfilename);
	return err;
}

LibAPI(MgErr) lvzlib_zipWriteInFileInZip(LVRefNum *refnum, const LStrHandle buffer)
{
	zipFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, ZipMagic);
	if(!err)
	{
		int retval = zipWriteInFileInZip(node, LStrBuf(*buffer), LStrLen(*buffer));
		if (retval < 0)
			return LibToMgErr(retval);
		LStrLen(*buffer) = retval;
	}
	return err;
}

LibAPI(MgErr) lvzlib_zipCloseFileInZipRaw32(LVRefNum *refnum, uInt32 uncompressedSize, uInt32 crc32)
{
	zipFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, ZipMagic);
	if (!err)
	{
		return LibToMgErr(zipCloseFileInZipRaw64(node, uncompressedSize, crc32));
	}
	return err;
}

LibAPI(MgErr) lvzlib_zipCloseFileInZipRaw64(LVRefNum *refnum, uInt64 uncompressedSize, uInt32 crc32)
{
	zipFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, ZipMagic);
	if (!err)
	{
		return LibToMgErr(zipCloseFileInZipRaw64(node, uncompressedSize, crc32));
	}
	return err;
}

LibAPI(MgErr) lvzlib_zipClose(LVRefNum *refnum, const LStrHandle globalComment, LStrHandle *stream)
{
	zipFile node;
	MgErr err = lvzlibDisposeRefnum(refnum, &node, ZipMagic);
	if (!err)
	{
		int retval;
		LStrHandle comment = NULL;
		if (globalComment && LStrLen(*globalComment) > 0)
		{
			err = ConvertLString(globalComment, CP_ACP, &comment, CP_OEMCP, 0, NULL);
			if (err)
				comment = globalComment;
		}
		retval = zipClose2(node, (const char*)LStrBufH(comment), stream);
		if (!err && retval)
			err = LibToMgErr(retval);
		DSDisposeHandle((UHandle)comment);
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzOpen(const void *pathname, zlib_filefunc64_def* pzlib_filefunc_def, LVRefNum *refnum)
{
	unzFile node = unzOpen2_64(pathname, pzlib_filefunc_def);
	*refnum = kNotARefNum;
	if (node)
	{
		MgErr err = lvzlibCreateRefnum(node, refnum, UnzMagic, LV_FALSE);
		if (err)
		{
			unzClose(node);
		}
		return err;
	}
	return fNotFound;
}

LibAPI(MgErr) lvzlib_unzClose(LVRefNum *refnum)
{
	unzFile node;
	MgErr err = lvzlibDisposeRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzClose(node));
	}
	return err;
}

static MgErr lvzlib_unzGetGlobalComment(unzFile node, LStrHandle *comment, uInt32 len)
{
	void *temp = malloc(len);
	if (temp)
	{
		MgErr err = LibToMgErr(unzGetGlobalComment(node, temp, len));
		if (!err)
		{
			err = ConvertCString(temp, StrLen(temp), CP_OEMCP, comment, CP_ACP, 0, NULL);
		}
		free(temp);
		return err;
	}
	return mFullErr;
}

LibAPI(MgErr) lvzlib_unzGetGlobalInfo32(LVRefNum *refnum, LStrHandle *comment, uInt32 *nEntry)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		unz_global_info pglobal_info;
		MgErr err = LibToMgErr(unzGetGlobalInfo(node, &pglobal_info));
		if (!err)
		{
			*nEntry = pglobal_info.number_entry;
			err = lvzlib_unzGetGlobalComment(node, comment, pglobal_info.size_comment + 1);
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGetGlobalInfo64(LVRefNum *refnum, LStrHandle *comment, uInt64 *nEntry)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		unz_global_info64 pglobal_info;
		err = LibToMgErr(unzGetGlobalInfo64(node, &pglobal_info));
		if (!err)
		{
			*nEntry = pglobal_info.number_entry;
			err = lvzlib_unzGetGlobalComment(node, comment, pglobal_info.size_comment + 1);
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzLocateFile(LVRefNum *refnum, const LStrHandle fileName, int iCaseSensitivity)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		unz_file_info pfile_info;
		err = LibToMgErr(unzGetCurrentFileInfo(node, &pfile_info, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			uInt32 cp = pfile_info.flag & FLAGS_UTF8 ? CP_UTF8 : CP_OEMCP; 
			LStrHandle name = NULL;
			err = ConvertLString(fileName, CP_ACP, &name, cp, 0, NULL);
			if (!err)
			{
				err = LibToMgErr(unzLocateFile(node, (const char*)LStrBufH(name), iCaseSensitivity));
				DSDisposeHandle((UHandle)name);
			}
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGetCurrentFileInfo32(LVRefNum *refnum, unz_file_info *pfile_info, LVBoolean getUTF,
                                             LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGetCurrentFileInfo(node, pfile_info, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			char *szFileName = malloc(pfile_info->size_filename + 1);
			if (szFileName)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)extraField, pfile_info->size_file_extra);
				if (!err)
				{
					char *szComment = malloc(pfile_info->size_file_comment + 1);
					if (szComment)
					{
						err = LibToMgErr(unzGetCurrentFileInfo(node, pfile_info, szFileName, pfile_info->size_filename + 1,
                                 LStrBuf(**extraField), pfile_info->size_file_extra, szComment, pfile_info->size_file_comment + 1));
						if (!err)
						{
							uInt32 cp = pfile_info->flag & FLAGS_UTF8 ? CP_UTF8 : CP_OEMCP; 
							err = ConvertCString((ConstCStr)szComment, (int32)strlen(szComment), cp, comment, getUTF ? CP_UTF8 : CP_ACP, '?', NULL);
							if (!err)
							{
								err = ConvertCPath((ConstCStr)szFileName, (int32)strlen(szFileName), cp, fileName, getUTF ? CP_UTF8 : CP_ACP, '?', NULL);
							}
							LStrLen(**extraField) = pfile_info->size_file_extra;
						}
						free(szComment);
					}
					else
					{
						err = mFullErr;
					}
				}
				free(szFileName);
			}	
			else
			{
				err = mFullErr;
			}
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGetCurrentFileInfo64(LVRefNum *refnum, unz_file_info64 *pfile_info, LVBoolean getUTF,
                                             LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGetCurrentFileInfo64(node, pfile_info, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			char *szFileName = malloc(pfile_info->size_filename + 1);
			if (szFileName)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)extraField, pfile_info->size_file_extra);
				if (!err)
				{
					char *szComment = malloc(pfile_info->size_file_comment + 1);
					if (szComment)
					{
						err = LibToMgErr(unzGetCurrentFileInfo64(node, pfile_info, szFileName, pfile_info->size_filename + 1,
                                 LStrBuf(**extraField), pfile_info->size_file_extra, szComment, pfile_info->size_file_comment + 1));
						if (!err)
						{
							uInt32 cp = pfile_info->flag & FLAGS_UTF8 ? CP_UTF8 : CP_OEMCP; 
							err = ConvertCString((ConstCStr)szComment, (int32)strlen(szComment), cp, comment, getUTF ? CP_UTF8 : CP_ACP, '?', NULL);
							if (!err)
							{
								err = ConvertCPath((ConstCStr)szFileName, (int32)strlen(szFileName), cp, fileName, getUTF ? CP_UTF8 : CP_ACP, '?', NULL);
							}
							LStrLen(**extraField) = pfile_info->size_file_extra;
						}
						free(szComment);
					}
					else
					{
						err = mFullErr;
					}
				}
				free(szFileName);
			}	
			else
			{
				err = mFullErr;
			}
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzOpenCurrentFile(LVRefNum *refnum, int32* method, int32* level, int16 raw, const char* password)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzOpenCurrentFile3(node, method, level, raw, password[0] ? password : NULL));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGetLocalExtrafield(LVRefNum *refnum, LStrHandle *extra)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		int len = unzGetLocalExtrafield(node, NULL, 0);
		if (len > 0)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)extra, len);
			if (!err)
			{
				err = LibToMgErr(unzGetLocalExtrafield(node, LStrBuf(**extra), len));
				LStrLen(**extra) = len;
			}
			return err;
		}
		err = LibToMgErr(len);
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzReadCurrentFile(LVRefNum *refnum, LStrHandle buffer)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		int retval = unzReadCurrentFile(node, LStrBuf(*buffer), LStrLen(*buffer));
		if (retval < 0)
			return LibToMgErr(retval);
		LStrLen(*buffer) = retval;
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzCloseCurrentFile(LVRefNum *refnum)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzCloseCurrentFile(node));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGoToFilePos32(LVRefNum *refnum, unz_file_pos *rec)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGoToFilePos(node, rec));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGetFilePos32(LVRefNum *refnum, unz_file_pos *rec)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGetFilePos(node, rec));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGoToFilePos64(LVRefNum *refnum, unz64_file_pos *rec)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGoToFilePos64(node, rec));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGetFilePos64(LVRefNum *refnum, unz64_file_pos *rec)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGetFilePos64(node, rec));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGoToFirstFile(LVRefNum *refnum)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGoToFirstFile(node));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGoToNextFile(LVRefNum *refnum)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGoToNextFile(node));
	}
	return err;
}
