/*
   lvapi.c -- LabVIEW interface for LabVIEW ZIP library

   Copyright (C) 2009-2024 Rolf Kalbermatter

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
#include "zlib.h"
#include "zip.h"
#include "unzip.h"

#ifndef lvzip_lvapi_h
#define lvzip_lvapi_h
LibAPI(const char *) lvzlib_zlibVersion(void);
LibAPI(uInt32) lvzlib_isLittleEndian(void);

/* exported zlib deflate and inflate functions */
LibAPI(int) lvzlib_compress(Bytef *dest, uInt32 *destLen, const Bytef *source, uInt32 sourceLen, int level);
LibAPI(int) lvzlib_compress2(Bytef *dest, uInt32 *destLen, const Bytef *source, uInt32 sourceLen, int level, int32 windowSize);
LibAPI(int) lvzlib_uncompress(Bytef *dest, uInt32 *destLen, const Bytef *source, uInt32 sourceLen);
LibAPI(int) lvzlib_uncompress2(Bytef *dest, uInt32 *destLen, const Bytef *source, uInt32 *sourceLen, int32 windowSize);
LibAPI(uInt32) lvzlib_crc32(uInt32 crc, const Bytef *buf, uInt32 len);
LibAPI(uInt32) lvzlib_cryptrand(Bytef *buf, uInt32 size);

/* exported zip functions */
LibAPI(MgErr) lvzlib_zipOpenLW(LWPathHandle *pathname, int append, uint64_t disk_size, LStrHandle *globalcomment, LVRefNum *refnum);
LibAPI(MgErr) lvzlib_zipOpenS(LStrHandle *memory, int append, uint64_t disk_size, LStrHandle *globalcomment, LVRefNum *refnum);
LibAPI(MgErr) lvzlib_zipOpenNewFileInZip(LVRefNum *refnum, LStrHandle filename, const zip_fileinfo* zipfi,
						   const LStrHandle extrafield_local, const LStrHandle extrafield_global, LStrHandle comment,
						   int method, int level, int raw, int windowBits, int memLevel, int strategy,
						   const char* password, uInt32 crcForCrypting, uInt32 flags, int zip64, int aes);
LibAPI(MgErr) lvzlib_zipWriteInFileInZip(LVRefNum *refnum, const LStrHandle buffer);
LibAPI(MgErr) lvzlib_zipCloseFileInZipRaw32(LVRefNum *refnum, uInt32 uncompressedSize, uInt32 crc32);
LibAPI(MgErr) lvzlib_zipCloseFileInZipRaw64(LVRefNum *refnum, uInt64 uncompressedSize, uInt32 crc32);
LibAPI(MgErr) lvzlib_zipClose(LVRefNum *refnum, const char *globalComment, LStrHandle *stream);

/* exported unzip functions */
LibAPI(MgErr) lvzlib_unzOpenLW(LWPathHandle *pathname, LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzOpenS(LStrHandle *memory, LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzClose(LVRefNum *refnum, LStrHandle *stream);
LibAPI(MgErr) lvzlib_unzGetGlobalInfo32(LVRefNum *refnum, LStrHandle *comment, uInt32 *nEntry);
LibAPI(MgErr) lvzlib_unzGetGlobalInfo64(LVRefNum *refnum, LStrHandle *comment, uInt64 *nEntry);
LibAPI(MgErr) lvzlib_unzLocateFile(LVRefNum *refnum, LStrHandle fileName, int iCaseSensitivity);
LibAPI(MgErr) lvzlib_unzLocateFile2_64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment, int iCaseSensitivity);
LibAPI(MgErr) lvzlib_unzGetCurrentFileInfo32(LVRefNum *refnum, unz_file_info *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment);
LibAPI(MgErr) lvzlib_unzGetCurrentFileInfo64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment);
LibAPI(MgErr) lvzlib_unzOpenCurrentFile(LVRefNum *refnum, int32* method, int32* level, int16 raw, const char* password);
LibAPI(MgErr) lvzlib_unzGetLocalExtrafield(LVRefNum *refnum, LStrHandle *extra);
LibAPI(MgErr) lvzlib_unzReadCurrentFile(LVRefNum *refnum, LStrHandle buffer);
LibAPI(MgErr) lvzlib_unzCloseCurrentFile(LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzGoToFirstFile(LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzGoToFirstFile2_32(LVRefNum *refnum, unz_file_info *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment);
LibAPI(MgErr) lvzlib_unzGoToFirstFile2_64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment);
LibAPI(MgErr) lvzlib_unzGoToNextFile(LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzGoToNextFile2_32(LVRefNum *refnum, unz_file_info *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment);
LibAPI(MgErr) lvzlib_unzGoToNextFile2_64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment);
LibAPI(MgErr) lvzlib_unzGoToNextFile3_64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment, uint64_t index);
LibAPI(MgErr) lvzlib_unzGoToFilePos32(LVRefNum *refnum, unz_file_pos *pos);
LibAPI(MgErr) lvzlib_unzGoToFilePos64(LVRefNum *refnum, unz64_file_pos *pos);
LibAPI(MgErr) lvzlib_unzGetFilePos32(LVRefNum *refnum, unz_file_pos *pos);
LibAPI(MgErr) lvzlib_unzGetFilePos64(LVRefNum *refnum, unz64_file_pos *pos);
#endif
