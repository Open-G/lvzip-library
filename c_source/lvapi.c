/* 
   lvapi.c -- LabVIEW interface for LabVIEW ZIP library

   Copyright (C) 2009-2023 Rolf Kalbermatter

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
#include <string.h>
#define ZLIB_INTERNAL
#include "zlib.h"
#include "crypt.h"
#include "lvutil.h"
#include "zip.h"
#include "unzip.h"
#include "lvapi.h"
#include "iomem.h"
#include "refnum.h"
#include "Resource.h"

#ifdef HAVE_BZIP2
#include "bzip2/bzlib.h"
#endif

#ifndef VERSIONMADEBY
# define VERSIONMADEBY   (0x0) /* platform depedent */
#endif

#if Win32
#define snprintf _snprintf
#endif

static char version[250] = {0};

LibAPI(const char *) lvzlib_zlibVersion(void)
{
	if (!version[0])
	{
		snprintf(version, sizeof(version), ABOUT_PRODUCTTITLE "\n"
		                                   "zlib version: %s, build flags: 0x%lX\n"
										   "minizip version: 1.2.0, September 16th, 2017\n"
										   "aes version: 2013\n"
#ifdef HAVE_BZIP2
										   "bzip2 version: %s\n"
#endif
                                           , lvzip_zlibVersion(), lvzip_zlibCompileFlags()
#ifdef HAVE_BZIP2
                                           , BZ2_bzlibVersion()
#endif
                                           );
	}
	return version;
}


LibAPI(uInt32) lvzlib_isLittleEndian(void)
{
#if BigEndian
	return LV_FALSE;
#else
	return LV_TRUE;
#endif
}


/* exported zlib deflate and inflate functions */
LibAPI(int) lvzlib_compress(Bytef *dest, uInt32 *destLen,
                             const Bytef *source, uInt32 sourceLen, int level)
{
	return compress2(dest, (uLong*)destLen, source, (uLong)sourceLen, level);
}

LibAPI(int) lvzlib_uncompress(Bytef *dest, uInt32 *destLen,
                             const Bytef *source, uInt32 sourceLen)
{
	return uncompress(dest, (uLong*)destLen, source, (uLong)sourceLen);
}

LibAPI(uInt32) lvzlib_crc32(uInt32 crc, const Bytef *buf, uInt32 len)
{
	return crc32(crc, buf, len);
}

LibAPI(uInt32) lvzlib_cryptrand(Bytef *buf, uInt32 size)
{
	return cryptrand(buf, size);
}

/* exported zip functions */

/****************************************************************************************************
 *
 * Opens or creates a ZIP archive for writing
 *
 * Parameters:
 *  pathname: The path and filename of the archive to open for a disk based archive file
 *  append: 0 - create new, 1 - append to end of file, 2 - add to archive
 *  globalcomment: 
 *  filefuncs: A pointer to a record containing the function pointers to use for access to the archive 
 *  refnum: An archive file reference
 *
 ****************************************************************************************************/
