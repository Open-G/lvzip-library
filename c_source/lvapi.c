#define ZLIB_INTERNAL
#include "zlib.h"
#include "lvutil.h"
#include "zip.h"
#include "unzip.h"
#include <string.h>
#include "lvapi.h"

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
                             zlib_filefunc64_def* filefuncs, LVRefNum *refnum)
{
	zipcharpc comment;
	zipFile node = zipOpen2_64(pathname, append, &comment, filefuncs);

	*refnum = kNotARefNum;
	if (*globalcomment)
		LStrLen(**globalcomment) = 0;

	if (node)
	{
		MgErr err = lvzlibCreateRefnum(node, refnum, ZipMagic, LV_FALSE);
		if (!err && comment)
		{
			int32 len = (int32)strlen(comment);
			err = NumericArrayResize(uB, 1, (UHandle*)globalcomment, len);
			if (!err)
			{
				MoveBlock((ConstUPtr)comment, LStrBuf(**globalcomment), len);
				LStrLen(**globalcomment) = len;
			}
			else
			{
				lvzlibDisposeRefnum(refnum, NULL, ZipMagic);		
			}
		}
		if (err)
		{
			zipClose(node, NULL);
		}
		return err;
	}
	return fNotFound;
}

LibAPI(MgErr) lvzlib_zipOpenNewFileInZip(LVRefNum *refnum, LStrHandle filename, const zip_fileinfo* zipfi,
						   const LStrHandle extrafield_local, const LStrHandle extrafield_global,
						   LStrHandle comment, int method, int level, int raw, int windowBits,
						   int memLevel, int strategy, const char* password, uLong crcForCrypting, uLong flags, int zip64)
{
	zipFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, ZipMagic);
	if (!err)
	{
		err = ZeroTerminateLString(&filename);
		if (!err)
		{
			err = ZeroTerminateLString(&comment);
			if (!err)
			{
				err = LibToMgErr(zipOpenNewFileInZip4_64(node, (const char*)LStrBufH(filename), zipfi,
						         LStrBufH(extrafield_local), LStrLenH(extrafield_local),
								 LStrBufH(extrafield_global), LStrLenH(extrafield_global),
								 (const char*)LStrBufH(comment), method, level, raw, windowBits, memLevel,
								 strategy, password[0] ? password : NULL, crcForCrypting, VERSIONMADEBY, flags, zip64));
			}
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_zipWriteInFileInZip(LVRefNum *refnum, const LStrHandle buffer)
{
	zipFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, ZipMagic);
	if (!err)
	{
		int retval = zipWriteInFileInZip(node, LStrBuf(*buffer), LStrLen(*buffer));
		if (retval < 0)
			return LibToMgErr(retval);
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
        
        *refnum = kNotARefNum;

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

/****************************************************************************************************
 *
 * Opens a ZIP archive for reading
 *
 * Parameters:
 *  pathname: The path and filename of the archive to open for a disk based archive file
 *  filefuncs: A pointer to a record containing the function pointers to use for access to the archive 
 *  refnum: An archive extraction file reference
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_unzOpen(const void *pathname, zlib_filefunc64_def* filefuncs, LVRefNum *refnum)
{
	unzFile node = unzOpen2_64(pathname, filefuncs);
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

/****************************************************************************************************
 *
 * Closes a ZIP archive that was opened for reading
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_unzClose(LVRefNum *refnum)
{
	unzFile node;
	MgErr err = lvzlibDisposeRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		*refnum = kNotARefNum;
		err = LibToMgErr(unzClose(node));
	}
	return err;
}

/****************************************************************************************************
 *
 * Read the global commen and number of entries in the archive
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *  comment: The LabVIEW string to receive the global comment
 *  nEntry: The number of file entries in the archive
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_unzGetGlobalInfo32(LVRefNum *refnum, LStrHandle *comment, uInt32 *nEntry)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		unz_global_info pglobal_info;
		err = LibToMgErr(unzGetGlobalInfo(node, &pglobal_info));
		if (!err)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)comment, pglobal_info.size_comment);
			if (!err)
			{
				err = LibToMgErr(unzGetGlobalComment(node, (char*)LStrBuf(**comment), pglobal_info.size_comment));
				if (!err)
				{
					LStrLen(**comment) = pglobal_info.size_comment;
					*nEntry = pglobal_info.number_entry;
				}
			}
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
			err = NumericArrayResize(uB, 1, (UHandle*)comment, pglobal_info.size_comment);
			if (!err)
			{
				err = LibToMgErr(unzGetGlobalComment(node, (char*)LStrBuf(**comment), pglobal_info.size_comment));
				if (!err)
				{
					LStrLen(**comment) = pglobal_info.size_comment;
					*nEntry = pglobal_info.number_entry;
				}
			}
		}
	}
	return err;
}

/****************************************************************************************************
 *
 * Position the current file pointer to the file with fileName
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *  fileName: The name of the file in the archive. This is the full path in unix file notation
 *            including the directory levels. This string is really a byte sequence that will
 *            be compared as binary byte stream to the internal names. No encoding translation
 *            will be done in either direction.
 *  caseSensivity: Indicates if the filename comparison should be done case sensitive or not.
 *                 Note that depending on the actual encoding of the string this might not
 *                 always work as intended for case insensitive comparison since only the normal
 *                 ASCII 7Bit alpha characters will be compared case insensitive, not any extended
 *                 characters or multibyte characters.
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_unzLocateFile(LVRefNum *refnum, LStrHandle fileName, int iCaseSensitivity)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = ZeroTerminateLString(&fileName);
		if (!err)
		{
			err = LibToMgErr(unzLocateFile(node, (const char*)LStrBufH(fileName), iCaseSensitivity));
		}
	}
	return err;
}

/****************************************************************************************************
 *
 * Retrieve the file information for the currently selected file entry
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *  pfile_info: A structure receiving different values and flags about the file entry.
 *  fileName: The name of the file in the archive. This is the full path in unix file notation
 *            including the directory levels. This string is really a byte sequence that could
 *            be either in the OEM codepage or in UTF8 depending on the according flag
 *  extraField: optional extra field information
 *  comment: An optional comment entry for the file entry. This is encoded in the same way as the fileName
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_unzGetCurrentFileInfo32(LVRefNum *refnum, unz_file_info *pfile_info, LStrHandle *fileName,
											 LStrHandle *extraField, LStrHandle *comment)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGetCurrentFileInfo(node, pfile_info, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)extraField, pfile_info->size_file_extra);
			if (!err)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)fileName, pfile_info->size_filename);
				if (!err)
				{
					err = NumericArrayResize(uB, 1, (UHandle*)comment, pfile_info->size_file_comment);
					if (!err)
					{
						err = LibToMgErr(unzGetCurrentFileInfo(node, pfile_info, (char*)LStrBuf(**fileName), pfile_info->size_filename,
								LStrBuf(**extraField), pfile_info->size_file_extra, (char*)LStrBuf(**comment), pfile_info->size_file_comment));
						if (!err)
						{
							LStrLen(**extraField) = pfile_info->size_file_extra;
							LStrLen(**fileName) = pfile_info->size_filename;
							LStrLen(**comment) = pfile_info->size_file_comment;
						}
					}
				}
			}
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGetCurrentFileInfo64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName,
											 LStrHandle *extraField, LStrHandle *comment)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGetCurrentFileInfo64(node, pfile_info, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)extraField, pfile_info->size_file_extra);
			if (!err)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)fileName, pfile_info->size_filename);
				if (!err)
				{
					err = NumericArrayResize(uB, 1, (UHandle*)comment, pfile_info->size_file_comment);
					if (!err)
					{
						err = LibToMgErr(unzGetCurrentFileInfo64(node, pfile_info, (char*)LStrBuf(**fileName), pfile_info->size_filename,
                                 LStrBuf(**extraField), pfile_info->size_file_extra, (char*)LStrBuf(**comment), pfile_info->size_file_comment));
						if (!err)
						{
							LStrLen(**extraField) = pfile_info->size_file_extra;
							LStrLen(**fileName) = pfile_info->size_filename;
							LStrLen(**comment) = pfile_info->size_file_comment;
						}
					}
				}
			}
		}
	}
	return err;
}

/****************************************************************************************************
 *
 * Opens the file the file pointer currently points to
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *  method: 
 *  level: 
 *  raw: Indicates if the file should be opened in raw mode which retrieves the raw bytes without
 *       attempting to expand them or decode any password protection.
 *  password: The password to use to open the file. Pass NULL for raw retrieval or if the file was
 *            not stored with a password.
 *
 ****************************************************************************************************/
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

/****************************************************************************************************
 *
 * Retrieve the local extrafield information for the current file
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *  extra: Receives the data of the local extrafield information as a byte stream.
 *
 ****************************************************************************************************/
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
				if (!err)
					LStrLen(**extra) = len;
			}
			return err;
		}
		err = LibToMgErr(len);
	}
	return err;
}

