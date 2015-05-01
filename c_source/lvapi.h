#include "lvutil.h"
#include "zlib.h"
#include "zip.h"
#include "unzip.h"

#ifndef lvzip_lvapi_h
#define lvzip_lvapi_h
LibAPI(const char *) lvzlib_zlibVersion(void);
LibAPI(int) lvzlib_compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);
LibAPI(int) lvzlib_uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);
LibAPI(uInt32) lvzlib_crc32(uInt32 crc, const Bytef *buf, uInt32 len);
LibAPI(MgErr) lvzlib_zipOpen(const void *pathname, int append, LStrHandle *globalcomment, zlib_filefunc64_def* filefuncs, LVRefNum *refnum);
LibAPI(MgErr) lvzlib_zipOpenNewFileInZip(LVRefNum *refnum, LStrHandle filename, const zip_fileinfo* zipfi,
						   const LStrHandle extrafield_local, const LStrHandle extrafield_global,
						   LStrHandle comment, int method, int level, int raw, int windowBits,
						   int memLevel, int strategy, const char* password, uLong crcForCrypting, uLong flags, int zip64);
LibAPI(MgErr) lvzlib_zipWriteInFileInZip(LVRefNum *refnum, const LStrHandle buffer);
LibAPI(MgErr) lvzlib_zipCloseFileInZipRaw32(LVRefNum *refnum, uInt32 uncompressedSize, uInt32 crc32);
LibAPI(MgErr) lvzlib_zipCloseFileInZipRaw64(LVRefNum *refnum, uInt64 uncompressedSize, uInt32 crc32);
LibAPI(MgErr) lvzlib_zipClose(LVRefNum *refnum, const LStrHandle globalComment, LStrHandle *stream);
LibAPI(MgErr) lvzlib_unzOpen(const void *pathname, zlib_filefunc64_def* filefuncs, LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzClose(LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzGetGlobalInfo32(LVRefNum *refnum, LStrHandle *comment, uInt32 *nEntry);
LibAPI(MgErr) lvzlib_unzGetGlobalInfo64(LVRefNum *refnum, LStrHandle *comment, uInt64 *nEntry);
LibAPI(MgErr) lvzlib_unzLocateFile(LVRefNum *refnum, LStrHandle fileName, int iCaseSensitivity);
LibAPI(MgErr) lvzlib_unzGetCurrentFileInfo32(LVRefNum *refnum, unz_file_info *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment);
LibAPI(MgErr) lvzlib_unzGetCurrentFileInfo64(LVRefNum *refnum, unz_file_info64 *pfile_info, LStrHandle *fileName, LStrHandle *extraField, LStrHandle *comment);
LibAPI(MgErr) lvzlib_unzOpenCurrentFile(LVRefNum *refnum, int32* method, int32* level, int16 raw, const char* password);
LibAPI(MgErr) lvzlib_unzGetLocalExtrafield(LVRefNum *refnum, LStrHandle *extra);
LibAPI(MgErr) lvzlib_unzReadCurrentFile(LVRefNum *refnum, LStrHandle buffer);
LibAPI(MgErr) lvzlib_unzCloseCurrentFile(LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzGoToFirstFile(LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzGoToNextFile(LVRefNum *refnum);
LibAPI(MgErr) lvzlib_unzGoToFilePos32(LVRefNum *refnum, unz_file_pos *pos);
LibAPI(MgErr) lvzlib_unzGoToFilePos64(LVRefNum *refnum, unz64_file_pos *pos);
LibAPI(MgErr) lvzlib_unzGetFilePos32(LVRefNum *refnum, unz_file_pos *pos);
LibAPI(MgErr) lvzlib_unzGetFilePos64(LVRefNum *refnum, unz64_file_pos *pos);
#endif