static MgErr lvzlib_zipOpen(const LWPathHandle pathName, int append, LStrHandle *globalcomment,
                            zlib_filefunc64_def* filefuncs, LVRefNum *refnum)
{
	MgErr err = fNotFound;
	const char *comment;
	zipFile node = zipOpen3_64(LWPathBuf(pathName), append, 0, &comment, filefuncs);
	if (*globalcomment)
		LStrLen(**globalcomment) = 0;
	*refnum = kNotARefNum;
	if (node)
	{
		err = lvzlibCreateRefnum(node, refnum, ZipMagic, LV_FALSE);
		if (!err && comment)
		{
			int32 len = (int32)StrLen((ConstCStr)comment);
			if (len > 0)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)globalcomment, len);
				if (!err)
				{
					MoveBlock(comment, LStrBuf(**globalcomment), len);
					LStrLen(**globalcomment) = len;
				}
				else
				{
					lvzlibDisposeRefnum(refnum, NULL, ZipMagic);		
				}
			}
		}
		if (err)
		{
			zipClose(node, NULL);
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_zipOpenLW(LWPathHandle *pathName, int append, LStrHandle *globalcomment, LVRefNum *refnum)
{
	MgErr err = LWPathZeroTerminate(pathName, -1);
	if (!err)
	{
		zlib_filefunc64_def filefuncs;
#if WIN32
		fill_win32_filefunc64W(&filefuncs);
#else
		fill_fopen64_filefunc(&filefuncs);
#endif
		err = lvzlib_zipOpen(*pathName, append, globalcomment, &filefuncs, refnum);
	}
	return err;
}

LibAPI(MgErr) lvzlib_zipOpenS(LStrHandle *memory, int append, LStrHandle *globalcomment, LVRefNum *refnum)
{
	zlib_filefunc64_def filefuncs;
	fill_mem_filefunc(&filefuncs, memory);
	return lvzlib_zipOpen(NULL, append, globalcomment, &filefuncs, refnum);
}

/****************************************************************************************************
 *
 * Adds a new archive entry to the archive and opens it to add data to it
 *
 * Parameters:
 *  refnum: An archive file reference
 *  filename: The filename to use for this new entry
 *  zipfi: fileinfo structure with settings for this entry
 *  extrafield_local:
 *  extrafield_global:
 *  comment: 
 *  method:
 *  level:
 *  raw:
 *  windowBits:
 *  memLevel:
 *  strategy:
 *  password:
 *  crcForCrypting:
 *  flags:
 *  zip64:
 *  aes:
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_zipOpenNewFileInZip(LVRefNum *refnum, LStrHandle filename, const zip_fileinfo* zipfi,
						   const LStrHandle extrafield_local, const LStrHandle extrafield_global, LStrHandle comment, 
						   int method, int level, int raw, int windowBits, int memLevel, int strategy, const char* password,
						   uInt32 crcForCrypting, uInt32 flags, int zip64, int aes)
{
	zipFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, ZipMagic);
	Unused(crcForCrypting);
	if (!err)
	{
		err = ZeroTerminateLString(&filename);
		if (!err)
		{
			err = ZeroTerminateLString(&comment);
			if (!err)
			{
				err = LibToMgErr(zipOpenNewFileInZip5(node, (const char*)LStrBufH(filename), zipfi,
						         LStrBufH(extrafield_local), (uint16_t)LStrLenH(extrafield_local),
								 LStrBufH(extrafield_global), (uint16_t)LStrLenH(extrafield_global),
								 (const char*)LStrBufH(comment), (uint16_t)flags, zip64, (uint16_t)method, level, raw,
								 windowBits, memLevel, strategy, password[0] ? password : NULL, aes));
			}
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_zipWriteInFileInZip(LVRefNum *refnum, const LStrHandle buffer)
{
	MgErr err = mgNoErr;
	int32 len = LStrLenH(buffer);
	if (len > 0)
	{
		zipFile node;
		err = lvzlibGetRefnum(refnum, &node, ZipMagic);
		if (!err)
		{
			int retval = zipWriteInFileInZip(node, LStrBuf(*buffer), len);
			if (retval < 0)
				return LibToMgErr(retval);
		}
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

/****************************************************************************************************
 *
 * Closes a ZIP archive that was opened for writing and finalize it
 *
 * Parameters:
 *  refnum: An archive file reference
 *  globalComment:
 *  stream: A handle to return the compresses zip archive for memory streams
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_zipClose(LVRefNum *refnum, const char *globalComment, LStrHandle *stream)
{
	zipFile node;
	MgErr err = lvzlibDisposeRefnum(refnum, &node, ZipMagic);
	if (!err)
	{
        *refnum = kNotARefNum;
		err = LibToMgErr(zipClose2(node, globalComment, VERSIONMADEBY, (voidpf*)stream));
	}
	return err;
}

/* exported unzip functions */

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
static MgErr lvzlib_unzOpen(const LWPathHandle pathName, zlib_filefunc64_def* filefuncs, LVRefNum *refnum)
{
	MgErr err = fNotFound;
	unzFile node = unzOpen2_64(LWPathBuf(pathName), filefuncs);
	*refnum = kNotARefNum;
	if (node)
	{
		err = lvzlibCreateRefnum(node, refnum, UnzMagic, LV_FALSE);
		if (err)
		{
			unzClose(node);
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzOpenLW(LWPathHandle *pathName, LVRefNum *refnum)
{
	MgErr err = LWPathZeroTerminate(pathName, -1);
	if (!err)
	{
		zlib_filefunc64_def filefuncs;
#if WIN32
		fill_win32_filefunc64W(&filefuncs);
#else
		fill_fopen64_filefunc(&filefuncs);
#endif
		err = lvzlib_unzOpen(*pathName, &filefuncs, refnum);
	}
	return err;
}
LibAPI(MgErr) lvzlib_unzOpenS(LStrHandle *memory, LVRefNum *refnum)
{
	zlib_filefunc64_def filefuncs;
	fill_mem_filefunc(&filefuncs, memory); 
	return lvzlib_unzOpen(NULL, &filefuncs, refnum);
}

/****************************************************************************************************
 *
 * Closes a ZIP archive that was opened for reading
 *
 * Parameters:
 *  refnum: An archive extraction file reference
 *
 ****************************************************************************************************/
LibAPI(MgErr) lvzlib_unzClose(LVRefNum *refnum, LStrHandle *stream)
{
	unzFile node;
	MgErr err = lvzlibDisposeRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		*refnum = kNotARefNum;
		err = LibToMgErr(unzClose2(node, (voidpf*)stream));
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
			if (pglobal_info.size_comment)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)comment, pglobal_info.size_comment + 1);
				if (!err)
				{
					err = LibToMgErr(unzGetGlobalComment(node, (char*)LStrBuf(**comment), pglobal_info.size_comment + 1));
				}
			}
			if (*comment)
			{
				LStrLen(**comment) = pglobal_info.size_comment;
			}
			if (!err)
			{
				*nEntry = pglobal_info.number_entry;
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
			if (pglobal_info.size_comment)
			{
				err = NumericArrayResize(uB, 1, (UHandle*)comment, pglobal_info.size_comment);
				if (!err)
				{
					err = LibToMgErr(unzGetGlobalComment(node, (char*)LStrBuf(**comment), pglobal_info.size_comment));
				}
			}
			if (*comment)
			{
				LStrLen(**comment) = pglobal_info.size_comment;
			}
			if (!err)
			{
				*nEntry = pglobal_info.number_entry;
			}
		}
	}
	return err;
}