/****************************************************************************************************
 *
 * Retrieve a block of data from the currently open file entry in the archive
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *  buffer: Receives the data of the file. The function retrieves as much data as the buffer is long
 *          or less if the file data is smaller.
 *
 ****************************************************************************************************/
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

/****************************************************************************************************
 *
 * Closes the current file entry in the archive
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *
 ****************************************************************************************************/
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

/****************************************************************************************************
 *
 * Positions the pointer to the first file entry in the archive
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *
 ****************************************************************************************************/
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

/****************************************************************************************************
 *
 * Positions the pointer to the next file entre in the archive
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *
 ****************************************************************************************************/
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

/****************************************************************************************************
 *
 * Low level function to position the current read pointer at a specific location inside the archive
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *  pos: The position to move too
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_unzGoToFilePos32(LVRefNum *refnum, unz_file_pos *pos)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGoToFilePos(node, pos));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGoToFilePos64(LVRefNum *refnum, unz64_file_pos *pos)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGoToFilePos64(node, pos));
	}
	return err;
}

/****************************************************************************************************
 *
 * Low level function to retrieve the read pointer at a current location inside the archive
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *  pos: Returns the current position
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_unzGetFilePos32(LVRefNum *refnum, unz_file_pos *pos)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGetFilePos(node, pos));
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGetFilePos64(LVRefNum *refnum, unz64_file_pos *pos)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGetFilePos64(node, pos));
	}
	return err;
}