static MgErr RetrieveFileInfo(unzFile node, unz_file_info *pfile_info, LStrHandle *fileName, uint16_t sizeFileName, LStrHandle *extraField, uint16_t sizeExtraField, LStrHandle *comment, uint16_t sizeComment)
{
	MgErr err = NumericArrayResize(uB, 1, (UHandle*)extraField, sizeExtraField + 1);
	if (!err)
	{
		err = NumericArrayResize(uB, 1, (UHandle*)fileName, sizeFileName + 1);
		if (!err)
		{
			err = NumericArrayResize(uB, 1, (UHandle*)comment, sizeComment + 1);
			if (!err)
			{
				err = LibToMgErr(unzGetCurrentFileInfo(node, pfile_info, (char*)LStrBufH(*fileName), sizeFileName + 1, LStrBufH(*extraField), sizeExtraField + 1, (char*)LStrBufH(*comment), sizeComment + 1));
				if (!err)
				{
					if (*extraField)
						LStrLen(**extraField) = sizeExtraField;
					if (*fileName)
						LStrLen(**fileName) = sizeFileName;
					if (*comment)
						LStrLen(**comment) = sizeComment;
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
 *                 0 - platform specific
 *                 1 - case sensitive
 *                 2 - case insensitive
 *
 ****************************************************************************************************/
static int caseInsensitiveNameComparer(unzFile file, const char *filename1, const char *filename2)
{
	Unused(file);
	return StrNCaseCmp((ConstCStr)filename1, (ConstCStr)filename2, StrLen((ConstCStr)filename1));
}

static int caseSensitiveNameComparer(unzFile file, const char *filename1, const char *filename2)
{
	Unused(file);
	return StrCmp((ConstCStr)filename1, (ConstCStr)filename2);
}

LibAPI(MgErr) lvzlib_unzLocateFile(LVRefNum *refnum, LStrHandle fileName, int iCaseSensitivity)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = ZeroTerminateLString(&fileName);
		if (!err)
		{
			unzFileNameComparer filename_compare_func = NULL;
			if (
#if Mac || MSWin || VxWorks
			    !iCaseSensitivity || 
#endif
				                     iCaseSensitivity == 2)
			{
				filename_compare_func = caseInsensitiveNameComparer;
			}
			else
			{
				filename_compare_func = caseSensitiveNameComparer;
			}
			err = LibToMgErr(unzLocateFile(node, (const char*)LStrBufH(fileName), filename_compare_func));
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzLocateFile2_64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment, int iCaseSensitivity)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = ZeroTerminateLString(fileName);
		if (!err)
		{
			unzFileNameComparer filename_compare_func = NULL;
			if (
#if Mac || MSWin || VxWorks
			    !iCaseSensitivity || 
#endif
				                     iCaseSensitivity == 2)
			{
				filename_compare_func = caseInsensitiveNameComparer;
			}
			else
			{
				filename_compare_func = caseSensitiveNameComparer;
			}
			err = LibToMgErr(unzLocateFile(node, (const char*)LStrBufH(*fileName), filename_compare_func));
			if (!err)
			{
				err = LibToMgErr(unzGetCurrentFileInfo64(node, pfile_info, NULL, 0, NULL, 0, NULL, 0));
				if (!err)
				{
					err = RetrieveFileInfo(node, NULL, fileName, pfile_info->size_filename, extraField, pfile_info->size_file_extra, comment, pfile_info->size_file_comment);
				}
			}
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
			err = RetrieveFileInfo(node, NULL, fileName, pfile_info->size_filename, extraField, pfile_info->size_file_extra, comment, pfile_info->size_file_comment);
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
			err = RetrieveFileInfo(node, NULL, fileName, pfile_info->size_filename, extraField, pfile_info->size_file_extra, comment, pfile_info->size_file_comment);
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
		err = LibToMgErr(unzOpenCurrentFile3(node, method, level, raw, password && password[0] ? password : NULL));
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
				LStrLen(**extra) = unzGetLocalExtrafield(node, LStrBuf(**extra), len);
			}
		}
		else
		{
			err = LibToMgErr(len);
		}
		if (*extra)
			LStrLen(**extra) = 0;
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
	if (!err && buffer)
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

LibAPI(MgErr) lvzlib_unzGoToFirstFile2_32(LVRefNum *refnum, unz_file_info *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		unz_file_info64 file_info64;
		err = LibToMgErr(unzGoToFirstFile2(node, &file_info64, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			err = RetrieveFileInfo(node, pfile_info, fileName, file_info64.size_filename, extraField, file_info64.size_file_extra, comment, file_info64.size_file_comment);
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGoToFirstFile2_64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGoToFirstFile2(node, pfile_info, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			err = RetrieveFileInfo(node, NULL, fileName, pfile_info->size_filename, extraField, pfile_info->size_file_extra, comment, pfile_info->size_file_comment);
		}
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

LibAPI(MgErr) lvzlib_unzGoToNextFile2_32(LVRefNum *refnum, unz_file_info *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		unz_file_info64 file_info64;
		err = LibToMgErr(unzGoToNextFile2(node, &file_info64, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			err = RetrieveFileInfo(node, pfile_info, fileName, file_info64.size_filename, extraField, file_info64.size_file_extra, comment, file_info64.size_file_comment);
		}
	}
	return err;
}

LibAPI(MgErr) lvzlib_unzGoToNextFile2_64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment)
{
	unzFile node;
	MgErr err = lvzlibGetRefnum(refnum, &node, UnzMagic);
	if (!err)
	{
		err = LibToMgErr(unzGoToNextFile2(node, pfile_info, NULL, 0, NULL, 0, NULL, 0));
		if (!err)
		{
			err = RetrieveFileInfo(node, NULL, fileName, pfile_info->size_filename, extraField, pfile_info->size_file_extra, comment, pfile_info->size_file_comment);
		}
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

